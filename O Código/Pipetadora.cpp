// Pipetadora.h
#ifndef PIPETADORA_H
#define PIPETADORA_H

// Inicializa GPIO, tickers e variáveis internas de motores
void Pipetadora_InitMotors();

// Executa rotina de homing (referenciamento) dos eixos X e Y
void Pipetadora_Homing();

// Loop de controle manual; deve ser chamado repetidamente até retornar
void Pipetadora_ManualControl();

// Retorna posição (em cm) do eixo especificado (0=X, 1=Y, 2=Z)
float Pipetadora_GetPositionCm(int id);

#endif // PIPETADORA_H


// Pipetadora.cpp
#include "mbed.h"
#include "pinos.h"
#include "Pipetadora.h"
#include <chrono>
#include <cmath>

using namespace std::chrono;
constexpr float PI = 3.14159265f;

// Evita redefinição de I2C_SDA/SCL
#ifdef I2C_SDA
#undef I2C_SDA
#endif
#ifdef I2C_SCL
#undef I2C_SCL
#endif

// — Identificadores de eixos
enum MotorId { MotorX = 0, MotorY = 1, MotorZ = 2, MotorCount };

// — Pinos por eixo
static constexpr PinName STEP_PIN   [MotorCount] = { MOTOR_X,   MOTOR_Y,   NC };
static constexpr PinName DIR_PIN    [MotorCount] = { DIR_X,     DIR_Y,     NC };
static constexpr PinName ENABLE_PIN [MotorCount] = { EN_X,      EN_Y,      NC };
static constexpr PinName FDC_MIN_PIN[MotorCount] = { FDC_XDWN,  FDC_YDWN,  NC };
static constexpr PinName FDC_MAX_PIN[MotorCount] = { FDC_XUP,   FDC_YUP,   NC };

// — Parâmetros de velocidade
static constexpr microseconds PERIODO_INICIAL[MotorCount] = { 1000us, 800us,  800us };
static constexpr microseconds PERIODO_MINIMO  [MotorCount] = { 175us,  175us,  175us };
static constexpr int          PASSOS_PARA_ACELERAR       = 25;
static constexpr microseconds REDUCAO_PERIODO[MotorCount] = { 25us,   25us,   25us };

// — Raio de fuso (cm)
static constexpr float DIAMETRO_FUSO[MotorCount] = { 16.0f, 14.0f, 16.0f };
static constexpr float RFUSO_CM    [MotorCount] = {
    DIAMETRO_FUSO[0]/2.0f/10.0f,
    DIAMETRO_FUSO[1]/2.0f/10.0f,
    DIAMETRO_FUSO[2]/2.0f/10.0f
};
static float RFuso[MotorCount] = { RFUSO_CM[0], RFUSO_CM[1], RFUSO_CM[2] };

// — Objetos de hardware
static DigitalOut* stepOut   [MotorCount];
static DigitalOut* dirOut    [MotorCount];
static DigitalOut* enableOut [MotorCount];
static DigitalIn*  endMin    [MotorCount];
static DigitalIn*  endMax    [MotorCount];
static Ticker      ticker    [MotorCount];

// — Estado de cada eixo
static volatile int32_t position  [MotorCount] = {0,0,0};
static volatile bool    tickerOn  [MotorCount] = {false,false,false};
static int             dirState  [MotorCount] = {0,0,0};  // 0→frente,1→trás
static microseconds     periodCur [MotorCount] = {
    PERIODO_INICIAL[0],
    PERIODO_INICIAL[1],
    PERIODO_INICIAL[2]
};
static int             stepCount [MotorCount] = {0,0,0};

// — Protótipos internos
static void stepISR      (int id);
static void startTicker  (int id);
static void stopTicker   (int id);
static void Mover_Frente (int id);
static void Mover_Tras   (int id);
static void Parar_Mov    (int id);
static void HomingTodos  ();

// — Converte passos em cm
float Pipetadora_GetPositionCm(int id) {
    return (2.0f * PI * RFuso[id] / 400.0f) * float(position[id]);
}

void Pipetadora_InitMotors() {
    for (int i = 0; i < MotorCount; ++i) {
        stepOut  [i] = new DigitalOut(STEP_PIN[i],    0);
        dirOut   [i] = new DigitalOut(DIR_PIN[i],     0);
        enableOut[i] = new DigitalOut(ENABLE_PIN[i],  1);
        endMin   [i] = new DigitalIn (FDC_MIN_PIN[i], PullDown);
        endMax   [i] = new DigitalIn (FDC_MAX_PIN[i], PullDown);
        periodCur[i] = PERIODO_INICIAL[i];
    }
}

