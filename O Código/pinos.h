#ifndef PINOS_H
#define PINOS_H

//======================================================================
//                     SENSORES DE FIM DE CURSO  
//======================================================================
#define FDC_YUP D10      // Sensor de fim de curso no sentido "para cima" do eixo Y
#define FDC_YDWN D13      // Sensor de fim de curso no sentido "para baixo" do eixo Y
#define FDC_XUP D12    // Sensor de fim de curso no sentido "para cima" do eixo X         
#define FDC_XDWN D11     // Sensor de fim de curso no sentido "para baixo" do eixo X        
#define FDC_ZUP PC_6       // Sensor de fim de curso no sentido "para cima" do eixo Z
#define FDC_ZDWN PC_10     // Sensor de fim de curso no sentido "para baixo" do eixo Z

//======================================================================
//                   BOTÕES DE MOVIMENTAÇÃO MANUAL  
//======================================================================
#define BTN_XUP PC_12
#define BTN_XDWN PC_2
#define BTN_YUP PC_3
#define BTN_YDWN PA_15
#define BTN_ZUP PC_0
#define BTN_ZDWN PC_1 

//======================================================================
//             BOTÕES DE CONTROLE E EMERGÊNCIA
//======================================================================
#define EMER_2 PA_4      // Botão de emergência
#define BTN_ENTER PB_13    // Botão para selecionar/confirmar
#define BTN_BACK PB_14     // Botão para voltar no menu

//======================================================================
//                           DISPLAY LCD 
//======================================================================
#define I2C_SDA       D14      // PB_9  (SDA)
#define I2C_SCL       D15      // PB_8  (SCL)
#define LCD_I2C_ADDR  0x27     // Alterar para 0x3F se necessário

//======================================================================
//                        CONTROLE DA PIPETA
//======================================================================
#define PIPETA PB_2


//======================================================================
//                        CONTROLE DOS MOTORES
//======================================================================
#define MOTOR_X D5
#define MOTOR_Y D4
#define MOTOR_Z D7

// EN_X, EN_Y e EN_Z: pinos para habilitar/desabilitar os drivers dos motores
#define EN_X PC_4
#define EN_Y D2
#define EN_Z D6

// DIR_X, DIR_Y e DIR_Z: pinos que determinam o sentido de rotação dos motores
#define DIR_X D9
#define DIR_Y D3
#define DIR_Z D8

#endif
