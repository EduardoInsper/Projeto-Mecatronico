//PINOS PARA TESTES

#ifndef PINOS_H
#define PINOS_H

//======================================================================
// Sensores de fim de curso
//======================================================================
#define FDC_YUP   D10
#define FDC_YDWN  D13
#define FDC_XUP   D12
#define FDC_XDWN  D11
#define FDC_ZUP   PC_6
#define FDC_ZDWN  PC_10

//======================================================================
// Botões de movimentação manual
//======================================================================
#define BTN_XUP   PC_12
#define BTN_XDWN  PC_2
#define BTN_YUP   PC_3
#define BTN_YDWN  PA_15
#define BTN_ZUP   PC_0
#define BTN_ZDWN  PC_1
#define SWITCH_PIN PB_1    // switch 0=Y / 1=Z

//======================================================================
// Botões de controle e emergência
//======================================================================
#define EMER_2    PA_4
#define BTN_ENTER PB_13
#define BTN_BACK  PB_14

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
#define MOTOR_X   D5
#define MOTOR_Y   D4
#define EN_X      PC_4
#define EN_Y      D2
#define DIR_X     D9
#define DIR_Y     D3

//======================================================================
// Controle direto de bobinas do eixo Z (MOSFETs)
// Substitua pelos pinos corretos de cada bobina no seu hardware
//======================================================================
#define Z_A1      D7
#define Z_A2      D6
#define Z_B1      D8
#define Z_B2      PC_8

#endif // PINOS_H

/*
PINOS FINAIS CORRETOS


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

*/