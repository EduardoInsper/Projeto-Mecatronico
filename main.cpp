#include "mbed.h"
#include "pinos.h"

/*---------- PINOS (ajuste se precisar) -----------------------*/
DigitalOut  stepY (MOTOR_Y);
DigitalOut  dirY  (DIR_Y);           // 0 → MIN , 1 → MAX
DigitalOut  enY   (EN_Y , 1);        // 0 = driver ON
InterruptIn fdcMin(FDC_YDWN);        // sensor MIN – ativo ALTO
InterruptIn fdcMax(FDC_YUP);        // sensor MAX – ativo ALTO

/*---------- PARÂMETROS ---------------------------------------*/
constexpr auto STEP_PERIOD = 1ms;
Ticker  clkY;
volatile bool tickerOn = false;      // <-- NOVO

/*---------- VARIÁVEIS ----------------------------------------*/
enum class HomingState { TO_MIN, TO_MAX, BACK_TO_CENTER, FINISHED };
volatile HomingState state = HomingState::TO_MIN;

volatile int32_t steps     = 0;      // posição atual
int32_t  travelSteps       = 0;      // curso total
int32_t  limitMin          = 0;
int32_t  limitMax          = 0;
volatile bool limitsActive = false;

/*---------- Funções auxiliares p/ controlar o Ticker ---------*/
inline void stopTicker()  { clkY.detach();                     tickerOn = false; }

/*---------- STEP ISR -----------------------------------------*/
void stepISR() {
    stepY = !stepY;
    if (stepY) {
        steps += (dirY ? 1 : -1);
        if (limitsActive) {
            if ((dirY  && steps >= limitMax) ||
                (!dirY && steps <= limitMin)) {
                stopTicker();        // parou no limite
                enY = 1;             // desliga driver
            }
        }
    }
}

/*---------- Funções auxiliares p/ controlar o Ticker ---------*/
inline void startTicker() { clkY.attach(&stepISR, STEP_PERIOD); tickerOn = true; }

/*---------- Fim-de-curso MIN --------------------------------*/
void reachedMin() {
    stopTicker();   enY = 1; steps = 0;
    fdcMin.rise(NULL);
    dirY = 1;       enY = 0;          // rumo ao MAX
    state = HomingState::TO_MAX;
    startTicker();
}

/*---------- Fim-de-curso MAX --------------------------------*/
void reachedMax() {
    stopTicker();   enY = 1;
    travelSteps = steps;             // curso total
    fdcMax.rise(NULL);
    dirY = 0;       enY = 0; steps = 0;  // volta p/ centro
    state = HomingState::BACK_TO_CENTER;
    startTicker();
}

/*============================================================*/
int main() {
    /*--- HOMING AUTOMÁTICO ---*/
    dirY = 0; enY = 0;                // parte em direção ao MIN
    fdcMin.rise(&reachedMin);
    startTicker();

    /*--- Aguarda homing terminar ---*/
    while (state != HomingState::FINISHED) {
        if (state == HomingState::BACK_TO_CENTER &&
            steps <= -travelSteps / 2) {
            stopTicker(); enY = 1;
            limitMin      = -travelSteps / 2;
            limitMax      =  travelSteps / 2;
            limitsActive  = true;
            state         = HomingState::FINISHED;
        }
        ThisThread::sleep_for(5ms);
    }

    /*--- LOOP VAI-E-VEM CONTÍNUO ---*/
    while (true) {
        /* Se o motor está parado (tickerOff), inverta o sentido
           e reinicie os passos até bater no outro limite         */
        if (!tickerOn) {
            dirY = !dirY;   // troca sentido
            enY  = 0;       // habilita driver
            startTicker();
        }
        ThisThread::sleep_for(5ms);
    }
}
