// main.cpp

#include "mbed.h"
#include "TextLCD.h"
#include "pinos.h"
#include "Pipetadora.h"
#define MAX_POINTS 9

DigitalIn switchSelectDisp(SWITCH_PIN, PullDown);

using namespace std::chrono;
using namespace std::chrono_literals;

// I2C LCD setup
I2C         i2c_lcd(D14, D15);
TextLCD_I2C lcd(&i2c_lcd, 0x7E, TextLCD::LCD20x4);

// Buttons
InterruptIn buttonUp   (BTN_XUP,   PullDown);
InterruptIn buttonDown (BTN_XDWN,  PullDown);
InterruptIn buttonEnter(BTN_ENTER, PullDown);
InterruptIn buttonBack (BTN_BACK,  PullDown);
InterruptIn buttonEmerg(EMER_2,    PullUp);

// Debounce timer
Timer debounceTimer;
const auto debounceTimeMs = 200ms;

// --- Estruturas para volumes e posições ---
typedef struct { int32_t pos[3]; } Ponto;
static Ponto pontosColeta;
static Ponto pontosSolta[MAX_POINTS];
static int  volumeSolta[MAX_POINTS] = {0};
// -----------------------------------------

static bool homed = false;
static int  cursor    = 0;
static bool inSubmenu = false;
static bool upFlag, downFlag, enterFlag, backFlag;
static int  numSolta  = 0;
static volatile bool emergActive = false;

// Menu definitions
#define MAIN_COUNT 3
#define SUB_COUNT  4
#define SUB_VISIBLE  3
const char* mainMenu[MAIN_COUNT] = { "Referenciamento", "Mov Manual", "Pipetadora" };
const char* subMenu[SUB_COUNT]  = { "Config Coleta", "Config Solta", "Reset Mem", "Iniciar" };

// ISR handlers
void isrEmergPress()   { emergActive = true; }
void isrEmergRelease() { emergActive = false; }
void isrUp()    { if (debounceTimer.elapsed_time() >= debounceTimeMs) { debounceTimer.reset(); upFlag    = true; } }
void isrDown()  { if (debounceTimer.elapsed_time() >= debounceTimeMs) { debounceTimer.reset(); downFlag  = true; } }
void isrEnter() { if (debounceTimer.elapsed_time() >= debounceTimeMs) { debounceTimer.reset(); enterFlag = true; } }
void isrBack()  { if (debounceTimer.elapsed_time() >= debounceTimeMs) { debounceTimer.reset(); backFlag  = true; } }

// Desenha menu inicial animado
void drawMainMenuAnim() {
    lcd.cls();
    lcd.locate(5,1); lcd.printf("Carregando...");
    thread_sleep_for(500);
    lcd.cls();
    lcd.locate(3,0); lcd.printf("MENU PRINCIPAL");
    for (int i = 0; i < MAIN_COUNT; ++i) {
        lcd.locate(1, i+1);
        lcd.printf("%s", mainMenu[i]);
    }
    cursor = 0; inSubmenu = false;
    lcd.locate(0,1); lcd.putc('>');
}

// Zera homing e pontos
void clearMemory() {
    homed = false;
    pontosColeta.pos[0] = pontosColeta.pos[1] = pontosColeta.pos[2] = 0;
    for (int i = 0; i < MAX_POINTS; ++i) {
        pontosSolta[i].pos[0] = pontosSolta[i].pos[1] = pontosSolta[i].pos[2] = 0;
        volumeSolta[i] = 0;
    }
    numSolta = 0;
}

// Desenha menu principal
void drawMainMenu() {
    lcd.cls();
    lcd.locate(3,0); lcd.printf("MENU PRINCIPAL");
    for (int i = 0; i < MAIN_COUNT; ++i) {
        lcd.locate(1, i+1);
        lcd.printf("%s", mainMenu[i]);
    }
    lcd.locate(0,1 + cursor); lcd.putc('>');
}

// Desenha submenu Pipetadora
void drawSubMenu() {
    lcd.cls();
    lcd.locate(3,0); lcd.printf("PIPETADORA");
    int windowStart = cursor < SUB_VISIBLE
                    ? 0
                    : (cursor > SUB_COUNT - SUB_VISIBLE
                       ? SUB_COUNT - SUB_VISIBLE
                       : cursor - (SUB_VISIBLE - 1));
    for (int i = 0; i < SUB_VISIBLE; ++i) {
        int idx = windowStart + i;
        lcd.locate(1, i+1); lcd.printf("%s", subMenu[idx]);
        if (idx == cursor) {
            lcd.locate(0, i+1); lcd.putc('>');
        }
    }
}

