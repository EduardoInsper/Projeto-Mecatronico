// Pipetadora.h
#ifndef PIPETADORA_H
#define PIPETADORA_H

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa GPIO, tickers e variáveis internas de motores e pipeta
void Pipetadora_InitMotors(void);
// Executa rotina de homing (referenciamento) dos eixos X, Y e Z
void Pipetadora_Homing(void);
// Loop de controle manual; deve ser chamado repetidamente até retornar
void Pipetadora_ManualControl(void);
// Retorna posição (em cm) do eixo especificado (0=X, 1=Y, 2=Z)
float Pipetadora_GetPositionCm(int id);
// Retorna posição (em passos) do eixo especificado (0=X, 1=Y, 2=Z)
int   Pipetadora_GetPositionSteps(int id);
// Move o eixo (0=X,1=Y,2=Z) até a posição especificada em passos
void  Pipetadora_MoveTo(int id, int targetSteps);
// Aciona a válvula da pipeta para o volume em mL (bloqueante)
void  Pipetadora_ActuateValve(int volume_ml);
// Para imediatamente todos os movimentos e desativa bobinas (emergência)
void  Pipetadora_StopAll(void);

#ifdef __cplusplus
}
#endif

#endif // PIPETADORA_H