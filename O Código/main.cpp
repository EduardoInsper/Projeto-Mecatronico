// main.cpp

#include "mbed.h"
#include "TextLCD.h"
#include "pinos.h"
#include "Pipetadora.h"
#include <chrono>

using namespace std::chrono;

// I2C LCD setup
I2C        i2c_lcd(D14, D15);
TextLCD_I2C lcd(&i2c_lcd, 0x7E, TextLCD::LCD20x4);

// Buttons
InterruptIn buttonUp   (BTN_XUP,   PullDown);
InterruptIn buttonDown (BTN_XDWN,  PullDown);
InterruptIn buttonEnter(BTN_ENTER, PullDown);
InterruptIn buttonBack (BTN_BACK,  PullDown);  // BTN_BACK is defined as PB_14 in pinos.h

// Debounce timer
static constexpr auto debounceTime = 200ms;
Timer debounceTimer;

// Menu state
bool      inSubmenu = false;
int       cursor    = 0;
volatile bool upFlag    = false;
volatile bool downFlag  = false;
volatile bool enterFlag = false;
volatile bool backFlag  = false;

// Menu definitions
enum { MAIN_COUNT = 3, SUB_COUNT = 2 };
const char* mainMenu[MAIN_COUNT] = {
    "Referenciamento",
    "Movimentacao Manual",
    "Pipetadora"
};
const char* subMenu[SUB_COUNT] = {
    "Placeholder A",
    "Placeholder B"
};

// ISR handlers
void isrUp() {
    if (debounceTimer.elapsed_time() < debounceTime) return;
    debounceTimer.reset();
    upFlag = true;
}
void isrDown() {
    if (debounceTimer.elapsed_time() < debounceTime) return;
    debounceTimer.reset();
    downFlag = true;
}
void isrEnter() {
    if (debounceTimer.elapsed_time() < debounceTime) return;
    debounceTimer.reset();
    enterFlag = true;
}
void isrBack() {
    if (debounceTimer.elapsed_time() < debounceTime) return;
    debounceTimer.reset();
    backFlag = true;
}

// Draw initial animated main menu
void drawMainMenuAnim() {
    lcd.cls();
    lcd.locate(5, 1);
    lcd.printf("Carregando...");
    ThisThread::sleep_for(500ms);
    lcd.cls();
    lcd.locate(3, 0);
    lcd.printf("MENU PRINCIPAL");
    for (int i = 0; i < MAIN_COUNT; ++i) {
        lcd.locate(1, i + 1);
        lcd.printf("%s", mainMenu[i]);
    }
    cursor = 0;
    lcd.locate(0, 1 + cursor);
    lcd.putc('>');
    inSubmenu = false;
}

// Redraw main menu
void drawMainMenu() {
    lcd.cls();
    lcd.locate(3, 0);
    lcd.printf("MENU PRINCIPAL");
    for (int i = 0; i < MAIN_COUNT; ++i) {
        lcd.locate(1, i + 1);
        lcd.printf("%s", mainMenu[i]);
    }
    lcd.locate(0, 1 + cursor);
    lcd.putc('>');
}

// Draw Pipetadora submenu
void drawSubMenu() {
    lcd.cls();
    lcd.locate(3, 0);
    lcd.printf("PIPETADORA");
    for (int i = 0; i < SUB_COUNT; ++i) {
        lcd.locate(1, i + 1);
        lcd.printf("%s", subMenu[i]);
    }
    cursor = 0;
    lcd.locate(0, 1 + cursor);
    lcd.putc('>');
}

int main() {
    // Inicializa motores/sensores
    Pipetadora_InitMotors();

    // Configura botões + debounce
    debounceTimer.start();
    buttonUp.fall(&isrUp);
    buttonDown.fall(&isrDown);
    buttonEnter.fall(&isrEnter);
    buttonBack.fall(&isrBack);

    // Mostra o menu animado
    drawMainMenuAnim();

    while (true) {
        if (upFlag) {
            upFlag = false;
            int count = inSubmenu ? SUB_COUNT : MAIN_COUNT;
            if (cursor > 0) --cursor;
            (inSubmenu ? drawSubMenu : drawMainMenu)();
        }
        if (downFlag) {
            downFlag = false;
            int count = inSubmenu ? SUB_COUNT : MAIN_COUNT;
            if (cursor < count - 1) ++cursor;
            (inSubmenu ? drawSubMenu : drawMainMenu)();
        }
        if (enterFlag) {
            enterFlag = false;
            if (!inSubmenu) {
                switch (cursor) {
                    case 0: // Referenciamento
                        Pipetadora_Homing();
                        lcd.cls();
                        lcd.locate(0,1);
                        lcd.printf("Homing concluido");
                        ThisThread::sleep_for(1s);
                        drawMainMenu();
                        break;
                    case 1: // Movimentacao Manual
                        lcd.cls();
                        lcd.locate(0,0);
                        lcd.printf("Modo Manual");
                        lcd.locate(0,1);
                        lcd.printf("Back p/ sair");
                        backFlag = false;
                        while (!backFlag) {
                            Pipetadora_ManualControl();
                        }
                        backFlag = false;
                        drawMainMenu();
                        break;
                    case 2: // Pipetadora submenu
                        inSubmenu = true;
                        cursor = 0;
                        drawSubMenu();
                        break;
                }
            } else {
                // ação placeholder no submenu
                lcd.cls();
                lcd.locate(0,1);
                lcd.printf("Selecionou: %s", subMenu[cursor]);
                ThisThread::sleep_for(1s);
                drawSubMenu();
            }
        }
        if (backFlag) {
            backFlag = false;
            if (inSubmenu) {
                inSubmenu = false;
                cursor = 2;  // volta em "Pipetadora"
                drawMainMenu();
            }
        }
        ThisThread::sleep_for(50ms);
    }
}
