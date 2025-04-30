#include "mbed.h"
#include "pinos.h"
#include <chrono>
#include <cmath>

using namespace std::chrono;
constexpr float PI = 3.14159265f;

// — Identificadores de eixos
enum MotorId { MotorX = 0, MotorY = 1, MotorZ = 2, MotorCount };

// — Pinos por eixo (X e Y ativos, Z desativado)
static constexpr PinName STEP_PIN[MotorCount]    = { MOTOR_X,   MOTOR_Y,  NC };
static constexpr PinName DIR_PIN[MotorCount]     = { DIR_X,     DIR_Y,    NC };
static constexpr PinName ENABLE_PIN[MotorCount]  = { EN_X,      EN_Y,     NC };
static constexpr PinName FDC_MIN_PIN[MotorCount] = { FDC_XDWN,  FDC_YDWN, NC };
static constexpr PinName FDC_MAX_PIN[MotorCount] = { FDC_XUP,   FDC_YUP,  NC };

// — Parâmetros de velocidade
static constexpr microseconds PERIODO_INICIAL[MotorCount] = { 1200us, 800us, 800us };
static constexpr microseconds PERIODO_MINIMO  [MotorCount] = { 150us, 175us, 175us };
static constexpr int          PASSOS_PARA_ACELERAR       = 25;
static constexpr microseconds REDUCAO_PERIODO[MotorCount] = { 25us,  25us,  25us };

// — Raio de fuso (cm) para conversão passos→cm
static constexpr float DIAMETRO_FUSO[MotorCount] = { 16.0f, 14.0f, 16.0f };
static constexpr float RFUSO_CM    [MotorCount] = {
    DIAMETRO_FUSO[0]/2.0f/10.0f,
    DIAMETRO_FUSO[1]/2.0f/10.0f,
    DIAMETRO_FUSO[2]/2.0f/10.0f
};
float RFuso[MotorCount] = {
    RFUSO_CM[0],
    RFUSO_CM[1],
    RFUSO_CM[2]
};

// — Objetos de hardware por eixo
DigitalOut* stepOut  [MotorCount];
DigitalOut* dirOut   [MotorCount];
DigitalOut* enableOut[MotorCount];
DigitalIn*  endMin   [MotorCount];
DigitalIn*  endMax   [MotorCount];
Ticker      ticker   [MotorCount];

// — Estado de cada eixo
volatile int32_t position  [MotorCount]    = {0,0,0};
volatile bool    tickerOn  [MotorCount]    = {false,false,false};
int              dirState  [MotorCount]    = {0,0,0}; // 0→frente, 1→trás
microseconds     periodCur [MotorCount]    = {
    PERIODO_INICIAL[0],
    PERIODO_INICIAL[1],
    PERIODO_INICIAL[2]
};
int              stepCount [MotorCount]    = {0,0,0};

// — Protótipos
void stepISR       (int id);
void startTicker   (int id);
void stopTicker    (int id);
void Mover_Frente  (int id);
void Mover_Tras    (int id);
void Parar_Mov     (int id);
void HomingTodos   ();
float getPositionCm(int id);
void manualControl ();

// — Converte passos em centímetros
float getPositionCm(int id) {
    return (2.0f * PI * RFuso[id] / 400.0f) * float(position[id]);
}

int main() {
    // inicializa hardware
    for (int i = 0; i < MotorCount; ++i) {
        stepOut  [i] = new DigitalOut(STEP_PIN[i],    0);
        dirOut   [i] = new DigitalOut(DIR_PIN[i],     0);
        enableOut[i] = new DigitalOut(ENABLE_PIN[i],  1); // driver desligado
        endMin   [i] = new DigitalIn (FDC_MIN_PIN[i], PullDown);
        endMax   [i] = new DigitalIn (FDC_MAX_PIN[i], PullDown);
    }

    DigitalIn btnXup (BTN_XUP,   PullDown),
              btnXdn (BTN_XDWN,  PullDown),
              btnYup (BTN_YUP,   PullDown),
              btnYdn (BTN_YDWN,  PullDown),
              btnRef (BTN_ENTER, PullDown);

    // 1) Antes de referenciar, controle manual liberado
    while (!btnRef.read()) {
        manualControl();
    }

    // 2) Homing: X ao fim UP, Y ao fim DOWN
    HomingTodos();

    // 3) Depois do homing, manual continua disponível
    while (true) {
        manualControl();
    }
}

