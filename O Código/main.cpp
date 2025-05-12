#include "mbed.h"
#include "TextLCD.h"
#include "pinos.h"
#include "Pipetadora.h"

using namespace std::chrono;

// I2C LCD
I2C         i2c_lcd(I2C_SDA, I2C_SCL);
TextLCD_I2C lcd(&i2c_lcd, LCD_I2C_ADDR, TextLCD::LCD20x4);

// Botões de menu
InterruptIn btnUp   (BTN_XUP,   PullDown);
InterruptIn btnDown (BTN_XDWN,  PullDown);
InterruptIn btnEnter(BTN_ENTER, PullDown);
InterruptIn btnBack (BTN_BACK,  PullDown);
InterruptIn btnEmerg(EMER_2,    PullUp);

// Debounce
Timer       debounceTimer;
const uint32_t debounceMs = 200;
volatile bool upFlag=false, downFlag=false, enterFlag=false, backFlag=false, emergFlag=false;

// Menu state
bool inSubmenu = false;
int cursor = 0;
const int MAIN_COUNT = 3;
const int SUB_COUNT  = 3;
const char* mainMenu[MAIN_COUNT] = {"Referenciar","Manual","Pipetadora"};
const char* subMenu[SUB_COUNT]   = {"Ponto Coleta","Pontos Solta","Pipetagem"};

// Prototypes
void drawMainMenu();
void drawSubMenu();
void handleMainSelect();
void handleSubSelect();
void telaColeta();
void editaColeta();
void telaSolta();
void editaSolta();
void selecionarML();
void iniciarPipetagem();

// ISRs
void isrUp()    { if(debounceTimer.elapsed_time().count()>debounceMs){ debounceTimer.reset(); upFlag = true; } }
void isrDown()  { if(debounceTimer.elapsed_time().count()>debounceMs){ debounceTimer.reset(); downFlag = true; } }
void isrEnter() { if(debounceTimer.elapsed_time().count()>debounceMs){ debounceTimer.reset(); enterFlag = true; } }
void isrBack()  { if(debounceTimer.elapsed_time().count()>debounceMs){ debounceTimer.reset(); backFlag = true; } }
void isrEmerg() { emergFlag = true; }

int main() {
    debounceTimer.start();
    Pipetadora_InitMotors();

    btnUp.fall(&isrUp);
    btnDown.fall(&isrDown);
    btnEnter.fall(&isrEnter);
    btnBack.fall(&isrBack);
    btnEmerg.fall(&isrEmerg);

    drawMainMenu();
    while(true) {
        if (emergFlag) {
            Pipetadora_EmergencyStop();
            lcd.cls(); lcd.locate(0,0); lcd.printf("!!! EMERGENCIA !!!");
            thread_sleep_for(1000);
            emergFlag = false;
            drawMainMenu();
        }
        if (upFlag || downFlag || enterFlag || backFlag) {
            if (!inSubmenu) handleMainSelect(); else handleSubSelect();
            upFlag = downFlag = enterFlag = backFlag = false;
        }
        thread_sleep_for(50);
    }
}

void drawMainMenu() {
    lcd.cls(); lcd.locate(3,0); lcd.printf("MENU PRINCIPAL");
    for (int i = 0; i < MAIN_COUNT; ++i) {
        lcd.locate(1, i+1); lcd.printf("%s", mainMenu[i]);
    }
    cursor = 0;
    inSubmenu = false;
    lcd.locate(0,1); lcd.putc('>');
}

void drawSubMenu() {
    lcd.cls(); lcd.locate(3,0); lcd.printf("PIPETADORA");
    for (int i = 0; i < SUB_COUNT; ++i) {
        lcd.locate(1, i+1); lcd.printf("%s", subMenu[i]);
    }
    cursor = 0;
    inSubmenu = true;
    lcd.locate(0,1); lcd.putc('>');
}

