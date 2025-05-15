#include "mbed.h"
#include "pinos.h"
#include "Pipetadora.h"
using namespace std::chrono;
using namespace std::chrono_literals;
// configurações de tempo de pipeta
static DigitalOut* pipette;
static constexpr int TIME_PER_ML_MS = 1000; // ajuste conforme calibração
// ------------------------------------------------------------------
// Variáveis e objetos para Z
// ------------------------------------------------------------------
static DigitalIn* switchSelect;         // lê o switch Y↔Z
static DigitalIn* endMinZ;              // FDC Z inferior
static DigitalIn* endMaxZ;              // FDC Z superior
static volatile int32_t positionZ;      // contador de passos Z
static constexpr float PASSO_FUSO_Z = 1.0f;
static int zSeqIndex = 0;               // idx para sequência de bobinas Z
static volatile int lin_x0, lin_y0, lin_tx, lin_ty, lin_dx, lin_dy, lin_sx, lin_sy, lin_err;
static volatile int br_stepsX, br_stepsY;
static microseconds br_periodX, br_periodY;


// — Identificadores de eixos X e Y
enum MotorId { MotorX = 0, MotorY = 1, MotorCount };

// — Parâmetros de velocidade e aceleração (X e Y)
static constexpr microseconds PERIODO_INICIAL[MotorCount] = { 1000us, 800us };
static constexpr microseconds PERIODO_MINIMO  [MotorCount] = {  225us, 225us };
static constexpr microseconds REDUCAO_PERIODO [MotorCount] = {   25us,  25us };
static constexpr int          PASSOS_PARA_ACELERAR      = 25;
static constexpr float        PASSO_FUSO[MotorCount]    = { 0.5f, 0.5f };
static constexpr microseconds INTERP_PERIOD = PERIODO_INICIAL[MotorX] / 2;   // velocidade fixa de interpolação (ex: 1000 µs)
// — Pinos drivers (X, Y)
static constexpr PinName STEP_PIN   [MotorCount] = { MOTOR_X, MOTOR_Y };
static constexpr PinName DIR_PIN    [MotorCount] = { DIR_X,   DIR_Y   };
static constexpr PinName ENABLE_PIN [MotorCount] = { EN_X,    EN_Y    };
static constexpr PinName ENDMIN_PIN [MotorCount] = { FDC_XDWN,FDC_YDWN};
static constexpr PinName ENDMAX_PIN [MotorCount] = { FDC_XUP, FDC_YUP };
static constexpr PinName BTN_UP_PIN [MotorCount] = { BTN_XUP, BTN_YUP };
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
static microseconds        periodCur   [MotorCount];
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
static constexpr auto     VEL_STEP_MS_Z = 3ms; // ms entre passos Z (chrono)
static constexpr uint8_t  SEQ_Z[4]      = { 0b0001, 0b0010, 0b0100, 0b1000 };
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
        passoCount [i] = 0;
        dirState   [i] = 0;
    }
    coilsZ = 0; // garante Z desligado
    switchSelect = new DigitalIn(SWITCH_PIN, PullDown);
    endMinZ      = new DigitalIn(FDC_ZDWN,  PullDown);
    endMaxZ      = new DigitalIn(FDC_ZUP,   PullDown);
    positionZ    = 0;

    // inicializa pipeta
    pipette = new DigitalOut(PIPETA);
    pipette->write(0);

}

void Pipetadora_Homing(void) {
    homingZ();
    HomingXY();
}
static void stepZForward() {
    if (endMaxZ->read()) { coilsZ = 0; return; }
    coilsZ = SEQ_Z[zSeqIndex];
    zSeqIndex = (zSeqIndex + 1) % 4;
    positionZ++;
}