void manualControl() {
    static DigitalIn btnXup(BTN_XUP,  PullDown),
                     btnXdn(BTN_XDWN, PullDown),
                     btnYup(BTN_YUP,  PullDown),
                     btnYdn(BTN_YDWN, PullDown);

    bool xup = btnXup.read(), xdn = btnXdn.read();
    bool yup = btnYup.read(), ydn = btnYdn.read();

    // — eixo X
    if (xup && !xdn) {
        if (!tickerOn[MotorX] || dirState[MotorX] != 0) {
            Mover_Frente(MotorX);
        }
    } else if (xdn && !xup) {
        if (!tickerOn[MotorX] || dirState[MotorX] != 1) {
            Mover_Tras(MotorX);
        }
    } else if (tickerOn[MotorX]) {
        Parar_Mov(MotorX);
    }

    // — eixo Y
    if (yup && !ydn) {
        if (!tickerOn[MotorY] || dirState[MotorY] != 0) {
            Mover_Frente(MotorY);
        }
    } else if (ydn && !yup) {
        if (!tickerOn[MotorY] || dirState[MotorY] != 1) {
            Mover_Tras(MotorY);
        }
    } else if (tickerOn[MotorY]) {
        Parar_Mov(MotorY);
    }

    ThisThread::sleep_for(10ms);
}

void stepISR(int id) {
    // para se atingir fim de curso físico
    if ((dirState[id]==0 && endMax[id]->read()) ||
        (dirState[id]==1 && endMin[id]->read())) {
        stopTicker(id);
        return;
    }

    // gera pulso de step
    (*stepOut[id]) = !(*stepOut[id]);
    if (!(*stepOut[id])) return;  // conta só na borda de subida

    // atualiza posição
    position[id] += (dirState[id]==0 ? +1 : -1);

    // aceleração
    if (++stepCount[id] >= PASSOS_PARA_ACELERAR) {
        stepCount[id] = 0;
        if (periodCur[id] > PERIODO_MINIMO[id]) {
            periodCur[id] -= REDUCAO_PERIODO[id];
            ticker[id].attach([id](){ stepISR(id); }, periodCur[id]);
            tickerOn[id] = true;
        }
    }
}

void startTicker(int id) {
    ticker[id].attach([id](){ stepISR(id); }, periodCur[id]);
    tickerOn[id] = true;
}

void stopTicker(int id) {
    ticker[id].detach();
    tickerOn[id] = false;
    (*enableOut[id]) = 1; // desliga driver
}

void Mover_Frente(int id) {
    // bloqueio em fim de curso físico
    if (endMax[id]->read()) return;

    // reconfigura aceleração
    periodCur[id] = PERIODO_INICIAL[id];
    stepCount[id] = 0;

    // direção normal para todos
    dirState[id]  = 0;
    (*dirOut[id]) = 0;

    (*enableOut[id]) = 0; // habilita driver
    startTicker(id);
}

void Mover_Tras(int id) {
    // bloqueio em fim de curso físico
    if (endMin[id]->read()) return;

    // reconfigura aceleração
    periodCur[id] = PERIODO_INICIAL[id];
    stepCount[id] = 0;

    // direção normal para todos
    dirState[id]  = 1;
    (*dirOut[id]) = 1;

    (*enableOut[id]) = 0; // habilita driver
    startTicker(id);
}

void Parar_Mov(int id) {
    stopTicker(id);
}

void HomingTodos() {
    // para movimentos ativos
    for (int i : { MotorX, MotorY }) {
        Parar_Mov(i);
    }

    // move X até o fim de curso MAX e Y até o fim de curso MIN simultaneamente
    Mover_Frente(MotorX);
    Mover_Tras (MotorY);
    while (tickerOn[MotorX] || tickerOn[MotorY]) {
        if (tickerOn[MotorX] && endMax[MotorX]->read()) Parar_Mov(MotorX);
        if (tickerOn[MotorY] && endMin[MotorY]->read()) Parar_Mov(MotorY);
        ThisThread::sleep_for(1ms);
    }

    // zera posição em ambos: X = 0 no fim MAX, Y = 0 no fim MIN
    position[MotorX] = 0;
    position[MotorY] = 0;
}
