//PINOS PARA TESTES

#ifndef PINOS_H
#define PINOS_H

//======================================================================
// Sensores de fim de curso
//======================================================================
#define FDC_YUP   PA_6 
#define FDC_YDWN  PA_7 
#define FDC_XUP   PC_9
#define FDC_XDWN  PA_5 
#define FDC_ZUP   PC_8 
#define FDC_ZDWN  PC_6 

//======================================================================
// Botões de movimentação manual
//======================================================================
#define BTN_XUP   PC_10 
#define BTN_XDWN  PC_12 
#define BTN_YUP   PC_11 
#define BTN_YDWN  PD_2 
#define SWITCH_PIN PC_3

//======================================================================
// Botões de controle e emergência
//======================================================================
#define EMER_2    PC_0
#define BTN_ENTER PB_7
#define BTN_BACK  PC_2

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
#define MOTOR_X   PB_5  
#define MOTOR_Y   PC_4 
#define EN_X      PB_10 
#define EN_Y      PB_3 
#define DIR_X     PB_4 
#define DIR_Y     PA_10 

//======================================================================
// Controle direto de bobinas do eixo Z (MOSFETs)
// Substitua pelos pinos corretos de cada bobina no seu hardware
//======================================================================
#define Z_A1      PA_8 
#define Z_A2      PA_9 
#define Z_B1      PC_7 
#define Z_B2      PB_6 

#endif // PINOS_H;