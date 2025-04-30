#ifndef PIPETADORA_H
#define PIPETADORA_H

// inicializa GPIO, tickers e variáveis internas
void Pipetadora_InitMotors();

// executa a rotina de homing (referenciamento) dos eixos X e Y
void Pipetadora_Homing();

// laço de controle manual; deve ser chamado repetidamente
void Pipetadora_ManualControl();

#endif // PIPETADORA_H
