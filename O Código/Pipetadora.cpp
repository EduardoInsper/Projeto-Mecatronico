#include "mbed.h"
#include "pinos.h"
#include "Pipetadora.h"

// emergência interna
static DigitalIn emergPin(EMER_2, PullUp);
using namespace std::chrono;
using namespace std::chrono_literals;

// configurações de tempo de pipeta
static DigitalOut* pipette;
static constexpr int TIME_PER_ML_MS = 50; // ajuste conforme calibração

// ------------------------------------------------------------------
// Variáveis e objetos para Z
// ------------------------------------------------------------------
static DigitalIn* switchSelect;         // lê o switch Y↔Z
static DigitalIn* endMinZ;              // FDC Z inferior
static DigitalIn* endMaxZ;              // FDC Z superior
static volatile int32_t positionZ;      // contador de passos Z
static constexpr float PASSO_FUSO_Z = 1.0f;
static int zSeqIndex = 0;               // idx para sequência de bobinas Z

// Velocidades Z com VELO1/2/3 (milissegundos)
static constexpr milliseconds VEL_STEP_MS_Z       = 2ms;  // alta
static constexpr milliseconds VEL_STEP_MS_Z_MEDIO = 4ms;  // média
static constexpr milliseconds VEL_STEP_MS_Z_BAIXO  = 6ms;  // baixa
static milliseconds velStepMsZCurrent = VEL_STEP_MS_Z;

// ------------------------------------------------------------------
// Algoritmo de Bresenham (movimento linear XY)
// ------------------------------------------------------------------
static volatile int lin_x0, lin_y0, lin_tx, lin_ty, lin_dx, lin_dy, lin_sx, lin_sy, lin_err;
static microseconds  br_periodX, br_periodY;

// — Identificadores de eixos X e Y
enum MotorId { MotorX = 0, MotorY = 1, MotorCount };

// — Estado de toggle para selecionar Y (false) ou Z (true)
static bool swMode = false;
static bool prevSwRaw = false;

// — Parâmetros de velocidade e aceleração (X e Y)
static constexpr microseconds PERIODO_INICIAL[MotorCount]      = { 1000us, 800us };
static constexpr microseconds PERIODO_MINIMO  [MotorCount]     = {  175us, 200us };
static constexpr microseconds PERIODO_MINIMO_MEDIO[MotorCount] = { 300us, 350us };
static constexpr microseconds PERIODO_MINIMO_BAIXO [MotorCount] = { 700us, 700us };
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
static constexpr PinName ENDMAX_PIN [MotorCount] = { FDC_XUP,  FDC_YUP };
static constexpr PinName BTN_UP_PIN [MotorCount] = { BTN_XUP,  BTN_YUP };
static constexpr PinName BTN_DWN_PIN[MotorCount] = { BTN_XDWN, BTN_YDWN };

// — Pinos de seleção de velocidade (VELO1, VELO2, VELO3)
static DigitalIn* velo1Pin;
static DigitalIn* velo2Pin;
static DigitalIn* velo3Pin;

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
static constexpr uint8_t SEQ_Z[4] = { 0b0001, 0b0010, 0b0100, 0b1000 };
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

        position        [i] = 0;
        tickerOn        [i] = false;
        periodCur       [i] = PERIODO_INICIAL[i];
        periodoMinAtual [i] = PERIODO_MINIMO[i];
        passoCount      [i] = 0;
        dirState        [i] = 0;
    }

    // Z
    coilsZ       = 0;
    switchSelect = new DigitalIn(SWITCH_PIN, PullDown);
    endMinZ      = new DigitalIn(FDC_ZDWN,  PullDown);
    endMaxZ      = new DigitalIn(FDC_ZUP,   PullDown);
    positionZ    = 0;

    // pinos de velocidade
    velo1Pin = new DigitalIn(BTN_VELO1, PullDown);
    velo2Pin = new DigitalIn(BTN_VELO2, PullDown);
    velo3Pin = new DigitalIn(BTN_VELO3, PullDown);

    // pipeta
    pipette = new DigitalOut(PIPETA);
    pipette->write(0);
}

void Pipetadora_Homing(void) {
    // Garante PERIODO_MINIMO para X/Y e VEL_STEP_MS_Z para Z
    for (int i = 0; i < MotorCount; ++i) {
        periodoMinAtual[i] = PERIODO_MINIMO[i];
        periodCur[i]       = PERIODO_MINIMO[i];
    }
    velStepMsZCurrent = VEL_STEP_MS_Z;

    // Rotinas de homing
    homingZ();
    HomingXY();
}

// avanço/back do Z
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

