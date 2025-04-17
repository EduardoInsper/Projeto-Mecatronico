#include "mbed.h"
#include "TextLCD.h"
#include <chrono>
#include "rtos/ThisThread.h"

using namespace std::chrono;
using namespace std::chrono_literals;

// Pinos I²C (SDA, SCL)
I2C i2c(D14, D15);

// Endereço I²C do módulo PCF8574
constexpr int LCD_I2C_ADDR = 0x40;

// Instância do display 20×4 via I²C
TextLCD_I2C lcd(&i2c, LCD_I2C_ADDR, TextLCD::LCD20x4);

DigitalIn botaoUp(D12);
DigitalIn botaoDown(D13);
DigitalIn botaoSelect(A0);
DigitalIn botaoBack(A1);

const int LCD_ROWS    = 4;
const int MENU_LENGTH = 4;
const char* opcoes[MENU_LENGTH] = {
    "Referenciamento",
    "Recipientes in",
    "Recipientes out",
    "Mover eixos"
};
int indiceAtual = 0;

int main() {
    // Configura pull-down nos botões
    botaoUp.mode(PullDown);
    botaoDown.mode(PullDown);
    botaoSelect.mode(PullDown);
    botaoBack.mode(PullDown);

    while (true) {
        lcd.cls();

        // Página de 4 linhas com base no índice atual
        int pageStart = (indiceAtual / LCD_ROWS) * LCD_ROWS;
        for (int row = 0; row < LCD_ROWS; row++) {
            int idx = pageStart + row;
            lcd.locate(0, row);
            if (idx < MENU_LENGTH) {
                lcd.printf("%s%-18s",
                    (idx == indiceAtual) ? "> " : "  ",
                    opcoes[idx]
                );
            } else {
                lcd.printf("  %-18s", "");  // linha em branco
            }
        }

        // Navegação
        if (botaoUp)    { indiceAtual = (indiceAtual - 1 + MENU_LENGTH) % MENU_LENGTH; ThisThread::sleep_for(200ms); continue; }
        if (botaoDown)  { indiceAtual = (indiceAtual + 1) % MENU_LENGTH; ThisThread::sleep_for(200ms); continue; }

        // Ação de seleção
        if (botaoSelect) {
            lcd.cls();
            lcd.locate(0, 0); lcd.printf("Selecionado:");
            lcd.locate(0, 1); lcd.printf("%s", opcoes[indiceAtual]);
            lcd.locate(0, 3); lcd.printf("< Back");
            while (!botaoBack) ThisThread::sleep_for(100ms);
            ThisThread::sleep_for(200ms);
        }

        ThisThread::sleep_for(100ms);
    }
}
