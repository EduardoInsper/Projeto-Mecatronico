#include "mbed.h"
#include "pinos.h"
#include "Pipetadora.h"

// emergência interna
static DigitalIn emergPin(EMER_2, PullUp);

// botões de seleção de velocidade (VELO1/VELO2/VELO3)
static DigitalIn velo1Pin(BTN_VELO1, PullDown);
static DigitalIn velo2Pin(BTN_VELO2, PullDown);
static DigitalIn velo3Pin(BTN_VELO3, PullDown);

using namespace std::chrono;
using namespace std::chrono_literals;

// configurações de tempo de pipeta
static DigitalOut* pipette;
static constexpr int TIME_PER_ML_MS = 50; // ajuste conforme calibração

// ------------------------------------------------------------------
// Variáveis e objetos para Z
// ------------------------------------------------------------------
static DigitalIn* switchSelect;    // lê o switch Y↔Z
static DigitalIn* endMinZ;         // fim-de-curso Z inferior
static DigitalIn* endMaxZ;         // fim-de-curso Z superior
static volatile int32_t positionZ; // contador de passos Z
static constexpr float PASSO_FUSO_Z = 1.0f;
static int zSeqIndex = 0;          // índice de sequência de bobinas Z

// velocidades Z (milissegundos)
static constexpr milliseconds VEL_STEP_MS_Z_HIGH   = 3ms;
static constexpr milliseconds VEL_STEP_MS_Z_MEDIUM = 4ms;
static constexpr milliseconds VEL_STEP_MS_Z_LOW    = 5ms;
static milliseconds velStepMsZCurrent = VEL_STEP_MS_Z_HIGH;

// ------------------------------------------------------------------
// Algoritmo de Bresenham (movimento linear XY)
// ------------------------------------------------------------------
static volatile int lin_x0, lin_y0, lin_tx, lin_ty, lin_dx, lin_dy, lin_sx, lin_sy, lin_err;

// — Identificadores de eixos X e Y
enum MotorId { MotorX = 0, MotorY = 1, MotorCount };

// — Estado de toggle Y↔Z
static bool swMode = false;
static bool prevSwRaw = false;

// — Parâmetros de velocidade/ aceleração X e Y
static constexpr microseconds PERIODO_INICIAL      [MotorCount] = { 1000us, 800us };
static constexpr microseconds PERIODO_MINIMO_FAST  [MotorCount] = { 175us, 200us };
static constexpr microseconds PERIODO_MINIMO_MED   [MotorCount] = { 300us, 350us };
static constexpr microseconds PERIODO_MINIMO_SLOW  [MotorCount] = { 700us, 700us };
static microseconds periodoMinAtual[MotorCount];

static microseconds periodCur    [MotorCount];
static constexpr microseconds REDUCAO_PERIODO    [MotorCount] = { 25us, 25us };
static constexpr int          PASSOS_PARA_ACELERAR        = 25;
static constexpr float        PASSO_FUSO[MotorCount]      = { 0.5f, 0.5f };
static constexpr microseconds INTERP_PERIOD             = PERIODO_INICIAL[MotorX] / 3;

// — Pinos drivers (X, Y)
static constexpr PinName STEP_PIN   [MotorCount] = { MOTOR_X, MOTOR_Y };
static constexpr PinName DIR_PIN    [MotorCount] = { DIR_X,   DIR_Y   };
static constexpr PinName ENABLE_PIN [MotorCount] = { EN_X,    EN_Y    };
static constexpr PinName ENDMIN_PIN [MotorCount] = { FDC_XDWN, FDC_YDWN };
static constexpr PinName ENDMAX_PIN [MotorCount] = { FDC_XUP,  FDC_YUP  };
static constexpr PinName BTN_UP_PIN [MotorCount] = { BTN_XUP,  BTN_YUP  };
static constexpr PinName BTN_DWN_PIN[MotorCount] = { BTN_XDWN, BTN_YDWN };

// — Estado e hardware X/Y
static DigitalOut* stepOut   [MotorCount];
static DigitalOut* dirOut    [MotorCount];
static DigitalOut* enableOut [MotorCount];
static DigitalIn*  endMin    [MotorCount];
static DigitalIn*  endMax    [MotorCount];
static DigitalIn*  btnUp     [MotorCount];
static DigitalIn*  btnDwn    [MotorCount];
static Ticker*     tickers   [MotorCount];
static volatile bool       tickerOn    [MotorCount];
static volatile int32_t    position    [MotorCount];
static int                 passoCount  [MotorCount];
static int                 dirState    [MotorCount]; // 0 → frente, 1 → trás

// — Wrappers para Ticker X/Y
static void stepISR(int id);
static void stepISR0() { stepISR(0); }
static void stepISR1() { stepISR(1); }
static void (* const stepWrapper[MotorCount])() = { stepISR0, stepISR1 };
static void stepLinearX_wrapper();
static void stepLinearY_wrapper();

