#include "mbed.h"
#include "pinos.h"
#include "Pipetadora.h"
#include <chrono>
using namespace std::chrono;

// — Identificadores de eixos
enum MotorId { MotorX = 0, MotorY = 1, MotorZ = 2, MotorCount };

// — Pinos
static constexpr PinName STEP_PIN   [MotorCount] = { MOTOR_X,   MOTOR_Y,   MOTOR_Z   };
static constexpr PinName DIR_PIN    [MotorCount] = { DIR_X,     DIR_Y,     DIR_Z     };
static constexpr PinName ENABLE_PIN [MotorCount] = { EN_X,      EN_Y,      EN_Z      };
static constexpr PinName ENDMIN_PIN [MotorCount] = { FDC_XDWN,  FDC_YDWN,  FDC_ZDWN  };
static constexpr PinName ENDMAX_PIN [MotorCount] = { FDC_XUP,   FDC_YUP,   FDC_ZUP   };
static constexpr PinName BTN_UP_PIN [MotorCount] = { BTN_XUP,   BTN_YUP,   BTN_ZUP   };
static constexpr PinName BTN_DWN_PIN[MotorCount] = { BTN_XDWN,  BTN_YDWN,  BTN_ZDWN  };

// — Parâmetros de velocidade
static constexpr microseconds PERIODO_INICIAL[MotorCount] = { 1000us, 800us, 800us };
static constexpr microseconds PERIODO_MINIMO  [MotorCount] = {  175us, 175us, 175us };
static constexpr microseconds REDUCAO_PERIODO [MotorCount] = {   25us,  25us,  25us };
static constexpr int          PASSOS_PARA_ACELERAR      = 25;

// — Passo de fuso (cm) para conversão de posição
static constexpr float PASSO_FUSO[MotorCount] = { 0.5f, 0.5f, 0.5f };

// — Hardware e estado
static DigitalOut* stepOut   [MotorCount];
static DigitalOut* dirOut    [MotorCount];
static DigitalOut* enableOut [MotorCount];
static DigitalIn*  endMin    [MotorCount];
static DigitalIn*  endMax    [MotorCount];
static DigitalIn*  btnUp     [MotorCount];
static DigitalIn*  btnDwn    [MotorCount];
static Ticker*     tickers   [MotorCount];

static volatile int32_t    position    [MotorCount];
static volatile bool       tickerOn    [MotorCount];
static microseconds        periodCur   [MotorCount];
static int                 passoCount  [MotorCount];
static int                 dirState    [MotorCount]; // 0 → frente, 1 → trás

// — Wrappers para Ticker
static void stepISR(int id);
static void stepISR0() { stepISR(0); }
static void stepISR1() { stepISR(1); }
static void stepISR2() { stepISR(2); }
static void (* const stepWrapper[MotorCount])() = {
    stepISR0, stepISR1, stepISR2
};

// — Protótipos internos
static void startTicker(int id);
static void stopTicker (int id);
static void Mover_Frente(int id);
static void Mover_Tras  (int id);
static void Parar_Mov   (int id);
static void HomingTodos (void);

// — API pública —

void Pipetadora_InitMotors(void) {
    for (int i = 0; i < MotorCount; ++i) {
        stepOut  [i] = new DigitalOut(STEP_PIN   [i], 0);
        dirOut   [i] = new DigitalOut(DIR_PIN    [i], 0);
        enableOut[i] = new DigitalOut(ENABLE_PIN [i], 1);
        endMin   [i] = new DigitalIn (ENDMIN_PIN [i], PullDown);
        endMax   [i] = new DigitalIn (ENDMAX_PIN [i], PullDown);
        btnUp    [i] = new DigitalIn (BTN_UP_PIN  [i], PullDown);
        btnDwn   [i] = new DigitalIn (BTN_DWN_PIN [i], PullDown);
        tickers  [i] = new Ticker();

        position   [i] = 0;
        tickerOn   [i] = false;
        periodCur  [i] = PERIODO_INICIAL[i];
        passoCount [i] = 0;
        dirState   [i] = 0;
    }
}

void Pipetadora_Homing(void) {
    HomingTodos();
}

void Pipetadora_ManualControl(void) {
    for (int i = 0; i < MotorCount; ++i) {
        bool up = btnUp[i]->read();
        bool dn = btnDwn[i]->read();
        if      (up && !dn) {
            if (!tickerOn[i] || dirState[i] != 0) Mover_Frente(i);
        } else if (dn && !up) {
            if (!tickerOn[i] || dirState[i] != 1) Mover_Tras(i);
        } else {
            if (tickerOn[i]) Parar_Mov(i);
        }
    }
    ThisThread::sleep_for(10ms);
}

float Pipetadora_GetPositionCm(int id) {
    double cm = (double)position[id] * (double)PASSO_FUSO[id] / 400.0;
    return (float)cm;
}

int Pipetadora_GetPositionSteps(int id) {
    return position[id];
}

// — Implementações internas —

static void startTicker(int id) {
    if (!tickerOn[id]) {
        // Usar attach com duração chrono em vez de attach_us
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
    // verifica fim de curso
    if ((dirState[id] == 0 && endMax[id]->read()) ||
        (dirState[id] == 1 && endMin[id]->read())) {
        stopTicker(id);
        return;
    }

    // gera pulso STEP (toggle)
    bool st = !stepOut[id]->read();
    stepOut[id]->write(st);

    // conta apenas na borda de subida, dobrando o incremento
    if (st) {
        int delta = (dirState[id] == 0 ? +1 : -1);
        if (id == MotorX) delta = -delta;
        position[id] += delta * 2;
    }

    // aceleração incremental
    if (++passoCount[id] >= PASSOS_PARA_ACELERAR) {
        passoCount[id] = 0;
        if (periodCur[id] > PERIODO_MINIMO[id]) {
            periodCur[id] -= REDUCAO_PERIODO[id];
            if (periodCur[id] < PERIODO_MINIMO[id]) {
                periodCur[id] = PERIODO_MINIMO[id];
            }
            // re-attach com novo período usando attach
            tickers[id]->detach();
            tickers[id]->attach(stepWrapper[id], periodCur[id]);
            tickerOn[id] = true;
        }
    }
}

static void Mover_Frente(int id) {
    if (endMax[id]->read()) return;
    dirState[id]   = 0;
    dirOut[id]->write(0);
    enableOut[id]->write(0);
    periodCur[id]  = PERIODO_INICIAL[id];
    passoCount[id] = 0;
    startTicker(id);
}

static void Mover_Tras(int id) {
    if (endMin[id]->read()) return;
    dirState[id]   = 1;
    dirOut[id]->write(1);
    enableOut[id]->write(0);
    periodCur[id]  = PERIODO_INICIAL[id];
    passoCount[id] = 0;
    startTicker(id);
}

static void Parar_Mov(int id) {
    stopTicker(id);
}

static void HomingTodos(void) {
    stopTicker(MotorX);
    stopTicker(MotorY);
    Mover_Frente(MotorX);
    Mover_Tras (MotorY);
    while (tickerOn[MotorX] || tickerOn[MotorY]) {
        if (tickerOn[MotorX] && endMax[MotorX]->read()) Parar_Mov(MotorX);
        if (tickerOn[MotorY] && endMin [MotorY]->read()) Parar_Mov(MotorY);
        ThisThread::sleep_for(1ms);
    }
    position[MotorX] = 0;
    position[MotorY] = 0;
    position[MotorZ] = 0;
}
