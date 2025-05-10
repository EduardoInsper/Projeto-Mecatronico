#ifndef PINOS_H
#define PINOS_H

//======================================================================
// Sensores de fim de curso
//======================================================================
#define FDC_YUP   D11
#define FDC_YDWN  D12
#define FDC_XUP   D13
#define FDC_XDWN  PC_9
#define FDC_ZUP   PC_6
#define FDC_ZDWN  PC_8

//======================================================================
// Botões de movimentação manual
//======================================================================
#define BTN_XUP   PC_10
#define BTN_XDWN  PC_12
#define BTN_YUP   PC_11
#define BTN_YDWN  PD_2

//======================================================================
// Botões de controle e emergência
//======================================================================
#define EMER_2    PC_3
#define BTN_ENTER PA_13
#define BTN_BACK  PA_14
#define BTN_SWITCH PA_15

#define BTN_VELO1 PA_0
#define BTN_VELO2 PA_1
#define BTN_VELO3 PA_4

//======================================================================
// Display LCD I2C
//======================================================================
#define I2C_SDA       D14
#define I2C_SCL       D15
#define LCD_I2C_ADDR  0x27

//======================================================================
// Controle da pipeta
//======================================================================
#define PIPETA    PB_2

//======================================================================
// Motores X e Y (drivers de passo)
//======================================================================
#define EN_X      D6
#define DIR_X     D5
#define MOTOR_X   D4

#define EN_Y      D3
#define DIR_Y     D2
#define MOTOR_Y   PC_4


//======================================================================
// Controle direto de bobinas do eixo Z (MOSFETs)
// Substitua pelos pinos corretos de cada bobina no seu hardware
//======================================================================
#define Z_A1      D7
#define Z_A2      D8
#define Z_B1      D9
#define Z_B2      D10

#endif // PINOS_H