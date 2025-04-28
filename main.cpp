#include "mbed.h"
#include "pinos.h"
#include <chrono>

using namespace std::chrono;

// ==== PINOS ====  
static constexpr PinName PINO_STEP       = MOTOR_Y;      // pulso de step  
static constexpr PinName PINO_DIR        = DIR_Y;        // direção: 0=frente→MAX, 1=trás→MIN  
static constexpr PinName PINO_ENABLE     = EN_Y;         // low = driver habilitado  

static constexpr PinName FDC_MIN_PIN     = FDC_YDWN;     // fim-de-curso MIN (ativo HIGH)  
static constexpr PinName FDC_MAX_PIN     = FDC_YUP;      // fim-de-curso MAX (ativo HIGH)  

static constexpr PinName BTN_START_REF   = PB_13;        // inicia homing  
static constexpr PinName BTN_MOVE_FWD    = PC_3;         // move frente enquanto pressionado  
static constexpr PinName BTN_MOVE_BWD    = PA_15;        // move trás enquanto pressionado  

// ==== PARÂMETROS DE VELOCIDADE ====  
static constexpr microseconds PERIODO_INICIAL      {300};  // 0.3 ms  
static constexpr microseconds PERIODO_MINIMO       { 60};  // 0,2 ms (velocidade máxima)  
static constexpr int          PASSOS_PARA_ACELERAR { 50};  
static constexpr microseconds REDUCAO_PERIOD       { 10};  // 0,01 ms  

// ==== TIPOS AUXILIARES ====  
enum class Direcao { FRENTE, TRAS };

// ==== OBJETOS MBED ====  
DigitalOut   stepPin        (PINO_STEP,     0);
DigitalOut   dirPin         (PINO_DIR,      0);
DigitalOut   enablePin      (PINO_ENABLE,   1);

DigitalIn    limitSwitchMin (FDC_MIN_PIN);
DigitalIn    limitSwitchMax (FDC_MAX_PIN);

DigitalIn    btnStartRef    (BTN_START_REF);
DigitalIn    btnMoveFwd     (BTN_MOVE_FWD);
DigitalIn    btnMoveBwd     (BTN_MOVE_BWD);

Ticker       stepTicker;

// ==== ESTADO GLOBAL ====  
volatile int32_t   position       = 0;
int32_t            minPosition    = 0;
int32_t            maxPosition    = 0;
bool               softwareLimits = false;
volatile Direcao   currentDir     = Direcao::FRENTE;
volatile bool      tickerRunning  = false;
bool               homed          = false;

// aceleração  
microseconds periodoAtual  = PERIODO_INICIAL;
int          contadorPassos = 0;

// estados anteriores dos botões (para borda de subida)
bool lastFwdState = false;
bool lastBwdState = false;

// —— Protótipos ——
void stepISR();
void startTicker();
void stopTicker();
void Mover_Frente();
void Mover_Tras();
void Parar_Movimento();
void Referenciamento();


// ==== IMPLEMENTAÇÃO ====  

void stepISR() {
    // **Software-limits guard**: não gera novos pulsos além dos limites
    if (softwareLimits) {
        if (currentDir == Direcao::FRENTE && position >= maxPosition) {
            stopTicker();
            return;
        }
        if (currentDir == Direcao::TRAS && position <= minPosition) {
            stopTicker();
            return;
        }
    }

    // gera pulso
    stepPin = !stepPin;
    if (!stepPin) return;  // conta só na borda de subida

    // 1) atualiza posição
    if (currentDir == Direcao::FRENTE)  position++;
    else                                 position--;

    // 2) soft-limits no ISR já feito acima

    // 3) aceleração a cada N passos
    if (++contadorPassos >= PASSOS_PARA_ACELERAR) {
        contadorPassos = 0;
        if (periodoAtual > PERIODO_MINIMO) {
            periodoAtual -= REDUCAO_PERIOD;
            stepTicker.attach(&stepISR, periodoAtual);
            tickerRunning = true;
        }
    }
}

void startTicker() {
    if (!tickerRunning) {
        stepTicker.attach(&stepISR, periodoAtual);
        tickerRunning = true;
    }
}

void stopTicker() {
    stepTicker.detach();
    tickerRunning = false;
}

void Mover_Frente() {
    // impede iniciar se já no limite
    if (softwareLimits && position >= maxPosition) {
        return;
    }
    // reset de aceleração e partida
    periodoAtual   = PERIODO_INICIAL;
    contadorPassos = 0;
    currentDir     = Direcao::FRENTE;
    dirPin         = 0;
    enablePin      = 0;
    startTicker();
}

void Mover_Tras() {
    if (softwareLimits && position <= minPosition) {
        return;
    }
    periodoAtual   = PERIODO_INICIAL;
    contadorPassos = 0;
    currentDir     = Direcao::TRAS;
    dirPin         = 1;
    enablePin      = 0;
    startTicker();
}

void Parar_Movimento() {
    stopTicker();
    enablePin = 1;
}

void Referenciamento() {
    Parar_Movimento();
    homed          = false;
    softwareLimits = false;
    position       = 0;
    minPosition    = 0;
    maxPosition    = 0;

    // 1) bate no MIN
    Mover_Tras();
    while (!limitSwitchMin.read()) { ThisThread::sleep_for(1ms); }
    Parar_Movimento();

    // 2) sai do MIN → define 0
    Mover_Frente();
    while (limitSwitchMin.read())  { ThisThread::sleep_for(1ms); }
    Parar_Movimento();
    position    = 0;
    minPosition = 0;

    // 3) bate no MAX
    Mover_Frente();
    while (!limitSwitchMax.read()) { ThisThread::sleep_for(1ms); }
    Parar_Movimento();
    maxPosition = position;

    // 4) sai do MAX
    Mover_Tras();
    while (limitSwitchMax.read())  { ThisThread::sleep_for(1ms); }
    Parar_Movimento();

    softwareLimits = true;
    homed          = true;
}

int main() {
    // pull-downs nos botões e fins de curso
    btnStartRef.   mode(PullDown);
    btnMoveFwd.    mode(PullDown);
    btnMoveBwd.    mode(PullDown);
    limitSwitchMin.mode(PullDown);
    limitSwitchMax.mode(PullDown);

    // aguarda homing
    while (!btnStartRef.read()) {
        ThisThread::sleep_for(10ms);
    }
    Referenciamento();

    // controle manual com borda de botão
    while (true) {
        bool fwd = homed && btnMoveFwd.read();
        bool bwd = homed && btnMoveBwd.read();

        if (fwd && !lastFwdState) {
            Mover_Frente();
        }
        else if (bwd && !lastBwdState) {
            Mover_Tras();
        }
        else if (!fwd && !bwd && tickerRunning) {
            Parar_Movimento();
        }

        lastFwdState = fwd;
        lastBwdState = bwd;

        ThisThread::sleep_for(10ms);
    }
}
