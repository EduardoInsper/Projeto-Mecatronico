#ifndef PIPETADORA_H
#define PIPETADORA_H

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa GPIO, tickers e motores X/Y/Z
void Pipetadora_InitMotors(void);

// Rotina de homing para Z, X e Y
void Pipetadora_Homing(void);

// Controle manual dos eixos. Alterna entre XY e Z conforme SWITCH_PIN
void Pipetadora_ManualControl(void);

// Retorna posição do eixo (0=X, 1=Y, 2=Z) em centímetros
float Pipetadora_GetPositionCm(int id);

// Retorna posição do eixo (0=X, 1=Y, 2=Z) em passos
int   Pipetadora_GetPositionSteps(int id);

// Parada de emergência: interrompe movimentos de X/Y e desativa Z
void  Pipetadora_EmergencyStop(void);

// Definição e leitura do ponto de coleta
void  Pipetadora_SetCollectionPoint(int x_steps, int y_steps, int z_steps);
void  Pipetadora_GetCollectionPoint(int *x_steps, int *y_steps, int *z_steps);

// Gerenciamento de pontos de dispensa (solta) e volume
void  Pipetadora_AddDispensePoint(int x_steps, int y_steps, int z_steps, int ml);
int   Pipetadora_GetDispenseCount(void);

// Seleção de volume em mL para cada ponto
void  Pipetadora_SetML(int ml);
int   Pipetadora_GetML(void);

// Movimentos de posicionamento
void  Pipetadora_MoveToPosition(int axisId, int target_steps);
void  Pipetadora_MoveInterpolated(int x_target, int y_target);

// Executa rotina completa: coleta + dispense automático
void  Pipetadora_ExecutePipetting(void);

#ifdef __cplusplus
}
#endif
#endif // PIPETADORA_H