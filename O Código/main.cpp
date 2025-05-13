// main.cpp

#include "mbed.h"
#include "TextLCD.h"
#include "pinos.h"
#include "Pipetadora.h"
#define MAX_POINTS 9

DigitalIn switchSelectDisp(SWITCH_PIN, PullDown);

using namespace std::chrono;

// I2C LCD setup
I2C         i2c_lcd(D14, D15);
TextLCD_I2C lcd(&i2c_lcd, 0x7E, TextLCD::LCD20x4);

// Buttons
InterruptIn buttonUp   (BTN_XUP,   PullDown);
InterruptIn buttonDown (BTN_XDWN,  PullDown);
InterruptIn buttonEnter(BTN_ENTER, PullDown);
InterruptIn buttonBack (BTN_BACK,  PullDown);
InterruptIn buttonEmerg(EMER_2, PullDown);

// Debounce timer
Timer       debounceTimer;
const uint32_t debounceTimeMs = 200;

// Menu state
typedef struct { int32_t pos[3]; } Ponto;
static Ponto pontosColeta; 
static Ponto pontosSolta[MAX_POINTS];
static int  cursor         = 0;
static bool inSubmenu      = false;
static bool upFlag, downFlag, enterFlag, backFlag, emergFlag;
static int  numColeta=0, numSolta=0, volume_ml=1;

// Menu definitions
#define MAIN_COUNT 3
#define SUB_COUNT  4
#define SUB_VISIBLE  3
const char* mainMenu[MAIN_COUNT] = { "Referenciamento", "Mov Manual", "Pipetadora" };
const char* subMenu[SUB_COUNT]  = { "Config Coleta", "Config Solta", "Volume mL", "Iniciar" };

// ISR handlers

