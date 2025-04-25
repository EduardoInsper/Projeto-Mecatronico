/* ==============================================================
 *  Vai-e-vem contínuo do eixo Y  │ homing MIN→MAX + limites SW
 *  DIR_Y: 0 → sentido MAX | 1 → sentido MIN   (inversão ajustada)
 * ============================================================*/

#include "mbed.h"
#include "pinos.h" 

/* --------- PINOS (ajuste se necessário) -------------------- */
DigitalOut  stepY (MOTOR_Y);
DigitalOut  dirY  (DIR_Y);        // 0 = MAX ; 1 = MIN
DigitalOut  enY   (EN_Y , 1);     // 0 = driver habilitado
InterruptIn fdcMin(FDC_YDWN);     // fim-de-curso MIN  (ativo ALTO)
InterruptIn fdcMax(FDC_YUP);      // fim-de-curso MAX  (ativo ALTO)

/* --------- PARÂMETROS -------------------------------------- */
constexpr auto STEP_PERIOD = 1ms;
Ticker  clkY;
volatile bool tickerOn = false; 

/* --------- VARIÁVEIS --------------------------------------- */
enum class HomingState { TO_MIN, TO_MAX, FINISHED };
volatile HomingState state = HomingState::TO_MIN;

volatile int32_t steps     = 0;    // posição atual (0 = MIN)
int32_t  travelSteps       = 0;    // curso total
int32_t  limitMin          = 0;    // limites de software
int32_t  limitMax          = 0;
volatile bool limitsActive = false;

inline void stopTicker()  { clkY.detach();                      tickerOn = false; }


/* --------- STEP ISR ---------------------------------------- */
void stepISR() {
    stepY = !stepY;
    if (stepY) {
        steps += (dirY ? -1 : 1);          // dirY=1 → MIN (-1); dirY=0 → MAX (+1)

        if (limitsActive) {                // aplica limites de software
            if ((dirY == 0 && steps >= limitMax) ||     // indo p/ MAX
                (dirY == 1 && steps <= limitMin)) {     // indo p/ MIN
                stopTicker();
                enY = 1;
            }
        }
    }
}

inline void startTicker() { clkY.attach(&stepISR, STEP_PERIOD); tickerOn = true; }

/* --------- Fim-de-curso MAX -------------------------------- */
void reachedMax() {
    stopTicker();               // pára o motor
    enY = 1;                    // desliga driver

    travelSteps = steps;        // distância MIN→MAX
    limitMin    = 0;
    limitMax    = travelSteps;
    limitsActive = true;        // ativa os soft-limits

    fdcMax.rise(NULL);          // desarma interrupção

    state = HomingState::FINISHED;
    /* NÃO reinicie o ticker aqui – homing terminou */
}

/* --------- Fim-de-curso MIN -------------------------------- */
void reachedMin() {
    stopTicker();     enY = 1;
    steps = 0;                          // origem absoluta = 0 no MIN
    fdcMin.rise(NULL);                  // desarma interrupção
    
    dirY = 0;        enY = 0;           // parte p/ MAX
    state = HomingState::TO_MAX;
    startTicker();
}
/* ---------- HOMING / REFERENCIAMENTO ---------- */
void referenciar() {

    /* 1. Reinicializa variáveis de posição e limites */
    stopTicker();            // garante motor parado
    steps        = 0;        // posição atual
    travelSteps  = 0;        // curso total ainda desconhecido
    limitMin     = 0;
    limitMax     = 0;
    limitsActive = false;    // soft-limits desabilitados
    state        = HomingState::TO_MIN;

    /* 2. Configura direção e ISRs de fim-de-curso */
    dirY = 1;                // vai em direção ao MIN
    enY  = 0;                // habilita driver

    fdcMin.rise(&reachedMin);   // ISR para MIN
    fdcMax.rise(&reachedMax);   // ISR para MAX

    /* 3. Inicia movimento */
    startTicker();
}




/* =========================================================== */
int main() {
    
    referenciar();

    /* --- espera homing terminar --- */
    while (state != HomingState::FINISHED) {
        ThisThread::sleep_for(5ms);
    }

    /* --- LOOP VAI-E-VEM CONTÍNUO --- */
    while (true) {
        if (!tickerOn) {                // bateu limite → inverte sentido
            dirY = !dirY;
            enY  = 0;
            startTicker();
        }
        ThisThread::sleep_for(5ms);
    }
}