static void stepZBackward() {
    if (endMinZ->read()) { coilsZ = 0; return; }
    zSeqIndex = (zSeqIndex + 3) % 4;
    coilsZ = SEQ_Z[zSeqIndex];
    positionZ--;
}
void Pipetadora_ManualControl(void) {
    // Lê o switch físico (0 → controlar Y; 1 → controlar Z)
    bool sw = switchSelect->read();

    // Movimento manual do eixo X (inalterado)
    {
        bool upX = btnUp[MotorX]->read();
        bool dnX = btnDwn[MotorX]->read();
        if      (upX && !dnX) { 
            if (!tickerOn[MotorX] || dirState[MotorX] != 0) 
                Mover_Frente(MotorX); 
        }
        else if (dnX && !upX) { 
            if (!tickerOn[MotorX] || dirState[MotorX] != 1) 
                Mover_Tras(MotorX);
        }
        else { 
            if (tickerOn[MotorX]) 
                Parar_Mov(MotorX);
        }
    }

    // Movimento manual do eixo Y ou Z, dependendo do estado do switch
    {
        bool upY = btnUp[MotorY]->read();
        bool dnY = btnDwn[MotorY]->read();

        if (!sw) {
            // Modo Y normal
            if      (upY && !dnY) { 
                if (!tickerOn[MotorY] || dirState[MotorY] != 0) 
                    Mover_Frente(MotorY); 
            }
            else if (dnY && !upY) { 
                if (!tickerOn[MotorY] || dirState[MotorY] != 1) 
                    Mover_Tras(MotorY);
            }
            else { 
                if (tickerOn[MotorY]) 
                    Parar_Mov(MotorY);
            }
        } else {
            // Modo Z manual (usa mesmos botões Up/Down de Y)
            if      (upY && !dnY) {
                stepZForward();
            }
            else if (dnY && !upY) {
                stepZBackward();
            }
            else {
                // Nenhum botão; desliga bobinas Z
                coilsZ = 0;
            }
        }
    }

    // Pequeno delay para suavizar;
    ThisThread::sleep_for(VEL_STEP_MS_Z);
}


float Pipetadora_GetPositionCm(int id) {
    if (id < MotorCount)
        return (float)(position[id] * PASSO_FUSO[id] / 400.0);
    else
        return (float)(positionZ * PASSO_FUSO_Z    / 400.0);
}

int Pipetadora_GetPositionSteps(int id) {
    if (id < MotorCount)
        return position[id];
    else
        return positionZ;
}


// — Homing paralelo X e Y —
static void HomingXY(void) {
    stopTicker(MotorX);
    stopTicker(MotorY);
    Mover_Frente(MotorX);
    Mover_Tras(MotorY);
    while (tickerOn[MotorX] || tickerOn[MotorY]) {
        if (tickerOn[MotorX] && endMax[MotorX]->read()) Parar_Mov(MotorX);
        if (tickerOn[MotorY] && endMin [MotorY]->read()) Parar_Mov(MotorY);
        ThisThread::sleep_for(1ms);
    }
    position[MotorX] = 0;
    position[MotorY] = 0;
}

// — Homing específico do Z (igual a X/Y, usa FDC_ZDWN e FDC_ZUP) —
static void homingZ(void) {
    // Desliga bobinas
    coilsZ = 0;

    // Sobe até acionar o fim de curso superior (ZUP)
    while (!endMaxZ->read()) {
        stepZForward();
        ThisThread::sleep_for(VEL_STEP_MS_Z);
    }

    // Ao atingir ZUP, zera posição e marca este ponto como zero
    coilsZ = 0;
    positionZ = 0;  // home no topo → posição = 0
}

// — Implementações internas — 
static void startTicker(int id) {
    if (!tickerOn[id]) {
        tickers[id]->attach(stepWrapper[id], periodCur[id]);
        tickerOn[id] = true;
        enableOut[id]->write(0);
    }
}

static void stopTicker(int id) {
    if (tickerOn[id]) {
        tickers[id]->detach();
        tickerOn[id] = false;
        enableOut[id]->write(1);
    }
}

static void stepISR(int id) {
    if ((dirState[id] == 0 && endMax[id]->read()) ||
        (dirState[id] == 1 && endMin[id]->read())) {
        stopTicker(id);
        return;
    }
    bool st = !stepOut[id]->read();
    stepOut[id]->write(st);
    if (st) {
        int delta = (dirState[id] == 0 ? +1 : -1);
        position[id] += delta * 2;
    }
    if (++passoCount[id] >= PASSOS_PARA_ACELERAR) {
        passoCount[id] = 0;
        if (periodCur[id] > PERIODO_MINIMO[id]) {
            periodCur[id] -= REDUCAO_PERIODO[id];
            if (periodCur[id] < PERIODO_MINIMO[id]) periodCur[id] = PERIODO_MINIMO[id];
            tickers[id]->detach();
            tickers[id]->attach(stepWrapper[id], periodCur[id]);
            tickerOn[id] = true;
        }
    }
}

static void Mover_Frente(int id) {
    if (endMax[id]->read()) return;
    dirState[id] = 0;
    dirOut[id]->write(0);
    enableOut[id]->write(0);
    periodCur[id] = PERIODO_INICIAL[id];
    passoCount[id] = 0;
    startTicker(id);
}

