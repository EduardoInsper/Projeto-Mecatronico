#ifndef PINOS_H
#define PINOS_H

// Definição dos pinos para o controle dos motores e sensores
// MOTOR_X, MOTOR_Y e MOTOR_Z: pinos responsáveis por emitir pulsos para os motores
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

// Sensores de fim de curso (endstops) para cada eixo
#define FDC_YUP PC_11      // Sensor de fim de curso no sentido "para cima" do eixo Y
#define FDC_YDWN D11       // Sensor de fim de curso no sentido "para baixo" do eixo Y
#define FDC_XUP PC_9       // Sensor de fim de curso no sentido "para cima" do eixo X
#define FDC_XDWN PB_8      // Sensor de fim de curso no sentido "para baixo" do eixo X
#define FDC_ZUP PC_6       // Sensor de fim de curso no sentido "para cima" do eixo Z
#define FDC_ZDWN PC_10     // Sensor de fim de curso no sentido "para baixo" do eixo Z

// Botões para movimentação manual dos eixos
#define BTN_XUP PC_12
#define BTN_XDWN PC_2
#define BTN_YUP PC_3
#define BTN_YDWN PA_15
#define BTN_ZUP PC_0
#define BTN_ZDWN PC_1

// Botões de emergência e controle do sistema
#define EMER_2 PB_11       // Botão de emergência
#define BTN_ENTER PB_13    // Botão para selecionar/confirmar
#define BTN_CANCEL PB_1    // Botão para cancelar a ação

// Pinos do display LCD
#define LCD_EN PC_5
#define LCD_RS PA_12
#define LCD_D4 PA_11
#define LCD_D5 PB_12 
#define LCD_D6 PB_15
#define LCD_D7 PB_14

// Pino que controla a pipeta (acionamento do mecanismo de pipetagem)
#define PIPETA PB_2

#endif  // PINOS_H
