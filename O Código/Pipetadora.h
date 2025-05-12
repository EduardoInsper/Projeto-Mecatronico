#ifndef PIPETADORA_H
#define PIPETADORA_H

#ifdef __cplusplus
extern "C" {
#endif

// Inicialização e controle básico
void Pipetadora_InitMotors(void);
void Pipetadora_Homing(void);
void Pipetadora_ManualControl(void);
float Pipetadora_GetPositionCm(int id);
int   Pipetadora_GetPositionSteps(int id);

// Emergência
void  Pipetadora_EmergencyStop(void);

// Coleta e solta
void  Pipetadora_SetCollectionPoint(int x, int y, int z);
void  Pipetadora_GetCollectionPoint(int *x, int *y, int *z);
void  Pipetadora_AddDispensePoint(int x, int y, int z, int ml);
int   Pipetadora_GetDispenseCount(void);

// Seleção de volume
void  Pipetadora_SetML(int ml);
int   Pipetadora_GetML(void);

// Movimentos
void  Pipetadora_MoveToPosition(int axisId, int targetSteps);
void  Pipetadora_MoveInterpolated(int xTarget, int yTarget);

// Pipetagem automática
void  Pipetadora_ExecutePipetting(void);

#ifdef __cplusplus
}
#endif
#endif // PIPETADORA_H