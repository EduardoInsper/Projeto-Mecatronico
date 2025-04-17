#include "mbed.h"
#include <chrono>
#include "TextLCD.h"
#include "rtos/ThisThread.h"  // Necessário para ThisThread::sleep_for

using namespace std::chrono;
using namespace std::chrono_literals;  // Permite usar "50ms", "100ms", etc.

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