// — Controle direto do Z
static constexpr uint8_t SEQ_Z[4] = { 0b0001,0b0010,0b0100,0b1000 };
static BusOut coilsZ(Z_A1, Z_A2, Z_B1, Z_B2);

// — Protótipos internos
static void startTicker(int id);
static void stopTicker (int id);
static void Mover_Frente(int id);
static void Mover_Tras  (int id);
static void Parar_Mov   (int id);
static void HomingXY    (void);
static void homingZ     (void);

// — API pública —
void Pipetadora_InitMotors(void) {
    for (int i = 0; i < MotorCount; ++i) {
        stepOut   [i] = new DigitalOut(STEP_PIN   [i], 0);
        dirOut    [i] = new DigitalOut(DIR_PIN    [i], 0);
        enableOut [i] = new DigitalOut(ENABLE_PIN [i], 1);
        endMin    [i] = new DigitalIn (ENDMIN_PIN [i], PullDown);
        endMax    [i] = new DigitalIn (ENDMAX_PIN [i], PullDown);
        btnUp     [i] = new DigitalIn (BTN_UP_PIN  [i], PullDown);
        btnDwn    [i] = new DigitalIn (BTN_DWN_PIN [i], PullDown);
        tickers   [i] = new Ticker();

        position   [i] = 0;
        tickerOn   [i] = false;
        periodCur  [i] = PERIODO_INICIAL[i];
        periodoMinAtual[i] = PERIODO_MINIMO_FAST[i];  // manual inicial: rápida
        passoCount [i] = 0;
        dirState   [i] = 0;
    }
    coilsZ = 0;
    switchSelect = new DigitalIn(SWITCH_PIN, PullDown);
    endMinZ      = new DigitalIn(FDC_ZDWN,   PullDown);
    endMaxZ      = new DigitalIn(FDC_ZUP,    PullDown);
    positionZ    = 0;

    pipette = new DigitalOut(PIPETA);
    pipette->write(0);
}

//Chama ambas as funções de referenciamento de eixo
void Pipetadora_Homing(void) {
    homingZ();
    HomingXY();
}

//Aciona motor de passo no sentido horario
static void stepZForward() {
    if (endMaxZ->read()) { coilsZ = 0; return; }
    coilsZ = SEQ_Z[zSeqIndex];
    zSeqIndex = (zSeqIndex + 1) % 4;
    positionZ++;
}

//Aciona motor de passo no sentido antihorario
static void stepZBackward() {
    if (endMinZ->read()) { coilsZ = 0; return; }
    zSeqIndex = (zSeqIndex + 3) % 4;
    coilsZ = SEQ_Z[zSeqIndex];
    positionZ--;
}

//Definições e funções do jog manual da pipetadora
void Pipetadora_ManualControl(void) {
    // 1) Toggle Y↔Z
    bool raw = switchSelect->read();
    if (raw && !prevSwRaw) {
        swMode = !swMode;
        Parar_Mov(MotorX);
        Parar_Mov(MotorY);
        coilsZ = 0;
    }
    prevSwRaw = raw;

    // 2) Ajuste de velocidade X/Y (manual)
    if      (velo1Pin.read()) {
        for (int i = 0; i < MotorCount; ++i) periodoMinAtual[i] = PERIODO_MINIMO_SLOW[i];
    }
    else if (velo2Pin.read()) {
        for (int i = 0; i < MotorCount; ++i) periodoMinAtual[i] = PERIODO_MINIMO_MED[i];
    }
    else {
        for (int i = 0; i < MotorCount; ++i) periodoMinAtual[i] = PERIODO_MINIMO_FAST[i];
    }

    // 3) Ajuste de velocidade Z (manual)
    if      (velo1Pin.read()) velStepMsZCurrent = VEL_STEP_MS_Z_LOW;
    else if (velo2Pin.read()) velStepMsZCurrent = VEL_STEP_MS_Z_MEDIUM;
    else                       velStepMsZCurrent = VEL_STEP_MS_Z_HIGH;

    // 4) Movimento manual do eixo X ou Z
    {
        bool upX = btnUp[MotorX]->read();
        bool dnX = btnDwn[MotorX]->read();
        if (!swMode) {
            if      (upX && !dnX) { if (!tickerOn[MotorX] || dirState[MotorX]!=0) Mover_Frente(MotorX); }
            else if (dnX && !upX) { if (!tickerOn[MotorX] || dirState[MotorX]!=1) Mover_Tras(MotorX); }
            else                  { if (tickerOn[MotorX]) Parar_Mov(MotorX); }
        } else {
            if      (upX && !dnX) stepZForward();
            else if (dnX && !upX) stepZBackward();
            else                  coilsZ = 0;
        }
    }

    // 5) Movimento manual do eixo Y
    {
        bool upY = btnUp[MotorY]->read();
        bool dnY = btnDwn[MotorY]->read();
        if      (upY && !dnY) { if (!tickerOn[MotorY] || dirState[MotorY]!=0) Mover_Frente(MotorY); }
        else if (dnY && !upY) { if (!tickerOn[MotorY] || dirState[MotorY]!=1) Mover_Tras(MotorY); }
        else                  { if (tickerOn[MotorY]) Parar_Mov(MotorY); }
    }

    // 6) Delay único para suavizar manual
    ThisThread::sleep_for(velStepMsZCurrent);
}