void Pipetadora_Homing() {
    HomingTodos();
}

void Pipetadora_ManualControl() {
    static DigitalIn btnXup(BTN_XUP,  PullDown),
                     btnXdn(BTN_XDWN, PullDown),
                     btnYup(BTN_YUP,  PullDown),
                     btnYdn(BTN_YDWN, PullDown);
    bool xup = btnXup.read(), xdn = btnXdn.read();
    bool yup = btnYup.read(), ydn = btnYdn.read();
    if (xup && !xdn) {
        if (!tickerOn[MotorX] || dirState[MotorX] != 0) Mover_Frente(MotorX);
    } else if (xdn && !xup) {
        if (!tickerOn[MotorX] || dirState[MotorX] != 1) Mover_Tras(MotorX);
    } else if (tickerOn[MotorX]) Parar_Mov(MotorX);
    if (yup && !ydn) {
        if (!tickerOn[MotorY] || dirState[MotorY] != 0) Mover_Frente(MotorY);
    } else if (ydn && !yup) {
        if (!tickerOn[MotorY] || dirState[MotorY] != 1) Mover_Tras(MotorY);
    } else if (tickerOn[MotorY]) Parar_Mov(MotorY);
    ThisThread::sleep_for(10ms);
}

// — Implementações internas —
static void stepISR(int id) {
    // para se atingir fim de curso físico
    if ((dirState[id] == 0 && endMax[id]->read()) ||
        (dirState[id] == 1 && endMin[id]->read())) {
        stopTicker(id);
        return;
    }
    // gera pulso
    (*stepOut[id]) = !(*stepOut[id]);
    if (!(*stepOut[id])) return;
    // atualiza posição: inverter sinal para MotorX
    if (id == MotorX) {
        position[id] += (dirState[id] == 0 ? -1 : +1);
    } else {
        position[id] += (dirState[id] == 0 ? +1 : -1);
    }
    // aceleração
    if (++stepCount[id] >= PASSOS_PARA_ACELERAR) {
        stepCount[id] = 0;
        if (periodCur[id] > PERIODO_MINIMO[id]) {
            periodCur[id] -= REDUCAO_PERIODO[id];
            ticker[id].attach([id]() { stepISR(id); }, periodCur[id]);
            tickerOn[id] = true;
        }
    }
}

static void startTicker(int id) {
    ticker[id].attach([id]() { stepISR(id); }, periodCur[id]);
    tickerOn[id] = true;
}

static void stopTicker(int id) {
    ticker[id].detach();
    tickerOn[id] = false;
    (*enableOut[id]) = 1;
}

static void Mover_Frente(int id) {
    // bloqueio em fim de curso físico
    if (endMax[id]->read()) return;
    // reconfigura aceleração
    periodCur[id] = PERIODO_INICIAL[id]; stepCount[id] = 0;
    dirState[id] = 0; (*dirOut[id]) = 0; (*enableOut[id]) = 0;
    startTicker(id);
}

static void Mover_Tras(int id) {
    // bloqueio em fim de curso físico
    if (endMin[id]->read()) return;
    // reconfigura aceleração
    periodCur[id] = PERIODO_INICIAL[id]; stepCount[id] = 0;
    dirState[id] = 1; (*dirOut[id]) = 1; (*enableOut[id]) = 0;
    startTicker(id);
}

static void Parar_Mov(int id) {
    stopTicker(id);
}

static void HomingTodos() {
    // para movimentos ativos
    for (int i : { MotorX, MotorY }) {
        Parar_Mov(i);
    }

    // move X ao fim máximo (UP) e Y ao fim mínimo (DOWN) simultaneamente
    Mover_Frente(MotorX);
    Mover_Tras (MotorY);
    while (tickerOn[MotorX] || tickerOn[MotorY]) {
        if (tickerOn[MotorX] && endMax[MotorX]->read()) Parar_Mov(MotorX);
        if (tickerOn[MotorY] && endMin[MotorY]->read()) Parar_Mov(MotorY);
        ThisThread::sleep_for(1ms);
    }

    // zera posições
    position[MotorX] = 0;
    position[MotorY] = 0;
}
