#ifndef PINOS_H
#define PINOS_H

//======================================================================
//                     SENSORES DE FIM DE CURSO  
//======================================================================
#define FDC_YUP   PC_9     
#define FDC_YDWN  PC_6     
#define FDC_XUP   PC_10    
#define FDC_XDWN  D11      
#define FDC_ZUP   PC_2     
#define FDC_ZDWN  PB_12    

//======================================================================
//                   BOTÕES DE MOVIMENTAÇÃO MANUAL  
//======================================================================
#define BTN_XUP   PC_1     
#define BTN_XDWN  PA_11    
#define BTN_YUP   PB_14    
#define BTN_YDWN  PC_3     
#define BTN_ZUP   PB_1     
#define BTN_ZDWN  PA_15    

//======================================================================
//             BOTÕES DE CONTROLE E EMERGÊNCIA
//======================================================================
#define EMER_2    PB_15    // antes LCD_D6
#define BTN_ENTER PB_2     // antes PIPETA
#define BTN_CANCEL PB_11   // antes EMER_2

//======================================================================
//                           DISPLAY LCD 
//======================================================================
#define LCD_EN  PC_5
#define LCD_RS  D8        
#define LCD_D4  PC_12     
#define LCD_D5  PB_13     
#define LCD_D6  PA_12     
#define LCD_D7  D3        

//======================================================================
//                        CONTROLE DA PIPETA
//======================================================================
#define PIPETA   PC_0     

//======================================================================
//                        CONTROLE DOS MOTORES
//======================================================================
#define MOTOR_X  D9       
#define MOTOR_Y  D7       
#define MOTOR_Z  D4       

#define EN_X     D6       
#define EN_Y     PC_4     
#define EN_Z     D2       

#define DIR_X    PB_8     
#define DIR_Y    D5       
#define DIR_Z    PC_11    

#endif