//Retorna o valor do modo de XY ou ZY
extern "C" bool Pipetadora_GetToggleMode(void) {
    return swMode;
}

//calcula a conversão de passo pra cm (não implementado)
float Pipetadora_GetPositionCm(int id) {
    if (id < MotorCount) return float(position[id] * PASSO_FUSO[id] / 400.0f);
    return float(positionZ * PASSO_FUSO_Z / 400.0f);
}

//Retorna posição absoluta em passos da pipetadora (para X e Y) 
int Pipetadora_GetPositionSteps(int id) {
    return (id < MotorCount ? position[id] : positionZ);
}

// — Homing paralelo X e Y —
static void HomingXY(void) {
    stopTicker(MotorX);
    stopTicker(MotorY);
    Mover_Frente(MotorX);
    Mover_Tras(MotorY);
    while (tickerOn[MotorX] || tickerOn[MotorY]) {
        if (!emergPin.read()) break;
        ThisThread::sleep_for(1ms);
    }
    position[MotorX] = position[MotorY] = 0;
    Parar_Mov(MotorX);
    Parar_Mov(MotorY);
}

// — Homing Z —
static void homingZ(void) {
    coilsZ = 0;
    while (!endMaxZ->read()) {
        if (!emergPin.read()) return;
        stepZForward();
        ThisThread::sleep_for(velStepMsZCurrent);
    }
    coilsZ = 0;
    positionZ = 0;
}

//Aciona ticked do motor de passo
static void startTicker(int id) {
    if (!tickerOn[id]) {
        tickers[id]->attach(stepWrapper[id], periodCur[id]);
        tickerOn[id] = true;
        enableOut[id]->write(0);
    }
}

//Desativa ticked do motor de passo
static void stopTicker(int id) {
    if (tickerOn[id]) {
        tickers[id]->detach();
        tickerOn[id] = false;
        enableOut[id]->write(1);
    }
}

//ISR dos steps para os motores X e Y
static void stepISR(int id) {
    // parar no fim de curso
    if ((dirState[id]==0 && endMax[id]->read()) ||
        (dirState[id]==1 && endMin[id]->read())) {
        stopTicker(id);
        return;
    }
    // gera pulso de step
    bool st = !stepOut[id]->read();
    stepOut[id]->write(st);
    if (st) {
        position[id] += (dirState[id]==0 ? +2 : -2);
    }
    // aceleração / desaceleração
    if (++passoCount[id] >= PASSOS_PARA_ACELERAR) {
        passoCount[id] = 0;
        if (periodCur[id] > periodoMinAtual[id]) {
            // acelerar
            periodCur[id] -= REDUCAO_PERIODO[id];
            if (periodCur[id] < periodoMinAtual[id]) {
                periodCur[id] = periodoMinAtual[id];
            }
        }
        else if (periodCur[id] < periodoMinAtual[id]) {
            // desacelerar
            periodCur[id] += REDUCAO_PERIODO[id];
            if (periodCur[id] > periodoMinAtual[id]) {
                periodCur[id] = periodoMinAtual[id];
            }
        }
        // atualiza ticker
        tickers[id]->detach();
        tickers[id]->attach(stepWrapper[id], periodCur[id]);
        tickerOn[id] = true;
    }
}

//Aciona motor de passo no sentido horario (X e Y)
static void Mover_Frente(int id) {
    if (endMax[id]->read()) return;
    dirState[id]   = 0;
    dirOut[id]->write(0);
    enableOut[id]->write(0);
    periodCur[id]  = PERIODO_INICIAL[id];
    passoCount[id] = 0;
    startTicker(id);
}
//Aciona motor de passo no sentido antihorario (X e Y)
static void Mover_Tras(int id) {
    if (endMin[id]->read()) return;
    dirState[id]   = 1;
    dirOut[id]->write(1);
    enableOut[id]->write(0);
    periodCur[id]  = PERIODO_INICIAL[id];
    passoCount[id] = 0;
    startTicker(id);
}

//Para movimentação de X e Y
static void Parar_Mov(int id) {
    stopTicker(id);
}