// controle manual
extern "C" void Pipetadora_ManualControl(void) {
    // toggle Y↔Z
    bool raw = switchSelect->read();
    if (raw && !prevSwRaw) {
        swMode = !swMode;
        Parar_Mov(MotorX);
        Parar_Mov(MotorY);
        coilsZ = 0;
    }
    prevSwRaw = raw;

    // ajuste de velocidade X/Y via VELO1/2/3
    if (velo1Pin->read()) {
        for (int i = 0; i < MotorCount; ++i) periodoMinAtual[i] = PERIODO_MINIMO_BAIXO[i];
    } else if (velo2Pin->read()) {
        for (int i = 0; i < MotorCount; ++i) periodoMinAtual[i] = PERIODO_MINIMO_MEDIO[i];
    } else if (velo3Pin->read()) {
        for (int i = 0; i < MotorCount; ++i) periodoMinAtual[i] = PERIODO_MINIMO[i];
    }

    // ajuste de velocidade Z via VELO1/2/3
    if (velo1Pin->read()) velStepMsZCurrent = VEL_STEP_MS_Z_BAIXO;
    else if (velo2Pin->read()) velStepMsZCurrent = VEL_STEP_MS_Z_MEDIO;
    else if (velo3Pin->read()) velStepMsZCurrent = VEL_STEP_MS_Z;

    // MOVIMENTO MANUAL EIXO X (ou Z se swMode == true)
    {
        bool up = btnUp[MotorX]->read();
        bool dn = btnDwn[MotorX]->read();
        if (!swMode) {
            if (up && !dn) { if (!tickerOn[MotorX]    || dirState[MotorX] != 0) Mover_Frente(MotorX); }
            else if (dn && !up) { if (!tickerOn[MotorX] || dirState[MotorX] != 1) Mover_Tras(MotorX); }
            else { if (tickerOn[MotorX]) Parar_Mov(MotorX); }
        } else {
            if      (up && !dn) stepZForward();
            else if (dn && !up) stepZBackward();
            else                coilsZ = 0;
        }
    }

    // MOVIMENTO MANUAL EIXO Y
    {
        bool up = btnUp[MotorY]->read();
        bool dn = btnDwn[MotorY]->read();
        if (up && !dn) { if (!tickerOn[MotorY]    || dirState[MotorY] != 0) Mover_Frente(MotorY); }
        else if (dn && !up) { if (!tickerOn[MotorY] || dirState[MotorY] != 1) Mover_Tras(MotorY); }
        else { if (tickerOn[MotorY]) Parar_Mov(MotorY); }
    }

    // aplica delay de passo Z baseado na seleção de VELO
    ThisThread::sleep_for(velStepMsZCurrent);
}

float Pipetadora_GetPositionCm(int id) {
    if (id < MotorCount) return (float)(position[id] * PASSO_FUSO[id] / 400.0f);
    return (float)(positionZ * PASSO_FUSO_Z / 400.0f);
}

int Pipetadora_GetPositionSteps(int id) {
    if (id < MotorCount) return position[id];
    return positionZ;
}

// homing XY
static void HomingXY(void) {
    stopTicker(MotorX);
    stopTicker(MotorY);
    Mover_Frente(MotorX);
    Mover_Tras(MotorY);
    while (tickerOn[MotorX] || tickerOn[MotorY]) {
        if (!emergPin.read()) return;
        if (tickerOn[MotorX] && endMax[MotorX]->read()) Parar_Mov(MotorX);
        if (tickerOn[MotorY] && endMin[MotorY]->read()) Parar_Mov(MotorY);
        ThisThread::sleep_for(1ms);
    }
    position[MotorX] = 0;
    position[MotorY] = 0;
}

// homing Z
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

// funções internas de ticker
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

// ISR de passo X/Y com aceleração e desaceleração suave
static void stepISR(int id) {
    // parar no fim de curso
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
        // se periodCur > periodoMinAtual: acelera (reduz período)
        if (periodCur[id] > periodoMinAtual[id]) {
            periodCur[id] -= REDUCAO_PERIODO[id];
            if (periodCur[id] < periodoMinAtual[id])
                periodCur[id] = periodoMinAtual[id];
        }
        // se periodCur < periodoMinAtual: desacelera (aumenta período)
        else if (periodCur[id] < periodoMinAtual[id]) {
            periodCur[id] += REDUCAO_PERIODO[id];
            if (periodCur[id] > periodoMinAtual[id])
                periodCur[id] = periodoMinAtual[id];
        }
        // atualiza ticker com novo período
        tickers[id]->detach();
        tickers[id]->attach(stepWrapper[id], periodCur[id]);
        tickerOn[id] = true;
    }
}

// Movimenta X/Y para frente
static void Mover_Frente(int id) {
    if (endMax[id]->read()) return;
    dirState[id] = 0;
    dirOut[id]->write(0);
    enableOut[id]->write(0);
    periodCur[id] = PERIODO_INICIAL[id];
    passoCount[id]= 0;
    startTicker(id);
}

// Movimenta X/Y para trás
static void Mover_Tras(int id) {
    if (endMin[id]->read()) return;
    dirState[id] = 1;
    dirOut[id]->write(1);
    enableOut[id]->write(0);
    periodCur[id] = PERIODO_INICIAL[id];
    passoCount[id]= 0;
    startTicker(id);
}

// Para movimento X/Y
static void Parar_Mov(int id) {
    stopTicker(id);
}

// Bresenham X
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

// Bresenham Y
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

// MoveTo, MoveLinear, ActuateValve e StopAll seguem sem alterações…

extern "C" void Pipetadora_MoveTo(int id, int targetSteps) { /* … */ }
extern "C" void Pipetadora_MoveLinear(int tx, int ty)   { /* … */ }
extern "C" void Pipetadora_ActuateValve(int volume_ml)  { /* … */ }
extern "C" void Pipetadora_StopAll(void)                { /* … */ }
extern "C" bool Pipetadora_GetToggleMode(void) { return swMode; }