// ISR de emergência
void isrEmerg() { emergFlag = true; }

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
    lcd.locate(3,0);  lcd.printf("PIPETADORA");
    // calcula onde começa a janela de 3 itens para caber o cursor
    int windowStart = cursor < SUB_VISIBLE
                    ? 0
                    : (cursor > SUB_COUNT - 1 
                       ? SUB_COUNT - SUB_VISIBLE 
                       : cursor - (SUB_VISIBLE - 1));
    // desenha apenas SUB_VISIBLE itens
    for (int i = 0; i < SUB_VISIBLE; ++i) {
        int idx = windowStart + i;
        lcd.locate(1, i+1);
        lcd.printf("%s", subMenu[idx]);
        if (idx == cursor) {
            lcd.locate(0, i+1);
            lcd.putc('>');
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

    buttonEmerg.rise(&isrEmerg);
    Pipetadora_InitMotors();
    drawMainMenu();

    drawMainMenuAnim();

    while (true) {
        // 1) Emergência sempre tem prioridade
        if (emergFlag) {
            Pipetadora_StopAll();
            lcd.cls(); lcd.locate(0,0);
            lcd.printf("!!! EMERGÊNCIA !!!");
            while (!backFlag) { ThisThread::sleep_for(50ms); }
            backFlag  = false;
            emergFlag = false;
            inSubmenu = false;
            cursor    = 0;
            drawMainMenu();
            continue;
        }

        // 2) Navegação Up / Down / Back
        if (!inSubmenu) {
            if (upFlag) {
                cursor = (cursor - 1 + MAIN_COUNT) % MAIN_COUNT;
                drawMainMenu();
                upFlag = false;
            } else if (downFlag) {
                cursor = (cursor + 1) % MAIN_COUNT;
                drawMainMenu();
                downFlag = false;
            } else if (backFlag) {
                backFlag = false; // sem efeito no menu principal
            }
        } else {
            // submenu Pipetadora
            if (upFlag) {
                cursor = (cursor - 1 + SUB_COUNT) % SUB_COUNT;
                drawSubMenu();
                upFlag = false;
            } else if (downFlag) {
                cursor = (cursor + 1) % SUB_COUNT;
                drawSubMenu();
                downFlag = false;
            } else if (backFlag) {
                backFlag  = false;
                inSubmenu = false;
                cursor    = 0;
                drawMainMenu();
            }
        }

        // 3) Ação ao Enter — consumido só AQUI, após todos os ifs acima
        if (enterFlag) {
            if (!inSubmenu) {
                // Menu principal
                switch (cursor) {
                    case 0:
                        lcd.cls(); lcd.printf("Referenciando...");
                        Pipetadora_Homing();
                        drawMainMenu();
                        break;
                    case 1:
                        lcd.cls(); lcd.printf("Mov Manual");
                        backFlag = false;
                        while (!backFlag) { Pipetadora_ManualControl(); }
                        backFlag = false;
                        drawMainMenu();
                        break;
                    case 2:
                        inSubmenu = true;
                        cursor    = 0;
                        drawSubMenu();
                        break;
                }
            } else {
                // Submenu Pipetadora
                switch (cursor) {
                    case 0: { // Config Coleta – único ponto
                        // 1) Esquece qualquer Enter/Back prévio
                        enterFlag = false;
                        backFlag  = false;
                        // 2) Pede ao usuário posicionar e apertar Enter
                        lcd.cls();
                        lcd.printf("Posicione e Enter");
                            // 3) Permite movimentação manual enquanto espera Enter ou Back
                        while (!enterFlag && !backFlag) {
                            Pipetadora_ManualControl();
                        }
                        // 4) Se foi Enter, grava somente este ponto
                        if (enterFlag) {
                            pontosColeta.pos[0] = Pipetadora_GetPositionSteps(0);
                            pontosColeta.pos[1] = Pipetadora_GetPositionSteps(1);
                            pontosColeta.pos[2] = Pipetadora_GetPositionSteps(2);
                            lcd.cls();
                            lcd.printf("Coleta Salvo");
                            ThisThread::sleep_for(500ms);
                        }
                        // 5) Limpa flags e volta ao submenu
                        enterFlag = backFlag = false;
                        drawSubMenu();
                        break;
                    }
                    case 1: { // Config Solta
                        enterFlag = false;
                        backFlag  = false;
                        bool done = false;
                        lcd.cls(); lcd.printf("Qtd Solta:%d", numSolta);
                        while (!done) {
                            if (backFlag) { backFlag = false; done = true; }
                            if (upFlag)   { if (numSolta < MAX_POINTS) numSolta++; lcd.cls(); lcd.printf("Qtd Solta:%d", numSolta); upFlag   = false; }
                            if (downFlag) { if (numSolta > 0)         numSolta--; lcd.cls(); lcd.printf("Qtd Solta:%d", numSolta); downFlag = false; }
                            if (enterFlag) {
                                enterFlag = false;
                                for (int i = 0; i < numSolta; i++) {
                                    lcd.cls(); lcd.printf("Mov PtoS %d", i+1);
                                    while (!enterFlag) {
                                        // permite mover manualmente os eixos enquanto espera o Enter
                                        Pipetadora_ManualControl();
                                    }
                                    enterFlag = false;
                                    pontosSolta[i].pos[0] = Pipetadora_GetPositionSteps(0);
                                    pontosSolta[i].pos[1] = Pipetadora_GetPositionSteps(1);
                                    pontosSolta[i].pos[2] = Pipetadora_GetPositionSteps(2);
                                    lcd.cls(); lcd.printf("Salvo S%d", i+1);
                                    ThisThread::sleep_for(500ms);
                                }
                                done = true;
                            }
                            ThisThread::sleep_for(50ms);
                        }
                        drawSubMenu();
                        break;
                    }
                    case 2: { // Volume mL
                        enterFlag = false;
                        backFlag  = false;
                        bool done = false;
                        lcd.cls(); lcd.printf("Vol:%d mL", volume_ml);
                        while (!done) {
                            if (backFlag)  { backFlag = false;  done = true; }
                            if (upFlag)   { volume_ml++;                     lcd.cls(); lcd.printf("Vol:%d mL", volume_ml); upFlag   = false; }
                            if (downFlag) { if (volume_ml > 1) volume_ml--;  lcd.cls(); lcd.printf("Vol:%d mL", volume_ml); downFlag = false; }
                            if (enterFlag) { enterFlag = false; done = true; }
                            ThisThread::sleep_for(50ms);
                        }
                        drawSubMenu();
                        break;
                    }
                    case 3: { // Iniciar Pipetagem
                        enterFlag = false;
                        backFlag  = false;
                        lcd.cls(); lcd.printf("Iniciando..."); ThisThread::sleep_for(500ms);
                        // Executa pipetagem no ponto único de coleta:
                        Pipetadora_MoveTo(0, pontosColeta.pos[0]);
                        Pipetadora_MoveTo(1, pontosColeta.pos[1]);
                        Pipetadora_MoveTo(2, pontosColeta.pos[2]);
                        Pipetadora_ActuateValve(volume_ml);
                        Pipetadora_MoveTo(2, 0);
                        
                        // Executa soltura em cada ponto configurado:
                        for (int j = 0; j < numSolta; j++) {
                            Pipetadora_MoveTo(0, pontosSolta[j].pos[0]);
                            Pipetadora_MoveTo(1, pontosSolta[j].pos[1]);
                            Pipetadora_MoveTo(2, pontosSolta[j].pos[2]);
                            Pipetadora_ActuateValve(volume_ml);
                            Pipetadora_MoveTo(2, 0);
                        }
                        lcd.cls(); lcd.printf("Concluido"); ThisThread::sleep_for(1000ms);
                        inSubmenu = false;
                        cursor    = 0;
                        drawMainMenu();
                        break;
                    }
                }
            }
            enterFlag = false;  // consome o Enter aqui
        }

        ThisThread::sleep_for(50ms);  // debounce / processamento
    }
}