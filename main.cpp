#include "mbed.h"
#include <chrono>

using namespace std::chrono;

// Define tempo de espera de 5 ms
#define velo 0.005f  

// Declaração de pinos para controle do motor de passo
BusOut MP(D5, D4, D3, D2);

// Declaração de pinos para controle do display de 7 segmentos
DigitalOut a(D6);
DigitalOut b(D7);
DigitalOut c(D8);
DigitalOut d(D9);
DigitalOut e(D10);
DigitalOut f(D11);
DigitalOut g(D12);
DigitalOut h(D13);

// Controle PWM no pino analógico chamado "Batata"
PwmOut Batata(A3);

// Entrada digital para o botão conectado no pino PC_13
DigitalIn Botao(PC_13);

// Frequências das notas desejadas (Hz)
const float FREQUENCIA_DO   = 264.0f; // dó
const float FREQUENCIA_MI   = 330.0f; // mi
const float FREQUENCIA_SOL  = 396.0f; // sol

// Tempo que cada nota ficará ativa (em segundos)
const float TEMPO_NOTA = 0.25f;

// Função para tocar uma nota
void tocarNota(float freq, float duracao) {
    // Ajusta o período de acordo com a frequência desejada
    Batata.period(1.0f / freq);
    // Define 50% de duty cycle
    Batata.write(0.50f);
    // Aguarda pelo tempo da nota
    ThisThread::sleep_for(milliseconds((int)(duracao * 1000.0f)));
    // Desliga o PWM (buzzer) antes da próxima nota
    Batata.write(0.0f);
}

int main() {
    while (1) {
        if (Botao == 0) {
            // 1) Movimento do motor de passo
            for (int contador = 0; contador <= 50; contador++) {
                for (int i = 0; i < 4; i++) {
                    MP = 1 << i;
                    // Substitui wait(velo) por sleep_for(5ms)
                    ThisThread::sleep_for(5ms);
                }
            }

            // 2) Liga todos os segmentos do display
            a = b = c = d = e = f = g = h = 1;

            // 3) Toca as 3 notas em sequência (dó, mi, sol)
            tocarNota(FREQUENCIA_DO,  TEMPO_NOTA); // dó
            tocarNota(FREQUENCIA_MI,  TEMPO_NOTA); // mi
            tocarNota(FREQUENCIA_SOL, TEMPO_NOTA); // sol

            // 4) Desliga o display ao final
            a = b = c = d = e = f = g = h = 0;
        }
    }
}
