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
InterruptIn buttonEmerg(EMER_2,    PullDown);

// Debounce timer
Timer debounceTimer;
const uint32_t debounceTimeMs = 200;

// --- Novas estruturas para guardar volumes por ponto ---
typedef struct { int32_t pos[3]; } Ponto;
static Ponto pontosColeta;
static Ponto pontosSolta[MAX_POINTS];
static int  volumeSolta[MAX_POINTS] = {0};  // volume desejado em mL para cada ponto
// ---------------------------------------------------------

static bool homed = false;         // indica se homing já foi feito
static int  cursor         = 0;
static bool inSubmenu      = false;
static bool upFlag, downFlag, enterFlag, backFlag, emergFlag;
static int  numSolta       = 0;

// Menu definitions
#define MAIN_COUNT 3
#define SUB_COUNT  4
#define SUB_VISIBLE  3
const char* mainMenu[MAIN_COUNT] = { "Referenciamento", "Mov Manual", "Pipetadora" };
const char* subMenu[SUB_COUNT]  = { "Config Coleta", "Config Solta", "Reset Mem", "Iniciar" };

// ISR handlers
void isrEmerg() { emergFlag = true; }
void isrUp()    { if (debounceTimer.elapsed_time() >= 200ms) { debounceTimer.reset(); upFlag    = true; } }
void isrDown()  { if (debounceTimer.elapsed_time() >= 200ms) { debounceTimer.reset(); downFlag  = true; } }
void isrEnter() { if (debounceTimer.elapsed_time() >= 200ms) { debounceTimer.reset(); enterFlag = true; } }
void isrBack()  { if (debounceTimer.elapsed_time() >= 200ms) { debounceTimer.reset(); backFlag  = true; } }

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
    cursor = 0; inSubmenu = false;
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
    int windowStart = cursor < SUB_VISIBLE
                    ? 0
                    : (cursor > SUB_COUNT - SUB_VISIBLE
                       ? SUB_COUNT - SUB_VISIBLE
                       : cursor - (SUB_VISIBLE - 1));
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
    drawMainMenuAnim();
    drawMainMenu();

    while (true) {
        // 1) Verifica emergência
        if (emergFlag) {
            Pipetadora_StopAll(); lcd.cls(); lcd.printf("!!! EMERGÊNCIA !!!");
            while (!backFlag) ThisThread::sleep_for(50ms);
            emergFlag = backFlag = false; inSubmenu = false; cursor = 0;
            drawMainMenu(); continue;
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

        // 3) Ação ao Enter
        if (enterFlag) {
            enterFlag = false;
            if (!inSubmenu) {
                switch (cursor) {
                    case 0: // Referenciamento
                        lcd.cls(); lcd.printf("Referenciando..."); Pipetadora_Homing(); homed=true;
                        drawMainMenu(); break;
                    case 1: // Mov Manual
                        lcd.cls(); lcd.printf("Mov Manual");
                        // fica em manual até Back
                        while (!backFlag) Pipetadora_ManualControl();
                        backFlag = false;
                        // garante parada imediata de TODOS os motores
                        Pipetadora_StopAll();
                        drawMainMenu();
                        break;
                    case 2: // Pipetadora submenu
                        if (!homed) {
                            lcd.cls(); lcd.printf("Erro: Faça homing"); ThisThread::sleep_for(800ms);
                            drawMainMenu();
                        } else {
                            inSubmenu=true; cursor=0; drawSubMenu();
                        }
                        break;
                }
            } else {
                switch (cursor) {
                    case 0: { // Config Coleta
                        lcd.cls(); lcd.printf("Posicione e Enter");
                        while (!enterFlag && !backFlag) Pipetadora_ManualControl();
                        if (enterFlag) {
                            pontosColeta.pos[0] = Pipetadora_GetPositionSteps(0);
                            pontosColeta.pos[1] = Pipetadora_GetPositionSteps(1);
                            pontosColeta.pos[2] = Pipetadora_GetPositionSteps(2);
                            lcd.cls(); lcd.printf("Coleta Salvo"); ThisThread::sleep_for(500ms);
                        }
                        enterFlag=backFlag=false; drawSubMenu();
                        break;
                    }
                    case 1: { // Config Solta (pos + volume)
                        enterFlag = backFlag = false;
                        lcd.cls(); lcd.printf("Qtd Solta:%d", numSolta);
                        // 1) escolhe quantos pontos
                        bool doneQtd=false;
                        while (!doneQtd) {
                            if (upFlag   && numSolta<MAX_POINTS) { numSolta++; lcd.cls(); lcd.printf("Qtd Solta:%d", numSolta); upFlag=false; }
                            if (downFlag && numSolta>0)           { numSolta--; lcd.cls(); lcd.printf("Qtd Solta:%d", numSolta); downFlag=false; }
                            if (enterFlag) { enterFlag=false; doneQtd=true; }
                            if (backFlag)  { backFlag=false; doneQtd=true; numSolta=0; }
                            ThisThread::sleep_for(50ms);
                        }
                        // 2) para cada ponto, salva pos + ajusta volume
                        for (int i=0; i<numSolta; i++) {
                            // posição
                            lcd.cls(); lcd.printf("Mov PtoS %d", i+1);
                            while (!enterFlag) Pipetadora_ManualControl();
                            enterFlag=false;
                            pontosSolta[i].pos[0] = Pipetadora_GetPositionSteps(0);
                            pontosSolta[i].pos[1] = Pipetadora_GetPositionSteps(1);
                            pontosSolta[i].pos[2] = Pipetadora_GetPositionSteps(2);
                            // volume
                            volumeSolta[i] = 1;  // default
                            bool doneVol=false;
                            while (!doneVol) {
                                lcd.cls(); lcd.printf("Vol Pto%d:%d mL", i+1, volumeSolta[i]);
                                if (upFlag)   { volumeSolta[i]++; upFlag=false; }
                                if (downFlag && volumeSolta[i]>1) { volumeSolta[i]--; downFlag=false; }
                                if (enterFlag) { enterFlag=false; doneVol=true; }
                                ThisThread::sleep_for(50ms);
                            }
                            lcd.cls(); lcd.printf("Salvo S%d", i+1); ThisThread::sleep_for(300ms);
                        }
                        drawSubMenu();
                        break;
                    }
                    case 2: { // Reset Memória
                        // limpa coleta e soltura
                        pontosColeta.pos[0]=pontosColeta.pos[1]=pontosColeta.pos[2]=0;
                        for (int i=0; i<MAX_POINTS; ++i) {
                            pontosSolta[i].pos[0]=pontosSolta[i].pos[1]=pontosSolta[i].pos[2]=0;
                            volumeSolta[i]=0;
                        }
                        numSolta=0;
                        lcd.cls(); lcd.printf("Memória limpa"); ThisThread::sleep_for(500ms);
                        drawSubMenu();
                        break;
                    }
                    case 3: { // Iniciar Pipetagem
                        // validação
                        if (pontosColeta.pos[2]==0 && pontosColeta.pos[0]==0 && pontosColeta.pos[1]==0) {
                            lcd.cls(); lcd.printf("Erro: Coleta?"); ThisThread::sleep_for(800ms);
                            drawSubMenu();
                            break;
                        }
                        lcd.cls(); lcd.printf("Iniciando..."); ThisThread::sleep_for(300ms);

                        // Z → topo
                        Pipetadora_MoveTo(2, 0); ThisThread::sleep_for(50ms);

                        // Para cada ponto de soltura
                        for (int j=0; j<numSolta; j++) {
                            int needed = volumeSolta[j];
                            int done    = 0;
                            while (done < needed) {
                                // 1) aspirar 1 mL no ponto de coleta
                                Pipetadora_MoveLinear(pontosColeta.pos[0], pontosColeta.pos[1]);
                                ThisThread::sleep_for(50ms);
                                Pipetadora_MoveTo   (2, pontosColeta.pos[2]);
                                ThisThread::sleep_for(50ms);
                                Pipetadora_ActuateValve(1);  // aspira
                                ThisThread::sleep_for(50ms);
                                Pipetadora_MoveTo   (2, 0);  // sobe
                                ThisThread::sleep_for(50ms);

                                // 2) dispensar 1 mL no ponto j
                                Pipetadora_MoveLinear(pontosSolta[j].pos[0], pontosSolta[j].pos[1]);
                                ThisThread::sleep_for(50ms);
                                Pipetadora_MoveTo   (2, pontosSolta[j].pos[2]);
                                ThisThread::sleep_for(50ms);
                                Pipetadora_ActuateValve(1);  // dispensa
                                ThisThread::sleep_for(50ms);
                                Pipetadora_MoveTo   (2, 0);  // sobe
                                ThisThread::sleep_for(50ms);

                                done++;
                            }
                        }

                        lcd.cls(); lcd.printf("Concluido"); ThisThread::sleep_for(800ms);
                        inSubmenu = false; cursor = 0;
                        drawMainMenu();
                        break;
                    }
                }
            }
        }

        ThisThread::sleep_for(50ms);
    }
}