//Interpolação de Bresenham para a pipetagem automatica
extern "C" void Pipetadora_MoveLinear(int tx, int ty) {
    lin_x0 = position[MotorX]; lin_y0 = position[MotorY];
    lin_tx = tx;              lin_ty = ty;
    lin_dx = abs(lin_tx - lin_x0);
    lin_dy = abs(lin_ty - lin_y0);
    lin_sx = (lin_tx > lin_x0 ? 1 : -1);
    lin_sy = (lin_ty > lin_y0 ? 1 : -1);
    lin_err = lin_dx - lin_dy;

    enableOut[MotorX]->write(0);
    enableOut[MotorY]->write(0);
    dirOut[MotorX]->write(lin_sx < 0);
    dirOut[MotorY]->write(lin_sy < 0);

    tickers[MotorX]->detach();
    tickers[MotorY]->detach();
    tickerOn[MotorX] = tickerOn[MotorY] = true;
    tickers[MotorX]->attach(stepLinearX_wrapper, INTERP_PERIOD);
    tickers[MotorY]->attach(stepLinearY_wrapper, INTERP_PERIOD);

    while (tickerOn[MotorX] || tickerOn[MotorY]) {
        if (!emergPin.read()) break;
        ThisThread::sleep_for(1ms);
    }

    enableOut[MotorX]->write(1);
    enableOut[MotorY]->write(1);
}

//Definição do step em Bresenham para X 
static void stepLinearX() {
    if (lin_x0 == lin_tx && lin_y0 == lin_ty) {
        tickers[MotorX]->detach();
        tickerOn[MotorX] = false;
        enableOut[MotorX]->write(1);
        return;
    }
    int e2 = lin_err * 2;
    if (e2 > -lin_dy) {
        stepOut[MotorX]->write(!stepOut[MotorX]->read());
        lin_x0 += lin_sx;
        position[MotorX] = lin_x0;
        lin_err -= lin_dy;
    }
    tickers[MotorX]->detach();
    tickers[MotorX]->attach(stepLinearX_wrapper, INTERP_PERIOD);
}

//Definição do step em Bresenham para Y
static void stepLinearY() {
    if (lin_x0 == lin_tx && lin_y0 == lin_ty) {
        tickers[MotorY]->detach();
        tickerOn[MotorY] = false;
        enableOut[MotorY]->write(1);
        return;
    }
    int e2 = lin_err * 2;
    if (e2 < lin_dx) {
        stepOut[MotorY]->write(!stepOut[MotorY]->read());
        lin_y0 += lin_sy;
        position[MotorY] = lin_y0;
        lin_err += lin_dx;
    }
    tickers[MotorY]->detach();
    tickers[MotorY]->attach(stepLinearY_wrapper, INTERP_PERIOD);
}

static void stepLinearX_wrapper() { stepLinearX(); }
static void stepLinearY_wrapper() { stepLinearY(); }

//Move ambos os eixos da pipetadora para a posição dos pontos
extern "C" void Pipetadora_MoveTo(int id, int targetSteps) {
    if (id < MotorCount) {
        int32_t current = position[id];
        int32_t delta   = targetSteps - current;
        if (delta > 0) {
            Mover_Frente(id);
            while (position[id] < targetSteps) {
                if (!emergPin.read()) return;
                ThisThread::sleep_for(1ms);
            }
            Parar_Mov(id);
        } else if (delta < 0) {
            Mover_Tras(id);
            while (position[id] > targetSteps) {
                if (!emergPin.read()) return;
                ThisThread::sleep_for(1ms);
            }
            Parar_Mov(id);
        }
    } else {
        int32_t delta = targetSteps - positionZ;
        if (delta > 0) {
            while (positionZ < targetSteps) {
                if (!emergPin.read()) return;
                if (endMaxZ->read()) { positionZ = 0; break; }
                stepZForward();
                ThisThread::sleep_for(VEL_STEP_MS_Z_HIGH);
            }
        } else if (delta < 0) {
            while (positionZ > targetSteps) {
                if (!emergPin.read()) { coilsZ = 0; return; }
                if (endMinZ->read()) break;
                stepZBackward();
                ThisThread::sleep_for(VEL_STEP_MS_Z_HIGH);
            }
        }
        coilsZ = 0;
    }
}

//Ativação da pipeta
extern "C" void Pipetadora_ActuateValve(int volume_ml) {
    if (!emergPin.read()) return;
    pipette->write(0);
    ThisThread::sleep_for(50ms);
    pipette->write(1);
}

//Para todos os motores
extern "C" void Pipetadora_StopAll(void) {
    Parar_Mov(MotorX);
    Parar_Mov(MotorY);
    coilsZ = 0;
    pipette->write(0);
}
