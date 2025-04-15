#include "mbed.h"
#include <chrono>
#include "TextLCD.h"
#include "rtos/ThisThread.h"  // Necessário para ThisThread::sleep_for

using namespace std::chrono;

// Define tempo de espera para cada passo (5 ms)
#define VEL 5ms  

// Declaração dos pinos para controle do motor de passo
BusOut MP(D5, D4, D3, D2);

// Entrada digital para disparo da rotina de calibração (por exemplo, PC_13)
DigitalIn Botao(PC_13);

// Declaração dos fins de curso (limit switches) – 
// assumindo que a leitura será 0 quando não acionados e 1 quando acionados.
DigitalIn fimIda(PC_9); //mudar pinos
DigitalIn fimVolta(PC_8); //mudar pinos

// Variáveis globais para controle de posição e sequência
int seqAtual = 0;       // Estado atual da sequência de acionamento
int currentPos = 0;     // Posição atual do motor (em passos). Considera 0 no fim de curso reverso.
int maxPos = 0;         // Número máximo de passos permitidos (definido na calibração)

// Função para executar um passo na direção informada
// Se sentidoDireto == true, incrementa o estado (movimento "ida")
// Se sentidoDireto == false, decrementa o estado (movimento "volta")
void darPasso(bool sentidoDireto) {
    MP = 1 << seqAtual;
    if (sentidoDireto) {
        seqAtual = (seqAtual + 1) % 4;
    } else {
        // Decrementa com wrap-around (garante a sequência correta)
        seqAtual = (seqAtual + 3) % 4;
    }
    ThisThread::sleep_for(VEL);
}

// Função que executa passos até detectar o acionamento do fim de curso.
// O parâmetro 'sentidoDireto' determina a direção do movimento.
int moverAteLimite(DigitalIn &fimCurso, bool sentidoDireto) {
    int passos = 0;
    while(fimCurso.read() == 0) {  // Enquanto o sensor indicar "não acionado" (0)
        darPasso(sentidoDireto);
        passos++;
    }
    return passos;
}

// Esta função movimenta o motor um número de passos desejado, mas somente 
// se a movimentação não ultrapassar os limites calibrados.
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
        // Movimento para a "volta" (passosDesejados negativo)
        for (int i = 0; i < (-passosDesejados); i++) {
            if(currentPos > 0) {
                darPasso(false); // passo na direção reversa
                currentPos--;
            } else {
                // Limite inferior atingido (posição 0), interrompe o movimento
                break;
            }
        }
    }
}

int main() {
    // Variáveis temporárias para armazenar os passos durante a calibração
    int posIda = 0;
    int posVolta = 0;
    
    // Espera que o botão seja pressionado para iniciar a calibração.
    // A lógica assumida é: Botão pressionado = leitura 0.
    while(1) {
        if (Botao.read() == 0) {
            // Calibração "ida": move até acionar o fim de curso do sentido "ida" (fimIda)
            posIda = moverAteLimite(fimIda, true);
            
            // Pequena pausa para estabilização (opcional)
            ThisThread::sleep_for(100ms);
            
            // Calibração "volta": move até acionar o fim de curso do sentido "volta" (fimVolta)
            posVolta = moverAteLimite(fimVolta, false);
            
            // Após a calibração, assume-se que o motor esteja no fim de curso reverso.
            // Define a posição atual como 0 e o limite máximo como o número de passos medido na ida.
            currentPos = 0;
            maxPos = posIda;
            
            // Sai do laço de calibração para iniciar o movimento contínuo.
            break;
        }
    }
    
    // Variável para controlar o sentido do movimento contínuo:
    // true = movendo "ida" (para posições maiores)
    // false = movendo "volta" (para posições menores)
    bool sentido = true;
    
    // Loop principal que mantém o motor se movendo dentro dos limites calibrados
    while(1) {
        if (sentido) {
            // Se ainda não atingiu o limite superior, move para "ida"
            if (currentPos < maxPos) {
                darPasso(true);
                currentPos++;
            } else {
                // Limite superior atingido: inverte o sentido
                sentido = false;
            }
        } else {
            // Se não estiver na posição 0, move para "volta"
            if (currentPos > 0) {
                darPasso(false);
                currentPos--;
            } else {
                // Limite inferior (posição 0) atingido: inverte o sentido para voltar à "ida"
                sentido = true;
            }
        }
    }
}



/*

// Configuração dos pinos para o LCD (ajuste conforme sua fiação)
TextLCD lcd(PA_0, PA_1, PA_2, PA_3, PA_4, PA_5);   // rs, e, d4, d5, d6, d7

// Configuração dos botões (assumindo que os botões acionam nível baixo)
DigitalIn botaoUp(PB_0);
DigitalIn botaoDown(PB_1);
DigitalIn botaoSelect(PB_2);

const int MENU_LENGTH = 3;  // Número de opções do menu

// Vetor com as opções do menu
const char* opcoes[MENU_LENGTH] = {
    "Opcao 1",
    "Opcao 2",
    "Opcao 3"
};

int indiceAtual = 0;  // Opção atualmente selecionada

int main() {
    // Configura os botões com resistor pull-up
    botaoUp.mode(PullUp);
    botaoDown.mode(PullUp);
    botaoSelect.mode(PullUp);
    
    while (true) {
        lcd.cls();  // Limpa o display
        
        // Define qual conjunto de opções será exibido para manter 2 linhas
        int linhaInicio = (indiceAtual == MENU_LENGTH - 1) ? indiceAtual - 1 : indiceAtual;
        int linhaSeguinte = linhaInicio + 1;
        if(linhaSeguinte >= MENU_LENGTH)
            linhaSeguinte = linhaInicio;
        
        // Exibe a primeira linha
        lcd.locate(0, 0);
        if(linhaInicio == indiceAtual)
            lcd.printf("> ");
        else
            lcd.printf("  ");
        lcd.printf("%-14s", opcoes[linhaInicio]);
        
        // Exibe a segunda linha
        lcd.locate(0, 1);
        if(linhaSeguinte == indiceAtual)
            lcd.printf("> ");
        else
            lcd.printf("  ");
        lcd.printf("%-14s", opcoes[linhaSeguinte]);
        
        // Verifica o botão UP (navega para opção anterior)
        if (!botaoUp) {
            indiceAtual--;
            if (indiceAtual < 0) {
                indiceAtual = MENU_LENGTH - 1;  // Se ultrapassar o início, vai para o final
            }
            ThisThread::sleep_for(milliseconds(200));  // Delay para debouncing
        }
        
        // Verifica o botão DOWN (navega para a próxima opção)
        if (!botaoDown) {
            indiceAtual++;
            if (indiceAtual >= MENU_LENGTH) {
                indiceAtual = 0;  // Se ultrapassar o final, volta ao início
            }
            ThisThread::sleep_for(milliseconds(200));
        }
        
        // Verifica o botão SELECT (seleciona a opção atual)
        if (!botaoSelect) {
            lcd.cls();
            lcd.printf("Selecionado:");
            lcd.locate(0, 1);
            lcd.printf("%s", opcoes[indiceAtual]);
            ThisThread::sleep_for(milliseconds(2000));  // Exibe a confirmação por 2 segundos
        }
        
        ThisThread::sleep_for(milliseconds(100));  // Pequeno atraso para estabilizar a leitura dos botões
    }
}
*/