static void Mover_Tras(int id) {
    if (endMin[id]->read()) return;
    dirState[id] = 1;
    dirOut[id]->write(1);
    enableOut[id]->write(0);
    periodCur[id] = PERIODO_INICIAL[id];
    passoCount[id] = 0;
    startTicker(id);
}

static void Parar_Mov(int id) {
    stopTicker(id);
}
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
        lin_err    -= lin_dy;
    }
    tickers[MotorX]->detach();
    tickers[MotorX]->attach(stepLinearX_wrapper, INTERP_PERIOD);
}

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
        lin_err    += lin_dx;
    }
    tickers[MotorY]->detach();
    tickers[MotorY]->attach(stepLinearY_wrapper, INTERP_PERIOD);
}

// precisa desses wrappers por causa da assinatura void func()
static void stepLinearX_wrapper() { stepLinearX(); }
static void stepLinearY_wrapper() { stepLinearY(); }
// Move o eixo especificado (0=X,1=Y,2=Z) até targetSteps passos (bloqueante)
extern "C" void Pipetadora_MoveTo(int id, int targetSteps) {
    if (id < MotorCount) {
        int32_t current = position[id];
        int32_t delta   = targetSteps - current;
        if (delta > 0) {
            Mover_Frente(id);
            while (position[id] < targetSteps) ThisThread::sleep_for(1ms);
            Parar_Mov(id);
        } else if (delta < 0) {
            Mover_Tras(id);
            while (position[id] > targetSteps) ThisThread::sleep_for(1ms);
            Parar_Mov(id);
        }
    } else if (id == MotorCount) {
        // ----------------------------------------------------------
        // Z axis movement with endstop detection to avoid lock-up
        // ----------------------------------------------------------
        int32_t delta = targetSteps - positionZ;
        if (delta > 0) {
            // descida do Z (positionZ incrementa)
            while (positionZ < targetSteps) {
                // se bateu no fim-de-curso superior, marca home e sai
                if (endMaxZ->read()) {
                    positionZ = 0;
                    break;
                }
                stepZForward();
                ThisThread::sleep_for(VEL_STEP_MS_Z);
            }
        } else if (delta < 0) {
            // subida do Z (positionZ decrementa)
            while (positionZ > targetSteps) {
                // se bateu no fim-de-curso inferior, sai
                if (endMinZ->read()) {
                    break;
                }
                stepZBackward();
                ThisThread::sleep_for(VEL_STEP_MS_Z);
            }
        }
    }
}
// Adicione no fim de Pipetadora.cpp, antes do “// Aciona válvula”:

// Caminha simultaneamente X e Y em linha reta até (tx,ty)
extern "C" void Pipetadora_MoveLinear(int tx, int ty) {
    // inicializa Bresenham
    lin_x0 = position[MotorX];  lin_y0 = position[MotorY];
    lin_tx = tx;                lin_ty = ty;
    lin_dx = abs(lin_tx - lin_x0);  lin_dy = abs(lin_ty - lin_y0);
    lin_sx = (lin_tx > lin_x0 ? 1 : -1); lin_sy = (lin_ty > lin_y0 ? 1 : -1);
    lin_err = lin_dx - lin_dy;

    // drivers e direção
    enableOut[MotorX]->write(0);
    enableOut[MotorY]->write(0);
    dirOut[MotorX]->write(lin_sx < 0);
    dirOut[MotorY]->write(lin_sy < 0);

    // arranque dos tickers com período fixo
    tickers[MotorX]->detach();
    tickers[MotorY]->detach();
    tickerOn[MotorX] = tickerOn[MotorY] = true;
    tickers[MotorX]->attach(stepLinearX_wrapper, INTERP_PERIOD);
    tickers[MotorY]->attach(stepLinearY_wrapper, INTERP_PERIOD);

    // espera ambos terminarem
    while (tickerOn[MotorX] || tickerOn[MotorY]) {
        ThisThread::sleep_for(1ms);
    }

    // desliga drivers
    enableOut[MotorX]->write(1);
    enableOut[MotorY]->write(1);
}



// Aciona válvula por volume_ml mL
extern "C" void Pipetadora_ActuateValve(int volume_ml) {
    pipette->write(1);
    ThisThread::sleep_for(std::chrono::milliseconds(volume_ml * TIME_PER_ML_MS));
    pipette->write(0);
}

// Para imediatamente todos os movimentos e desativa bobinas (emergência)
extern "C" void Pipetadora_StopAll(void) {
    // paralisa X e Y
    Parar_Mov(MotorX);
    Parar_Mov(MotorY);
    // desliga bobinas Z
    coilsZ = 0;
    // garante válvula fechada
    pipette->write(0);
}