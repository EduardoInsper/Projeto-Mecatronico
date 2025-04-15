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
// agora assumindo que a leitura será 0 quando não acionados e 1 quando acionados.
DigitalIn fimIda(PC_9);
DigitalIn fimVolta(PC_8);

// Variáveis globais para controle de posição e sequência
int seqAtual = 0;       // Guarda o estado atual da sequência de acionamento
int currentPos = 0;     // Posição atual do motor (em passos). Considera 0 no fim de curso reverso.
int maxPos = 0;         // Número máximo de passos permitidos (calibrado na ida)

// Função para executar um passo na direção informada
// Se sentidoDireto == true, incrementa o estado (movimento "ida")
// Se sentidoDireto == false, decrementa o estado (movimento "volta")
void darPasso(bool sentidoDireto) {
    MP = 1 << (seqAtual);
    if (sentidoDireto) {
        seqAtual = (seqAtual + 1) % 4;
    } else {
        // Decrementa com wrap-around
        seqAtual = (seqAtual + 3) % 4;
    }
    ThisThread::sleep_for(VEL);
}

// Função que executa passos até detectar o acionamento do fim de curso.
// O parâmetro 'sentidoDireto' determina a direção do movimento.
// Agora, o laço segue enquanto o fim de curso não estiver acionado (leitura 0).
int moverAteLimite(DigitalIn &fimCurso, bool sentidoDireto) {
    int passos = 0;
    while(fimCurso.read() == 0) {  // Enquanto não acionado (0 = não acionado, 1 = acionado)
        darPasso(sentidoDireto);
        passos++;
    }
    return passos;
}

// Esta função movimenta o motor um número de passos desejado, mas somente 
// se a movimentação não ultrapassar os limites calibrados.
// Se a soma da posição atual com os passos desejados exceder o limite ou for menor que zero,
// o motor para ao atingir o limite.
void moverPassosLimitados(int passosDesejados) {
    if(passosDesejados > 0) {
        // Movimento para a "ida"
        for (int i = 0; i < passosDesejados; i++) {
            if(currentPos < maxPos) {
                darPasso(true);  // passo na direção direta
                currentPos++;
            } else {
                // Limite superior atingido, interrompe o movimento
                break;
            }
        }
    } else if(passosDesejados < 0) {
        // Movimento para a "volta" (passosDesejados é negativo)
        for (int i = 0; i < (-passosDesejados); i++) {
            if(currentPos > 0) {
                darPasso(false); // passo na direção reversa
                currentPos--;
            } else {
                // Limite inferior atingido (posição zero), interrompe o movimento
                break;
            }
        }
    }
}

int main() {
    // Variáveis para armazenar os limites (número de passos) da ida e da volta
    int posIda = 0;
    int posVolta = 0;
    
    while(1) {
        // Inicia a rotina de calibração quando o botão for pressionado 
        // (assumindo lógica ativa baixa)
        if (Botao.read() == 0) {
            // Calibração na ida: move até acionar o fim de curso (leitura passa a ser 1)
            posIda = moverAteLimite(fimIda, true);
            
            // Pequena pausa para estabilização (opcional)
            ThisThread::sleep_for(100ms);
            
            // Calibração na volta: move até acionar o fim de curso (leitura passa a ser 1)
            posVolta = moverAteLimite(fimVolta, false);
            
            // Após a calibração, assume-se que o motor esteja no fim de curso reverso.
            // Define a posição atual como 0 e o limite máximo como o número de passos da ida.
            currentPos = 0;
            maxPos = posIda;
            
            // Exemplo de utilização:
            // Tenta mover 10 passos para a "ida". Se já estiver próximo do limite,
            // o movimento parará ao atingir o máximo permitido.
            moverPassosLimitados(10);
            
            // Tenta mover 10 passos para a "volta". Se o motor estiver no limite inferior (posição 0),
            // nenhum movimento ocorrerá.
            moverPassosLimitados(-10);
        }
    }
}