void handleMainSelect() {
    if (upFlag && cursor > 0) --cursor;
    if (downFlag && cursor < MAIN_COUNT-1) ++cursor;
    drawMainMenu();
    if (enterFlag) {
        switch(cursor) {
            case 0:
                lcd.cls(); lcd.printf("Referenciando...");
                Pipetadora_Homing();
                thread_sleep_for(500);
                drawMainMenu();
                break;
            case 1:
                lcd.cls(); lcd.printf("Modo Manual");
                while(!backFlag) Pipetadora_ManualControl();
                backFlag = false;
                drawMainMenu();
                break;
            case 2:
                drawSubMenu();
                break;
        }
    }
}

void handleSubSelect() {
    if (upFlag && cursor > 0) --cursor;
    if (downFlag && cursor < SUB_COUNT-1) ++cursor;
    drawSubMenu();
    if (enterFlag) {
        switch(cursor) {
            case 0: telaColeta(); break;
            case 1: telaSolta();  break;
            case 2: iniciarPipetagem(); break;
        }
    }
    if (backFlag) {
        drawMainMenu();
        backFlag = false;
    }
}

void telaColeta() {
    int x,y,z;
    Pipetadora_GetCollectionPoint(&x, &y, &z);
    lcd.cls(); lcd.printf("Coleta: X=%d Y=%d", x, y);
    lcd.locate(0,1); lcd.printf("Z=%d", z);
    lcd.locate(0,2); lcd.printf("Enter p/ editar");
    while(!enterFlag && !backFlag) thread_sleep_for(50);
    if (enterFlag) editaColeta();
    if (backFlag) {
        drawSubMenu();
        backFlag = false;
    }
    enterFlag = false;
}

void editaColeta() {
    lcd.cls(); lcd.printf("Mover e Enter");
    while(!enterFlag) Pipetadora_ManualControl();
    int cx = Pipetadora_GetPositionSteps(0);
    int cy = Pipetadora_GetPositionSteps(1);
    int cz = Pipetadora_GetPositionSteps(2);
    Pipetadora_SetCollectionPoint(cx, cy, cz);
    lcd.cls(); lcd.printf("Coleta salvo");
    thread_sleep_for(500);
    drawSubMenu();
    enterFlag = false;
}

void telaSolta() {
    int n = Pipetadora_GetDispenseCount();
    lcd.cls(); lcd.printf("Pontos Solta: %d", n);
    lcd.locate(0,1); lcd.printf("Enter p/ editar");
    while(!enterFlag && !backFlag) thread_sleep_for(50);
    if (enterFlag) editaSolta();
    if (backFlag) {
        drawSubMenu();
        backFlag = false;
    }
    enterFlag = false;
}

void editaSolta() {
    int idx = Pipetadora_GetDispenseCount();
    lcd.cls(); lcd.printf("Ponto #%d: Mover", idx+1);
    while(!enterFlag) Pipetadora_ManualControl();
    int sx = Pipetadora_GetPositionSteps(0);
    int sy = Pipetadora_GetPositionSteps(1);
    int sz = Pipetadora_GetPositionSteps(2);
    lcd.cls(); lcd.printf("Sel ML");
    selecionarML();
    Pipetadora_AddDispensePoint(sx, sy, sz, Pipetadora_GetML());
    int newCount = Pipetadora_GetDispenseCount();
    lcd.cls(); lcd.printf("Salvo Pt %d", newCount);
    thread_sleep_for(500);
    drawSubMenu();
    enterFlag = false;
}

void selecionarML() {
    int ml = Pipetadora_GetML();
    lcd.cls(); lcd.printf("ML: %d", ml);
    while(true) {
        if(upFlag)    ++ml;
        if(downFlag)  --ml;
        if(ml < 1)   ml = 1;
        if(ml > 100) ml = 100;
        Pipetadora_SetML(ml);
        lcd.locate(0,1); lcd.printf("%d ml", ml);
        if (enterFlag) break;
        upFlag = downFlag = false;
        thread_sleep_for(100);
    }
    enterFlag = false;
}

void iniciarPipetagem() {
    lcd.cls(); lcd.printf("Pipetando...");
    Pipetadora_ExecutePipetting();
    lcd.cls(); lcd.printf("Concluido");
    thread_sleep_for(500);
    drawSubMenu();
}
