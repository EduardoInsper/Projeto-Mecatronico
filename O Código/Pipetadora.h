#ifndef PIPETADORA_H
#define PIPETADORA_H

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa GPIO, tickers e variáveis internas de motores
void Pipetadora_InitMotors(void);

// Executa rotina de homing (referenciamento) dos eixos X e Y
void Pipetadora_Homing(void);

// Loop de controle manual; deve ser chamado repetidamente até retornar
void Pipetadora_ManualControl(void);

// Retorna posição (em cm) do eixo especificado (0=X, 1=Y, 2=Z)
float Pipetadora_GetPositionCm(int id);

// Retorna posição (em passos) do eixo especificado (0=X, 1=Y, 2=Z)
int   Pipetadora_GetPositionSteps(int id);

#ifdef __cplusplus
}
#endif

#endif // PIPETADORA_H
