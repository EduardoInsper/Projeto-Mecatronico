//======================================================================
//          Sistema de Pipetagem Automático – Versão Otimizada
//======================================================================
//  
//  Autor: Eduardo Monteiro Rosa de Souza
//  Plataforma‑alvo: NUCLEO‑STM32 (mbed OS 6)
//  Data: 22 abr 2025
//  
//  Principais melhorias:
//  • Estrutura Eixo: elimina código duplicado para X, Y e Z
//  • Substituição de wait() (depreciado) por ThisThread::sleep_for() e atualização do mbed 2 para mbed OS 6
//  • Uso de "chrono literals" para tempos
//  • Comentários adicionais
//  • Funções helper para LCD, cursor e debouncing
//======================================================================