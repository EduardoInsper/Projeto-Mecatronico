// main.cpp

#include "mbed.h"
#include "TextLCD.h"
#include "pinos.h"
#include "Pipetadora.h"

using namespace std::chrono;

// I2C LCD setup
I2C         i2c_lcd(D14, D15);
TextLCD_I2C lcd(&i2c_lcd, 0x7E, TextLCD::LCD20x4);

// Buttons
InterruptIn buttonUp   (BTN_XUP,   PullDown);
InterruptIn buttonDown (BTN_XDWN,  PullDown);
InterruptIn buttonEnter(BTN_ENTER, PullDown);
InterruptIn buttonBack (BTN_BACK,  PullDown);

// Debounce timer
Timer       debounceTimer;
const uint32_t debounceTimeMs = 200;

// Menu state
bool           inSubmenu = false;
int            cursor    = 0;
volatile bool  upFlag    = false;
volatile bool  downFlag  = false;
volatile bool  enterFlag = false;
volatile bool  backFlag  = false;

// Menu definitions
#define MAIN_COUNT 3
#define SUB_COUNT  2
const char* mainMenu[MAIN_COUNT] = {
    "Referenciamento",
    "Mov Manual",
    "Pipetadora"
};
const char* subMenu[SUB_COUNT] = {
    "Placeholder A",
    "Placeholder B"
};

// ISR handlers
void isrUp() {
    if (duration_cast<milliseconds>(debounceTimer.elapsed_time()).count() >= debounceTimeMs) {
        debounceTimer.reset();
        upFlag = true;
    }
}
void isrDown() {
    if (duration_cast<milliseconds>(debounceTimer.elapsed_time()).count() >= debounceTimeMs) {
        debounceTimer.reset();
        downFlag = true;
    }
}
void isrEnter() {
    if (duration_cast<milliseconds>(debounceTimer.elapsed_time()).count() >= debounceTimeMs) {
        debounceTimer.reset();
        enterFlag = true;
    }
}
void isrBack() {
    if (duration_cast<milliseconds>(debounceTimer.elapsed_time()).count() >= debounceTimeMs) {
        debounceTimer.reset();
        backFlag = true;
    }
}

// Draw initial animated main menu
void drawMainMenuAnim() {
    lcd.cls();
    lcd.locate(5,1);      lcd.printf("Carregando...");
    thread_sleep_for(500);
    lcd.cls();
    lcd.locate(3,0);      lcd.printf("MENU PRINCIPAL");
    for (int i=0; i<MAIN_COUNT; ++i) {
        lcd.locate(1,i+1);
        lcd.printf("%s", mainMenu[i]);
    }
    cursor = 0;
    inSubmenu = false;
    lcd.locate(0,1); lcd.putc('>');
}

// Redraw main menu
void drawMainMenu() {
    lcd.cls();
    lcd.locate(3,0);      lcd.printf("MENU PRINCIPAL");
    for (int i=0; i<MAIN_COUNT; ++i) {
        lcd.locate(1,i+1);
        lcd.printf("%s", mainMenu[i]);
    }
    lcd.locate(0,1+cursor); lcd.putc('>');
}

// Draw Pipetadora submenu
void drawSubMenu() {
    lcd.cls();
    lcd.locate(3,0);      lcd.printf("PIPETADORA");
    for (int i=0; i<SUB_COUNT; ++i) {
        lcd.locate(1,i+1);
        lcd.printf("%s", subMenu[i]);
    }
    cursor = 0;
    lcd.locate(0,1); lcd.putc('>');
}

int main() {
    Pipetadora_InitMotors();

    debounceTimer.start();
    buttonUp.fall(&isrUp);
    buttonDown.fall(&isrDown);
    buttonEnter.fall(&isrEnter);
    buttonBack.fall(&isrBack);

    drawMainMenuAnim();

    while (true) {
        if (upFlag)    { upFlag=false;    if(cursor>0) --cursor;    if(inSubmenu) drawSubMenu(); else drawMainMenu(); }
        if (downFlag)  { downFlag=false;  int cnt = inSubmenu?SUB_COUNT:MAIN_COUNT; if(cursor<cnt-1) ++cursor; if(inSubmenu) drawSubMenu(); else drawMainMenu(); }
        if (backFlag)  { backFlag=false;  if(inSubmenu){ inSubmenu=false; cursor=2; drawMainMenu(); } }

        if (enterFlag) {
            enterFlag = false;
            if (!inSubmenu) {
                switch (cursor) {
                    case 0: // Homing
                        lcd.cls();
                        lcd.locate(0,1);
                        lcd.printf("Referenciando...");
                        Pipetadora_Homing();
                        lcd.cls();
                        lcd.locate(0,0);
                        lcd.printf("Referenciamento");
                        lcd.locate(0,1);
                        lcd.printf("Concluido");
                        thread_sleep_for(1000);
                        drawMainMenu();
                        break;

                    case 1: { // Manual
                        lcd.cls();
                        lcd.locate(0,0); lcd.printf("Modo Manual");
                        lcd.locate(0,1); lcd.printf("Back p/ sair");
                        backFlag = false;
                        while (!backFlag) {
                            int   xs = Pipetadora_GetPositionSteps(0);
                            int   ys = Pipetadora_GetPositionSteps(1);
                            float xc = Pipetadora_GetPositionCm   (0);
                            float yc = Pipetadora_GetPositionCm   (1);

                            // separar parte inteira e decimal de xc e yc
                            int xci = (int) xc;
                            int xcd = (int)((xc - xci)*10);
                            int yci = (int) yc;
                            int ycd = (int)((yc - yci)*10);

                            lcd.locate(0,2);
                            lcd.printf("X:%5d/%2d.%1dcm", xs, xci, xcd);
                            lcd.locate(0,3);
                            lcd.printf("Y:%5d/%2d.%1dcm", ys, yci, ycd);

                            Pipetadora_ManualControl();
                            thread_sleep_for(100);
                        }
                        backFlag = false;
                        drawMainMenu();
                        break;
                    }

                    case 2: // Submenu
                        inSubmenu = true; cursor = 0; drawSubMenu();
                        break;
                }
            } else {
                lcd.cls();
                lcd.locate(0,1);
                lcd.printf("Selecionou:%s", subMenu[cursor]);
                thread_sleep_for(1000);
                drawSubMenu();
            }
        }

        thread_sleep_for(50);
    }
}