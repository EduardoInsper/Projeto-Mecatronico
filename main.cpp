/******************************************************************************************
 *  Módulo isolado – Movimentação manual dos eixos + rotina de referenciamento (homing)
 *  Mantém apenas:
 *      • Estrutura Eixo (motor + fins-de-curso + botões de movimento)
 *      • Instâncias para X, Y e Z
 *      • Função referenciarEixos() que faz o homing sequencial Z→Y→X
 *  Todo o restante (LCD, menus, pipeta, etc.) foi removido.
 ******************************************************************************************/

#include "mbed.h"
#include "pinos.h"        // Mapeamento de pinos do hardware
#include <chrono>

using namespace std::chrono;
using namespace mbed;

//------------------------------------------------------------------------------------------------
// Constantes / flags globais
//------------------------------------------------------------------------------------------------
constexpr auto T_PULSO = 1ms;            // Velocidade do motor (1 kHz)
volatile bool habilitarMovimentos = true; // Travamento geral (emergência)

//------------------------------------------------------------------------------------------------
// Estrutura de controle de um eixo
//------------------------------------------------------------------------------------------------
struct Eixo {
    // Pinos
    InterruptIn  fdcTopo;
    InterruptIn  fdcBase;
    InterruptIn  btnCima;
    InterruptIn  btnBaixo;
    DigitalOut   pulso;
    DigitalOut   enable;
    DigitalOut   dir;

    // Controle
    Ticker       ticker;
    volatile int passos  = 0;
    volatile bool permitirCima  = true;
    volatile bool permitirBaixo = true;

    Eixo(PinName pFdcTopo, PinName pFdcBase,
         PinName pBtnCima, PinName pBtnBaixo,
         PinName pPulso,   PinName pEnable, PinName pDir)
        : fdcTopo(pFdcTopo), fdcBase(pFdcBase),
          btnCima(pBtnCima), btnBaixo(pBtnBaixo),
          pulso(pPulso), enable(pEnable, 1), dir(pDir, 0)
    {
        // Fins de curso
        fdcTopo.rise(callback(this,&Eixo::atingiuTopo));
        fdcTopo.fall(callback(this,&Eixo::liberarTopo));
        fdcBase.rise(callback(this,&Eixo::atingiuBase));
        fdcBase.fall(callback(this,&Eixo::liberarBase));

        // Botões de movimento manual
        btnCima.fall(callback(this,&Eixo::moverCima));
        btnCima.rise(callback(this,&Eixo::parar));
        btnBaixo.fall(callback(this,&Eixo::moverBaixo));
        btnBaixo.rise(callback(this,&Eixo::parar));
    }

    // Geração de passo (1 pulsação = ½ passo físico)
    void geraPulso() {
        pulso = !pulso;
        if (pulso) passos += (dir ? 1 : -1);
    }

    // Movimento manual
    void moverCima()  { if (!permitirCima  || !habilitarMovimentos) return;
                         dir = 0; enable = 0;
                         ticker.attach(callback(this,&Eixo::geraPulso), T_PULSO); }
    void moverBaixo() { if (!permitirBaixo || !habilitarMovimentos) return;
                         dir = 1; enable = 0;
                         ticker.attach(callback(this,&Eixo::geraPulso), T_PULSO); }
    void parar()      { ticker.detach(); enable = 1; }

    // Eventos de fim-de-curso
    void atingiuTopo()  { parar(); permitirCima  = false; }
    void liberarTopo()  { permitirCima  = true;  }
    void atingiuBase()  { parar(); permitirBaixo = false; }
    void liberarBase()  { permitirBaixo = true;  }
};

//------------------------------------------------------------------------------------------------
// Instâncias dos três eixos
//------------------------------------------------------------------------------------------------
Eixo eixoX(FDC_XUP, FDC_XDWN, BTN_XUP, BTN_XDWN, MOTOR_X, EN_X, DIR_X);
Eixo eixoY(FDC_YUP, FDC_YDWN, BTN_YUP, BTN_YDWN, MOTOR_Y, EN_Y, DIR_Y);
Eixo eixoZ(FDC_ZUP, FDC_ZDWN, BTN_ZUP, BTN_ZDWN, MOTOR_Z, EN_Z, DIR_Z);

//------------------------------------------------------------------------------------------------
// Homing / Referenciamento: Z (top), Y (top), X (base)
//------------------------------------------------------------------------------------------------
void referenciarEixos()
{
    int passo = 0;
    bool concluido = false;

    while (!concluido) {
        switch (passo) {
            case 0: eixoZ.moverCima();  passo = 1; break;
            case 1: if (!eixoZ.permitirCima)  { eixoZ.passos = 0; passo = 2; } break;

            case 2: eixoY.moverCima();  passo = 3; break;
            case 3: if (!eixoY.permitirCima)  { eixoY.passos = 0; passo = 4; } break;

            case 4: eixoX.moverBaixo(); passo = 5; break;
            case 5: if (!eixoX.permitirBaixo) { eixoX.passos = 0; concluido = true; } break;
        }
        ThisThread::yield();   // Coopera com o RTOS
    }
}

//------------------------------------------------------------------------------------------------
// Exemplo mínimo de uso: homing + controle manual pelos botões
//------------------------------------------------------------------------------------------------
int main()
{
    referenciarEixos();   // Executa homing na inicialização

    while (true) {
        // Movimento manual já está amarrado aos botões
        ThisThread::yield();
    }
}
