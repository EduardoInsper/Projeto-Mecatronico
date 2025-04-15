#include "mbed.h"
#include <chrono>

using namespace std::chrono;

// Define tempo de espera para cada passo (5 ms)
#define VEL 5ms  

// Declaração dos pinos para controle do motor de passo
BusOut MP(D5, D4, D3, D2);

// Entrada digital para disparo da rotina de calibração (por exemplo, PC_13)
DigitalIn Botao(PC_13);

// Declaração dos fins de curso (limit switches) – 
// assumindo que a leitura será 1 quando não acionados e 0 ao serem pressionados.
DigitalIn fimIda(PC_9);
DigitalIn fimVolta(PC_8);

// Função que executa um passo da sequência de 4 estados (full-step)
void darPasso(int seq) {
    MP = 1 << (seq % 4);
    ThisThread::sleep_for(VEL);
}

// Função para mover o motor até detectar o fim de curso
// Parâmetro 'sentidoDireto': se true, utiliza sequência direta (0,1,2,3);
// se false, utiliza sequência inversa.
int moverAteLimite(DigitalIn &fimCurso, bool sentidoDireto) {
    int passos = 0;
    int seq = 0;
    
    // Enquanto o fim de curso não for acionado (leitura 1 = não acionado)
    while(fimCurso.read() == 1) {
        if (sentidoDireto) {
            darPasso(seq);
            seq = (seq + 1) % 4;
        } else {
            darPasso(seq);
            seq = (seq - 1);
            if (seq < 0) {
                seq = 3;
            }
        }
        passos++;
    }
    return passos;
}

int main() {
    // Variáveis para armazenar os limites (número de passos) da ida e da volta
    int posIda = 0;
    int posVolta = 0;
    
    while(1) {
        // Inicia a rotina de calibração quando o botão for pressionado (assumindo lógica ativa baixa)
        if (Botao.read() == 0) {
            // Calibração na ida
            posIda = moverAteLimite(fimIda, true);
            
            // Pequena pausa para estabilização (opcional)
            ThisThread::sleep_for(100ms);
            
            // Calibração na volta
            posVolta = moverAteLimite(fimVolta, false);
            
            // Neste ponto, posIda e posVolta armazenam os números de passos 
            // correspondentes aos limites da movimentação do eixo.
            
            // Aqui você pode implementar a lógica para impedir que o motor ultrapasse
            // estes limites em movimentos futuros.
        }
    }
}
