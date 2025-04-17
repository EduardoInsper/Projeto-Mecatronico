#include "mbed.h"
#include <chrono>
#include "TextLCD.h"
#include "rtos/ThisThread.h"  // Necessário para ThisThread::sleep_for

using namespace std::chrono;
using namespace std::chrono_literals;  // Permite usar "50ms", "100ms", etc.
/*
InterruptIn button(PC_13);      // Botão de usuário
DigitalOut pipetteControl(PA_5);        // Saída que aciona o relé da pipeta (defina o pino conforme a sua montagem)
DigitalOut ledIndicator(LED1);          // LED para feedback visual

// Função callback chamada na borda de subida do botão
void buttonRiseCallback() {
    // Gera um pulso de 50ms para acionar o relé da pipeta.
    // Conforme os slides, o relé é acionado com nível lógico "0".
    pipetteControl = 0;              // Ativa o relé (acionamento)
    ThisThread::sleep_for(50ms);       // Duração do pulso (50 ms) – ajuste conforme necessário
    pipetteControl = 1;              // Desativa o relé
}

int main(void) {
    // Configuração inicial: sinal de controle em estado inativo (1 = desacionado) e LED apagado
    pipetteControl = 1;
    ledIndicator = 0;

    // Configura a interrupção para detectar a borda de subida do botão
    button.rise(&buttonRiseCallback);

    // Loop principal (fica aguardando interrupções)
    while (true) {
        ThisThread::sleep_for(100ms);
    }
}
*/

/*
#define velo 0.05 // Tempo de espera entre passos (em segundos)

BusOut MP(D2, D3, D4, D5);   // Bobinas do motor
DigitalIn Botao(PC_13);       // Botão com resistor de pull-up

int passo[4] = {
    0b0001, // Ativa bobina conectada ao pino D2
    0b0010, // Ativa bobina conectada ao pino D3
    0b0100, // Ativa bobina conectada ao pino D4
    0b1000  // Ativa bobina conectada ao pino D5
};

int main() {
    while (true) {
        if (Botao == 0) { // Botão pressionado
            // Executa 50 ciclos de passos enquanto o botão permanece pressionado
            for (int contador = 0; contador <= 50; contador++) {
                for (int i = 0; i < 4; i++) {
                    if (Botao != 0) {
                        MP = 0;
                        break;
                    }
                    MP = passo[i];
                    ThisThread::sleep_for(chrono::milliseconds(static_cast<int>(velo * 1000))); // 50 ms
                    MP = 0;
                    ThisThread::sleep_for(chrono::milliseconds(static_cast<int>(velo * 1000))); // 50 ms
                }
                if (Botao != 0) break;
            }
        } else {
            MP = 0;
        }
    }
}
*/



// rs=D8, e=D9, d4=D4, d5=D5, d6=D6, d7=D7 
TextLCD lcd(D8, D9, D4, D5, D6, D7); 

DigitalIn botaoUp(A0);
DigitalIn botaoDown(A1);
DigitalIn botaoSelect(A2);
DigitalIn botaoBack(A3);

// Definição das opções de menu
const int MENU_LENGTH = 4;
const char* opcoes[MENU_LENGTH] = {"Referenciamento", "Recipientes in", "Recipientes out", "Mover eixos"};
int indiceAtual = 0;

int main() {
    // Leitura ativa em nível alto (Pull-Down interno)
    botaoUp.mode(PullDown);
    botaoDown.mode(PullDown);
    botaoSelect.mode(PullDown);
    botaoBack.mode(PullDown);  // Configura PullDown para Back

    while (true) {
        // === exibição do menu principal ===
        lcd.cls();  // Limpa o display

        // Define as duas opções a serem exibidas
        int linhaInicio = (indiceAtual == MENU_LENGTH - 1) ? indiceAtual - 1 : indiceAtual;
        int linhaSeguinte = linhaInicio + 1;
        if (linhaSeguinte >= MENU_LENGTH) linhaSeguinte = linhaInicio;

        // Exibe a primeira linha com indicador “>”
        lcd.locate(0, 0);
        lcd.printf("%s%-14s", (linhaInicio == indiceAtual) ? "> " : "  ", opcoes[linhaInicio]);

        // Exibe a segunda linha
        lcd.locate(0, 1);
        lcd.printf("%s%-14s", (linhaSeguinte == indiceAtual) ? "> " : "  ", opcoes[linhaSeguinte]);

        // Navegação para cima
        if (botaoUp) {
            indiceAtual = (indiceAtual - 1 + MENU_LENGTH) % MENU_LENGTH;
            ThisThread::sleep_for(200ms);  // Debounce
        }

        // Navegação para baixo
        if (botaoDown) {
            indiceAtual = (indiceAtual + 1) % MENU_LENGTH;
            ThisThread::sleep_for(200ms);
        }

        // Seleção da opção
        if (botaoSelect) {
            // entra na “subtela” de confirmação
            lcd.cls();
            lcd.locate(0, 0);
            lcd.printf("Selecionado:");
            lcd.locate(0, 1);
            lcd.printf("%s", opcoes[indiceAtual]);

            // informa que pressione Back para voltar
            ThisThread::sleep_for(500ms);
            lcd.cls();
            lcd.locate(0, 0);
            lcd.printf("%s", opcoes[indiceAtual]);
            lcd.locate(0, 1);
            lcd.printf("< Back");

            // fica aguardando Back
            while (!botaoBack) {
                ThisThread::sleep_for(100ms);
            }
            ThisThread::sleep_for(200ms);  // Debounce do Back

            // ao sair, volta ao menu (continue implicitamente)
        }

        ThisThread::sleep_for(100ms);
    }
}

