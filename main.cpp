/******************************************************************************************
 *  Núcleo mínimo – Movimentação manual dos eixos, homing e **emergência**.
 *  Mantém apenas:
 *      • Estrutura/instâncias dos 3 eixos (motores, fins-de-curso, botões)
 *      • Rotina de homing Z → Y → X
 *      • Botão de emergência (EMER_2) que trava/destrava todos os motores
 *
 *  PERSONALIZE:
 *      • Ajuste os pinos em pinos.h
 *      • Se quiser, chame referenciarEixos() novamente quando liberar a emergência
 ******************************************************************************************/

#include "mbed.h"
#include "pinos.h"          // Mapeamento de pinos do seu hardware
#include <chrono>

using namespace std::chrono;
using namespace mbed;

//------------------------------------------------------------------------------------------------
// Constantes e flags globais
//------------------------------------------------------------------------------------------------
constexpr auto T_PULSO = 1ms;                 // Periodo do ticker (velocidade do motor)
volatile bool habilitarMovimentos = true;     // Travamento global (emergência)

//------------------------------------------------------------------------------------------------
// Estrutura de um eixo
//------------------------------------------------------------------------------------------------
struct Eixo {
    InterruptIn  fdcTopo;
    InterruptIn  fdcBase;
    InterruptIn  btnCima;
    InterruptIn  btnBaixo;
    DigitalOut   pulso;
    DigitalOut   enable;
    DigitalOut   dir;

    Ticker       ticker;
    volatile int passos  = 0;
    volatile bool permitirCima  = true;
    volatile bool permitirBaixo = true;

    Eixo(PinName pFdcTopo, PinName pFdcBase,
         PinName pBtnCima, PinName pBtnBaixo,
         PinName pPulso,   PinName pEnable, PinName pDir)
        : fdcTopo(pFdcTopo), fdcBase(pFdcBase),
          btnCima(pBtnCima), btnBaixo(pBtnBaixo),
          pulso(pPulso), enable(pEnable, 1 /*desabilitado*/), dir(pDir, 0)
    {
        // Fins-de-curso
        fdcTopo.rise(callback(this,&Eixo::atingiuTopo));
        fdcTopo.fall(callback(this,&Eixo::liberarTopo));
        fdcBase.rise(callback(this,&Eixo::atingiuBase));
        fdcBase.fall(callback(this,&Eixo::liberarBase));

        // Botões manuais
        btnCima.fall(callback(this,&Eixo::moverCima));
        btnCima.rise(callback(this,&Eixo::parar));
        btnBaixo.fall(callback(this,&Eixo::moverBaixo));
        btnBaixo.rise(callback(this,&Eixo::parar));
    }

    // Pulso único
    void geraPulso()
    {
        pulso = !pulso;
        if (pulso)
            passos += (dir ? 1 : -1);
    }

    // Movimento manual
    void moverCima()
    {
        if (!permitirCima || !habilitarMovimentos) return;
        dir = 0; enable = 0;
        ticker.attach(callback(this,&Eixo::geraPulso), T_PULSO);
    }
    void moverBaixo()
    {
        if (!permitirBaixo || !habilitarMovimentos) return;
        dir = 1; enable = 0;
        ticker.attach(callback(this,&Eixo::geraPulso), T_PULSO);
    }
    void parar() { ticker.detach(); enable = 1; }

    // Fins-de-curso
    void atingiuTopo()  { parar(); permitirCima  = false; }
    void liberarTopo()  { permitirCima  = true;  }
    void atingiuBase()  { parar(); permitirBaixo = false; }
    void liberarBase()  { permitirBaixo = true;  }
};

//------------------------------------------------------------------------------------------------
// Instâncias dos eixos
//------------------------------------------------------------------------------------------------
Eixo eixoX(FDC_XUP, FDC_XDWN, BTN_XUP, BTN_XDWN, MOTOR_X, EN_X, DIR_X);
Eixo eixoY(FDC_YUP, FDC_YDWN, BTN_YUP, BTN_YDWN, MOTOR_Y, EN_Y, DIR_Y);
Eixo eixoZ(FDC_ZUP, FDC_ZDWN, BTN_ZUP, BTN_ZDWN, MOTOR_Z, EN_Z, DIR_Z);

//------------------------------------------------------------------------------------------------
// Funções de emergência
//------------------------------------------------------------------------------------------------
InterruptIn botaoEmerg(EMER_2);

void emergenciaOn()
{
    habilitarMovimentos = false;

    // Garante parada imediata dos motores
    eixoX.parar();
    eixoY.parar();
    eixoZ.parar();
}

void emergenciaOff()
{
    habilitarMovimentos = true;
    /* Se desejar, re-execute homing aqui:
       referenciarEixos();
    */
}

//------------------------------------------------------------------------------------------------
// Homing: Z topo → Y topo → X base
//------------------------------------------------------------------------------------------------
void referenciarEixos()
{
    // Z → topo
    eixoZ.moverCima();  while (eixoZ.permitirCima)  { ThisThread::yield(); }
    eixoZ.passos = 0;

    // Y → topo
    eixoY.moverCima();  while (eixoY.permitirCima)  { ThisThread::yield(); }
    eixoY.passos = 0;

    // X → base
    eixoX.moverBaixo(); while (eixoX.permitirBaixo) { ThisThread::yield(); }
    eixoX.passos = 0;
}

//------------------------------------------------------------------------------------------------
// MAIN
//------------------------------------------------------------------------------------------------
int main()
{
    // Configura botão de emergência
    botaoEmerg.rise(&emergenciaOn);
    botaoEmerg.fall(&emergenciaOff);

    referenciarEixos();   // Homing na partida

    while (true) {
        /* Movimento manual já está ligado aos botões.
           Nada a fazer aqui além de cooperar com o RTOS. */
        ThisThread::yield();
    }
}
