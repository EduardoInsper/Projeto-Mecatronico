# Sistema de Pipetagem Automático – Versão Otimizada

## Descrição

Este projeto implementa um sistema de pipetagem automática em plataforma NUCLEO‑STM32 (mbed OS 6). Oferece modos de operação manual e automático, com controle de três eixos (X, Y e Z), atuador de válvula de pipeta, interface via display LCD e botões com tratamento de debounce e rotinas de homing.

## Autor

Eduardo Monteiro Rosa de Souza

## Plataforma-alvo

NUCLEO‑STM32 (mbed OS 6)

## Data

22 abr 2025

## Principais Melhorias

* **Estrutura Eixo**: elimina código duplicado para X, Y e Z
* **Atualização mbed**: migração de mbed 2 para mbed OS 6 e substituição de `wait()` por `ThisThread::sleep_for()` com *chrono literals*
* **Comentários Adicionais**: documentação inline para facilitar manutenção
* **Funções Helper**: abstrações para LCD, posicionamento de cursor e *debounce* de botões

## Funcionalidades e Definições

### Pipetadora.h

* `Pipetadora_InitMotors()` – inicializa GPIO, tickers e variáveis dos motores e pipeta
* `Pipetadora_Homing()` – rotina de referenciamento dos eixos X, Y e Z
* `Pipetadora_MoveLinear(tx, ty)` – movimento linear combinado nos eixos X e Y até (tx, ty)
* `Pipetadora_MoveTo(id, targetSteps)` – movimento bloqueante de um eixo até passos definidos
* `Pipetadora_ActuateValve(volume_ml)` – acionamento bloqueante da válvula para aspirar ou dispensar líquido
* `Pipetadora_StopAll()` – para imediata de todos os movimentos (situação de emergência)
* `Pipetadora_ManualControl()` – loop de controle manual via botões
* `Pipetadora_GetPositionCm(id)` – retorna posição atual em centímetros
* `Pipetadora_GetPositionSteps(id)` – retorna posição atual em passos

### Pipetadora.cpp

* `enum MotorId { MotorX, MotorY, MotorCount }` – identificadores de eixos
* *ISRs* de passo (`stepISR`) e funções auxiliares para geração de pulso e atualização de posição
* Rotinas de movimentação: `Mover_Frente`, `Mover_Tras`, `Parar_Mov` por eixo
* Interpolação linear bidimensional via algoritmo de Bresenham: `stepLinearX`, `stepLinearY`
* Homing paralelo para X e Y (`HomingXY`) e homing dedicado para Z (`homingZ`)

### pinos.h

* Definições de pinos dos sensores de fim de curso (FDC), botões (*enter*, *back*, *emergência*), linha I²C e controle da pipeta
* Pinos dos drivers de passo para X, Y e bobinas diretas para Z

### main.cpp

* Configurações de hardware: I²C para LCD, interrupções para botões, *timer* para debounce
* Estrutura `Ponto` e arrays para armazenamento de coordenadas de coleta e soltura
* Handlers de *interrupt* para navegação de menu (*up*, *down*, *enter*, *back*) e emergência (*isrEmergPress*, *isrEmergRelease*)
* Menus gráficos no LCD: `drawMainMenuAnim()`, `drawMainMenu()`, `drawSubMenu()`
* Rotina principal (`main`) com lógica de seleção de modo, controle de pipetagem automática e tratamento de emergência

## Compilação e Execução

1. Instale o Mbed CLI v1 e importe o projeto.
2. Configure o alvo: `mbed target NUCLEO_F446RE`
3. Compile: `mbed compile -t GCC_ARM -m NUCLEO_F446RE`
4. Grave o binário na placa via USB.

## Licença

Este projeto está licenciado sob MIT.