int main() {
    Pipetadora_InitMotors();
    debounceTimer.start();
    buttonUp.rise(&isrUp);
    buttonDown.rise(&isrDown);
    buttonEnter.rise(&isrEnter);
    buttonBack.rise(&isrBack);
    buttonEmerg.fall(&isrEmergPress);
    buttonEmerg.rise(&isrEmergRelease);
    drawMainMenuAnim();
    drawMainMenu();

    while (true) {
        // 1) Emergência
        if (emergActive) {
            Pipetadora_StopAll();
            lcd.cls(); lcd.printf("!!! EMERGENCIA !!!");
            // espera até o botão de emergência ser solto
            while (emergActive) ThisThread::sleep_for(50ms);
            // solicita confirmação de ENTER para voltar ao menu principal
            lcd.cls(); lcd.printf("Aperte ENTER");
            enterFlag = false;                          // zera flag de ENTER
            while (!enterFlag) ThisThread::sleep_for(50ms);
            enterFlag = false;                          // consome o ENTER
            clearMemory();
            cursor = 0; inSubmenu = false;
            drawMainMenu();
            continue;
        }


        // 2) Navegação
        if (!inSubmenu) {
            if (upFlag)   { cursor = (cursor-1+MAIN_COUNT)%MAIN_COUNT; drawMainMenu(); upFlag=false; }
            else if (downFlag) { cursor = (cursor+1)%MAIN_COUNT; drawMainMenu(); downFlag=false; }
            else if (backFlag) { backFlag=false; }
        } else {
            if (upFlag)   { cursor = (cursor-1+SUB_COUNT)%SUB_COUNT; drawSubMenu(); upFlag=false; }
            else if (downFlag) { cursor = (cursor+1)%SUB_COUNT; drawSubMenu(); downFlag=false; }
            else if (backFlag) { backFlag=false; inSubmenu=false; cursor=0; drawMainMenu(); }
        }

        // 3) Ação Enter
        if (enterFlag) {
            enterFlag = false;
            if (!inSubmenu) {
                switch (cursor) {
                    case 0: { // Referenciamento sempre em velocidade máxima (PERIODO_MINIMO interno)
                        lcd.cls(); lcd.printf("Referenciando...");
                        Pipetadora_Homing();
                        homed = true;
                        drawMainMenu();
                        break;
                    }

                    case 1: { // Movimento Manual
                        lcd.cls(); lcd.printf("Mov Manual");
                        bool lastSw = Pipetadora_GetToggleMode();
                        lcd.locate(17,0);
                        lcd.printf(lastSw ? "Z/Y" : "X/Y");
                        while (!backFlag && !emergActive) {
                            Pipetadora_ManualControl();
                            bool sw = Pipetadora_GetToggleMode();
                            if (sw != lastSw) {
                                lcd.locate(17,0);
                                lcd.printf(sw ? "Z/Y" : "X/Y");
                                lastSw = sw;
                            }
                        }
                        backFlag = false;
                        Pipetadora_StopAll();
                        drawMainMenu();
                        break;
                    }

                    case 2:  // Submenu Pipetadora
                        if (!homed) {
                            lcd.cls(); lcd.printf("Erro: Faca homing");
                            ThisThread::sleep_for(800ms);
                            drawMainMenu();
                        } else {
                            inSubmenu = true;
                            cursor = 0;
                            drawSubMenu();
                        }
                        break;
                }
            } else {
                switch (cursor) {
                    case 0: { // Config Coleta
                        lcd.cls(); lcd.printf("Posicione e Enter");
                        bool lastSw = Pipetadora_GetToggleMode();
                        lcd.locate(17,4);
                        lcd.printf(lastSw ? "Z/Y" : "X/Y");
                        while (!enterFlag && !backFlag && !emergActive) {
                            Pipetadora_ManualControl();
                            bool sw = Pipetadora_GetToggleMode();
                            if (sw != lastSw) {
                                lcd.locate(17,4);
                                lcd.printf(sw ? "Z/Y" : "X/Y");
                                lastSw = sw;
                            }
                        }
                        if (enterFlag) {
                            pontosColeta.pos[0] = Pipetadora_GetPositionSteps(0);
                            pontosColeta.pos[1] = Pipetadora_GetPositionSteps(1);
                            pontosColeta.pos[2] = Pipetadora_GetPositionSteps(2);
                            lcd.cls(); lcd.printf("Coleta Salvo");
                            ThisThread::sleep_for(500ms);
                        }
                        enterFlag = backFlag = false;
                        drawSubMenu();
                        break;
                    }

                    case 1: { // Config Solta
                        enterFlag = backFlag = false;
                        lcd.cls(); lcd.printf("Qtd Solta:%d", numSolta);
                        bool doneQtd = false;
                        while (!doneQtd && !emergActive) {
                            if (upFlag   && numSolta < MAX_POINTS) { numSolta++; lcd.cls(); lcd.printf("Qtd Solta:%d", numSolta); upFlag=false; }
                            if (downFlag && numSolta > 0)           { numSolta--; lcd.cls(); lcd.printf("Qtd Solta:%d", numSolta); downFlag=false; }
                            if (enterFlag) { enterFlag=false; doneQtd=true; }
                            if (backFlag)  { backFlag=false; doneQtd=true; numSolta=0; }
                            ThisThread::sleep_for(1ms);
                        }
                        for (int i = 0; i < numSolta; ++i) {
                            lcd.cls(); lcd.printf("Mov PtoS %d", i+1);
                            bool lastSw = Pipetadora_GetToggleMode();
                            lcd.locate(17,4);
                            lcd.printf(lastSw ? "Z/Y" : "X/Y");
                            while (!enterFlag && !backFlag && !emergActive) {
                                Pipetadora_ManualControl();
                                bool sw = Pipetadora_GetToggleMode();
                                if (sw != lastSw) {
                                    lcd.locate(17,4);
                                    lcd.printf(sw ? "Z/Y" : "X/Y");
                                    lastSw = sw;
                                }
                            }
                            if (!enterFlag) { backFlag=false; break; }
                            enterFlag = false;
                            pontosSolta[i].pos[0] = Pipetadora_GetPositionSteps(0);
                            pontosSolta[i].pos[1] = Pipetadora_GetPositionSteps(1);
                            pontosSolta[i].pos[2] = Pipetadora_GetPositionSteps(2);
                            volumeSolta[i] = 1;
                            bool doneVol = false;
                            while (!doneVol && !backFlag && !emergActive) {
                                lcd.cls(); lcd.printf("Vol Pto%d:%d mL", i+1, volumeSolta[i]);
                                if (upFlag)   { volumeSolta[i]++; upFlag=false; }
                                if (downFlag && volumeSolta[i]>1) { volumeSolta[i]--; downFlag=false; }
                                if (enterFlag) { enterFlag=false; doneVol=true; }
                                ThisThread::sleep_for(50ms);
                            }
                            lcd.cls(); lcd.printf("Salvo S%d", i+1);
                            ThisThread::sleep_for(300ms);
                        }
                        drawSubMenu();
                        break;
                    }

                    case 2: { // Reset Memória
                        clearMemory();
                        lcd.cls(); lcd.printf("Memória limpa");
                        ThisThread::sleep_for(500ms);
                        drawSubMenu();
                        break;
                    }

                    case 3: { // Iniciar Pipetagem
                        if (pontosColeta.pos[2]==0 && pontosColeta.pos[0]==0 && pontosColeta.pos[1]==0) {
                            lcd.cls(); lcd.printf("Erro: Coleta?");
                            ThisThread::sleep_for(800ms);
                            drawSubMenu();
                            break;
                        }
                        lcd.cls(); lcd.printf("Iniciando...");
                        ThisThread::sleep_for(300ms);
                        Pipetadora_MoveTo(2, 0);
                        ThisThread::sleep_for(50ms);
                        for (int j=0; j<numSolta && !emergActive; ++j) {
                            int needed = volumeSolta[j];
                            int done   = 0;
                            while (done < needed && !emergActive) {
                                // Aspirar
                                Pipetadora_MoveLinear(pontosColeta.pos[0], pontosColeta.pos[1]);
                                ThisThread::sleep_for(50ms);
                                Pipetadora_MoveTo(2, pontosColeta.pos[2]);
                                ThisThread::sleep_for(50ms);
                                Pipetadora_ActuateValve(1);
                                for (int t=0; t<2000; t+=50) {
                                    if (emergActive) break;
                                    ThisThread::sleep_for(50ms);
                                }
                                Pipetadora_MoveTo(2, 0);
                                ThisThread::sleep_for(50ms);
                                // Dispensar
                                Pipetadora_MoveLinear(pontosSolta[j].pos[0], pontosSolta[j].pos[1]);
                                ThisThread::sleep_for(50ms);
                                Pipetadora_MoveTo(2, pontosSolta[j].pos[2]);
                                ThisThread::sleep_for(50ms);
                                Pipetadora_ActuateValve(1);
                                for (int t=0; t<1200; t+=50) {
                                    if (emergActive) break;
                                    ThisThread::sleep_for(50ms);
                                }
                                Pipetadora_MoveTo(2, 0);
                                ThisThread::sleep_for(50ms);
                                ++done;
                            }
                            if (emergActive) break;
                        }
                        if (emergActive) break;
                        lcd.cls(); lcd.printf("Concluido");
                        ThisThread::sleep_for(800ms);
                        inSubmenu = false;
                        cursor    = 0;
                        drawMainMenu();
                        break;
                    }
                }
            }
        }

        ThisThread::sleep_for(50ms);
    }
}
