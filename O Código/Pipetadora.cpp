#include "mbed.h"
#include "pinos.h"
#include "Pipetadora.h"
using namespace std::chrono;

// ------------------------------------------------------------------
// Variáveis e objetos para Z
// ------------------------------------------------------------------
static DigitalIn* switchSelect;         // lê o switch Y↔Z
static DigitalIn* endMinZ;              // FDC Z inferior
static DigitalIn* endMaxZ;              // FDC Z superior
static volatile int32_t positionZ;      // contador de passos Z
static constexpr float PASSO_FUSO_Z = 1.0f;
static int zSeqIndex = 0;               // idx para sequência de bobinas Z


// — Identificadores de eixos X e Y
enum MotorId { MotorX = 0, MotorY = 1, MotorCount };

// — Parâmetros de velocidade e aceleração (X e Y)
static constexpr microseconds PERIODO_INICIAL[MotorCount] = { 1000us, 800us };
static constexpr microseconds PERIODO_MINIMO  [MotorCount] = {  175us, 175us };
static constexpr microseconds REDUCAO_PERIODO [MotorCount] = {   25us,  25us };
static constexpr int          PASSOS_PARA_ACELERAR      = 25;
static constexpr float        PASSO_FUSO[MotorCount]    = { 0.5f, 0.5f };

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

// — Controle direto do Z
static constexpr uint32_t VEL_STEP_MS_Z = 3;  // ms entre passos Z
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

    // Pequeno delay para suavizar controle
    ThisThread::sleep_for(10ms);
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
    // garante bobinas desligadas
    coilsZ = 0;
    // 1) Desce até bater no fim de curso inferior (ZDWN)
    while (!endMinZ->read()) {
        stepZBackward();
        ThisThread::sleep_for(VEL_STEP_MS_Z);
    }
    // 2) Sobe até o fim de curso superior (ZUP)
    while (!endMaxZ->read()) {
        stepZForward();
        ThisThread::sleep_for(VEL_STEP_MS_Z);
    }
    // ponto home: desliga bobinas e zera contador
    coilsZ = 0;
    positionZ = 0;
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