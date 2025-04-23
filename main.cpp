#include "mbed.h"
#include "TextLCD.h" 
#include "pinos.h"           // Definição dos pinos utilizados
#include <chrono>

using namespace std::chrono;  // Permite usar 50ms, 1s, etc.
using namespace mbed;        // Facilita callback()

//======================================================================
//                     Constantes Globais
//======================================================================

constexpr auto T_PULSO      = 1ms;   // Intervalo entre pulsos do motor (velocidade)
constexpr auto T_DEBOUNCE   = 100ms; // Para botões
constexpr auto T_MSG_BRIEF  = 500ms; // Mensagens rápidas no LCD
constexpr auto T_MSG_LONG   = 1s;    // Mensagens mais longas no LCD

//======================================================================
//                 Variáveis de Estado & Utilidades
//======================================================================
volatile bool habilitarMovimentos = true;  // Travamento geral (emergência)
Timer        cronometro;                   // Para debounce

InterruptIn  botaoEnter(BTN_ENTER);
DigitalIn    botaoCancelar(BTN_CANCEL);
InterruptIn  botaoEmerg(EMER_2);
DigitalOut   sinalPipeta(PIPETA, 1);       // 1 = inativo

inline bool debounce() {
    if (cronometro.elapsed_time() > T_DEBOUNCE) {
        cronometro.reset();
        return true;
    }
    return false;
}

//======================================================================
//                         LCD (20×4)
//======================================================================
I2C i2c_bus(I2C_SDA, I2C_SCL);
TextLCD_I2C lcd(&i2c_bus, LCD_I2C_ADDR, TextLCD::LCD20x4);

inline void lcdLimpar() {
    lcd.cls();
}
inline void lcdMsg(uint8_t coluna, uint8_t linha, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lcd.locate(coluna, linha);
    lcd.vprintf(fmt, args);
    va_end(args);
}

//======================================================================
//                   Estrutura Genérica para os eixos
//======================================================================
struct Eixo {
    // --- Pinos ---
    InterruptIn  fdcTopo;
    InterruptIn  fdcBase;
    InterruptIn  btnCima;
    InterruptIn  btnBaixo;
    DigitalOut   pulso;
    DigitalOut   enable;
    DigitalOut   dir;
    // --- Controle ---
    Ticker       ticker;
    volatile int passos  = 0;     // Contador de passos para referência/posição
    volatile bool permitirCima  = true;
    volatile bool permitirBaixo = true;

    // Construtor
    Eixo(PinName pFdcTopo, PinName pFdcBase,
         PinName pBtnCima, PinName pBtnBaixo,
         PinName pPulso,    PinName pEnable, PinName pDir)
        : fdcTopo(pFdcTopo), fdcBase(pFdcBase),
          btnCima(pBtnCima), btnBaixo(pBtnBaixo),
          pulso(pPulso), enable(pEnable, 1 /*desabilitado*/), dir(pDir, 0) {

        // Interrupções Fim‑de‑Curso
        fdcTopo.rise(callback(this, &Eixo::atingiuTopo));  // acionamento
        fdcTopo.fall(callback(this, &Eixo::liberarTopo));  // liberou

        fdcBase.rise(callback(this, &Eixo::atingiuBase));
        fdcBase.fall(callback(this, &Eixo::liberarBase));

        // Interrupções Botões Manuais
        btnCima.fall(callback(this, &Eixo::moverCima));
        btnCima.rise(callback(this, &Eixo::parar));
        btnBaixo.fall(callback(this, &Eixo::moverBaixo));
        btnBaixo.rise(callback(this, &Eixo::parar));
    }

    // Geração de pulso
    void geraPulso() {
        pulso = !pulso;
        if (pulso) {
            passos += (dir ? 1 : -1);
        }
    }

    // ------------------ Movimento Manual ------------------
    void moverCima() {
        if (!permitirCima || !habilitarMovimentos) return;
        dir = 0; enable = 0; ticker.attach(callback(this, &Eixo::geraPulso), T_PULSO);
    }
    void moverBaixo() {
        if (!permitirBaixo || !habilitarMovimentos) return;
        dir = 1; enable = 0; ticker.attach(callback(this, &Eixo::geraPulso), T_PULSO);
    }
    void parar() {
        ticker.detach(); enable = 1;
    }

    // ------------------ Fim‑de‑Curso ------------------
    void atingiuTopo()  { parar(); permitirCima  = false; }
    void liberarTopo()  { permitirCima  = true;  }
    void atingiuBase()  { parar(); permitirBaixo = false; }
    void liberarBase()  { permitirBaixo = true;  }
};

//======================================================================
//                      Instâncias dos Eixos
//======================================================================
Eixo eixoX(FDC_XUP,  FDC_XDWN,  BTN_XUP,  BTN_XDWN,  MOTOR_X, EN_X, DIR_X);
Eixo eixoY(FDC_YUP,  FDC_YDWN,  BTN_YUP,  BTN_YDWN,  MOTOR_Y, EN_Y, DIR_Y);
Eixo eixoZ(FDC_ZUP,  FDC_ZDWN,  BTN_ZUP,  BTN_ZDWN,  MOTOR_Z, EN_Z, DIR_Z);

// Ponteiros de conveniência para acessar dinamicamente
Eixo* const eixos[3] = { &eixoX, &eixoY, &eixoZ };

//======================================================================
//                   Estruturas de Coordenadas
//======================================================================
struct Coordenada {
    int x{0}, y{0}, z{0}, ml{0};
};
Coordenada pontoColeta;          // Único ponto de coleta
Coordenada pontosSolta[9];       // Até 9 pontos de solta
int qtdPontosSolta = 1;          // Inicialmente 1

constexpr float PASSO_GRAUS = 1.8f;   // Passo do motor (graus)
constexpr float ROSCA_MM    = 1.8f;   // Passo do fuso (mm)
constexpr float FATOR_PASSO = ROSCA_MM / (360.0f / PASSO_GRAUS);

inline float passosParaMm(int p) { return p * FATOR_PASSO; }

//======================================================================
//                 Máquina de Estados – Definição
//======================================================================
enum class Estado {
    INICIO, CHECAR_REF, REFERENCIAR, MENU,
    TELA_COLETA, EDITAR_COLETA,
    TELA_SOLTA, PONTO_SOLTA, EDITAR_SOLTA, DEFINIR_ML,
    CICLO_PIPETA, PIPETA_COLETA, PIPETA_SOLTA
};
Estado estadoAtual = Estado::INICIO;
Estado estadoAnterior = Estado::INICIO;

// Cursor (menus)
uint8_t linhaCursor   = 1;  // Para menu vertical
uint8_t colCursor     = 4;  // Para menu horizontal (solta)

// Pipetagem
bool cicloEmAndamento = false;
int  indiceSolta      = 0;     // Índice do ponto atual
int  mlProcessados    = 0;

//======================================================================
//                    Tratamento de Emergência
//======================================================================
void emergenciaOn()  { habilitarMovimentos = false; }
void emergenciaOff() { habilitarMovimentos = true;  }

//======================================================================
//                  Helper – Espera não‑bloqueante breve
//======================================================================
inline void pausaBreve() { ThisThread::sleep_for(T_MSG_BRIEF); }
inline void pausaLonga() { ThisThread::sleep_for(T_MSG_LONG);  }

//======================================================================
//                      Funções de Pipeta
//======================================================================
void acionarPipeta() {
    sinalPipeta = 0;
    ThisThread::sleep_for(2s);
    sinalPipeta = 1;
}

//======================================================================
//                Funções de Navegação no Menu Principal
//======================================================================
void cursorMenuSubir() {
    if (linhaCursor > 1) {
        lcdMsg(0, linhaCursor, " ");
        linhaCursor--; lcdMsg(0, linhaCursor, ">");
    }
}
void cursorMenuDescer() {
    if (linhaCursor < 3) {
        lcdMsg(0, linhaCursor, " ");
        linhaCursor++; lcdMsg(0, linhaCursor, ">");
    }
}

//======================================================================
//             Callback Botão Enter (altera flag global)
//======================================================================
volatile bool flagEnter = false;
void enterPressionado() {
    if (debounce()) flagEnter = !flagEnter;
}

//======================================================================
//              Rotinas de Exibição – Reutilizáveis
//======================================================================
void mostrarMenu() {
    lcdLimpar();
    lcdMsg(0,0,"MENU");
    lcdMsg(1,1,"Ponto Coleta");
    lcdMsg(1,2,"Pontos Solta");
    lcdMsg(1,3,"Pipetar");
    linhaCursor = 1; lcdMsg(0,1,">");
}

void mostrarPontoColeta() {
    lcdLimpar();
    lcdMsg(0,0,"Ponto de Coleta");
    lcdMsg(1,1,"X=%1.1f Y=%1.1f Z=%1.1f", passosParaMm(pontoColeta.x), passosParaMm(pontoColeta.y), passosParaMm(pontoColeta.z));
    lcdMsg(1,2,"Editar");
}

void mostrarTelaSolta() {
    lcdLimpar();
    lcdMsg(2,0,"Pontos de Solta");
    lcdMsg(4,3,"^"); colCursor = 4;
    // Exibe números + '+' (adicionar ponto)
    lcdMsg(1,2,"                    ");
    lcd.locate(4,2);
    for(int i=0;i<qtdPontosSolta;i++) lcd.printf("%d", i+1);
    if (qtdPontosSolta<9) { lcd.printf("+"); }
}

//======================================================================
//                   Rotina Principal
//======================================================================
int main() {
    // Inicialização geral
    cronometro.start();
    botaoEmerg.rise(&emergenciaOn);
    botaoEmerg.fall(&emergenciaOff);
    botaoEnter.fall(&enterPressionado);

    lcdLimpar();
    lcdMsg(0,0,"Sistema iniciado"); pausaLonga();

    // Loop infinito
    while (true) {
        // Detecta transição de estado
        if (estadoAtual != estadoAnterior) {
            estadoAnterior = estadoAtual; // Atualiza referência
        }

        //------------------------------------------
        //              MAQUINA DE ESTADOS
        //------------------------------------------
        switch (estadoAtual) {
        case Estado::INICIO:
            lcdLimpar(); lcdMsg(0,0,"Iniciando..."); pausaBreve();
            estadoAtual = Estado::CHECAR_REF; break;

        case Estado::CHECAR_REF:
            lcdLimpar(); lcdMsg(4,1,"Referenciar?"); pausaBreve();
            if (flagEnter) { estadoAtual = Estado::REFERENCIAR; flagEnter = false; }
            break;

        case Estado::REFERENCIAR: {
            lcdLimpar(); lcdMsg(3,0,"Referenciando"); pausaBreve();
            // Sequência: Z topo, Y topo, X base
            static int passo = 0;
            switch (passo) {
                case 0: eixoZ.moverCima(); passo=1; break;
                case 1: if (!eixoZ.permitirCima) { eixoZ.passos=0; passo=2; } break;
                case 2: eixoY.moverCima(); passo=3; break;
                case 3: if (!eixoY.permitirCima) { eixoY.passos=0; passo=4; } break;
                case 4: eixoX.moverBaixo(); passo=5; break;
                case 5: if (!eixoX.permitirBaixo) { eixoX.passos=0; passo=6; } break;
                case 6: lcdLimpar(); lcdMsg(0,1,"Referência OK"); pausaBreve();
                        passo=0; estadoAtual=Estado::MENU; break;
            }
            break; }

        case Estado::MENU:
            if (estadoAnterior!=Estado::MENU) {
                // Adapta botões para navegar
                eixoZ.btnCima.fall(&cursorMenuSubir);
                eixoZ.btnBaixo.fall(&cursorMenuDescer);
                mostrarMenu();
            }
            // Seleção via ENTER
            if (flagEnter) {
                flagEnter=false;
                if (linhaCursor==1) estadoAtual=Estado::TELA_COLETA;
                else if (linhaCursor==2) estadoAtual=Estado::TELA_SOLTA;
                else estadoAtual=Estado::CICLO_PIPETA;
            }
            break;

        case Estado::TELA_COLETA:
            if (estadoAnterior!=Estado::TELA_COLETA) mostrarPontoColeta();
            if (flagEnter) { flagEnter=false; estadoAtual=Estado::EDITAR_COLETA; }
            if (!botaoCancelar.read()) { estadoAtual=Estado::MENU; }
            break;

        case Estado::EDITAR_COLETA: {
            // Habilita movimento manual (já configurado nos objetos)
            if (flagEnter) {
                // Salva nova coordenada
                pontoColeta.x=eixoX.passos; pontoColeta.y=eixoY.passos; pontoColeta.z=eixoZ.passos;
                flagEnter=false; estadoAtual=Estado::MENU; mostrarMenu();
            }
            if (!botaoCancelar.read()) { estadoAtual=Estado::TELA_COLETA; }
            break; }

        case Estado::TELA_SOLTA:
            if (estadoAnterior!=Estado::TELA_SOLTA) {
                // Ajusta botões para navegação horizontal
                eixoZ.btnCima.fall(NULL); eixoZ.btnBaixo.fall(NULL);
                eixoX.btnCima.fall([]{ if(colCursor<4+qtdPontosSolta) { lcdMsg(colCursor,3," "); colCursor++; lcdMsg(colCursor,3,"^"); } });
                eixoX.btnBaixo.fall([]{ if(colCursor>4) { lcdMsg(colCursor,3," "); colCursor--; lcdMsg(colCursor,3,"^"); } });
                mostrarTelaSolta();
            }
            if (flagEnter) {
                flagEnter=false;
                if (colCursor==4+qtdPontosSolta && qtdPontosSolta<9) { // '+' selecionado
                    qtdPontosSolta++; mostrarTelaSolta();
                } else {
                    indiceSolta = colCursor-4; estadoAtual=Estado::PONTO_SOLTA;
                }
            }
            if (!botaoCancelar.read()) { estadoAtual=Estado::MENU; mostrarMenu(); }
            break;

        case Estado::PONTO_SOLTA:
            if (estadoAnterior!=Estado::PONTO_SOLTA) {
                lcdLimpar();
                lcdMsg(0,0,"Ponto Solta %d", indiceSolta+1);
                auto &pt=pontosSolta[indiceSolta];
                lcdMsg(0,1,"X=%1.1f Y=%1.1f Z=%1.1f ml=%d", passosParaMm(pt.x), passosParaMm(pt.y), passosParaMm(pt.z), pt.ml);
                lcdMsg(1,2,"Editar");
            }
            if (flagEnter) { flagEnter=false; estadoAtual=Estado::EDITAR_SOLTA; }
            if (!botaoCancelar.read()) { estadoAtual=Estado::TELA_SOLTA; }
            break;

        case Estado::EDITAR_SOLTA: {
            if (flagEnter) {
                auto &pt=pontosSolta[indiceSolta];
                pt.x=eixoX.passos; pt.y=eixoY.passos; pt.z=eixoZ.passos;
                flagEnter=false; estadoAtual=Estado::DEFINIR_ML;
            }
            if (!botaoCancelar.read()) { estadoAtual=Estado::PONTO_SOLTA; }
            break; }

        case Estado::DEFINIR_ML: {
            static int mlTmp=0;
            if (estadoAnterior!=Estado::DEFINIR_ML) {
                lcdLimpar(); lcdMsg(3,1,"Quantos ml?: ");
                eixoZ.btnCima.fall([]{ ++mlTmp; });
                eixoZ.btnBaixo.fall([]{ if(mlTmp>0) --mlTmp; });
            }
            lcdMsg(16,1,"%2d", mlTmp);
            if (flagEnter) {
                pontosSolta[indiceSolta].ml = mlTmp; mlTmp=0; flagEnter=false;
                estadoAtual=Estado::TELA_SOLTA; mostrarTelaSolta();
            }
            if (!botaoCancelar.read()) { estadoAtual=Estado::TELA_SOLTA; }
            break; }

        //-----------------------------
        //     CICLO DE PIPETAGEM
        //-----------------------------
        case Estado::CICLO_PIPETA:
            cicloEmAndamento=true; indiceSolta=0; estadoAtual=Estado::PIPETA_COLETA; break;

        case Estado::PIPETA_COLETA: {
            lcdLimpar(); lcdMsg(1,1,"Coletando...");
            // 1. Sobe Z até topo
            eixoZ.moverCima(); while(eixoZ.permitirCima) { ThisThread::yield(); }
            // 2. Move X/Y para ponto de coleta
            eixoX.dir = (pontoColeta.x < eixoX.passos); eixoX.enable=0;
            eixoY.dir = (pontoColeta.y < eixoY.passos); eixoY.enable=0;
            // Simplesmente ajusta contadores direto (para reduzir código)
            eixoX.passos = pontoColeta.x; eixoY.passos = pontoColeta.y;
            eixoX.enable=1; eixoY.enable=1;
            // 3. Desce Z até profundidade de coleta
            eixoZ.dir=1; eixoZ.enable=0; eixoZ.passos = pontoColeta.z; eixoZ.enable=1;
            // 4. Aciona pipeta
            acionarPipeta();
            estadoAtual=Estado::PIPETA_SOLTA; break; }

        case Estado::PIPETA_SOLTA: {
            auto &pt = pontosSolta[indiceSolta];
            lcdLimpar(); lcdMsg(0,1,"Ponto %d – %dml", indiceSolta+1, pt.ml);
            // Sobe Z topo
            eixoZ.moverCima(); while(eixoZ.permitirCima) { ThisThread::yield(); }
            // Move p/ X/Y do ponto
            eixoX.passos = pt.x; eixoY.passos = pt.y;
            // Desce Z
            eixoZ.passos = pt.z;
            // Solta líquido
            for (mlProcessados=0; mlProcessados<pt.ml; ++mlProcessados) {
                acionarPipeta();
            }
            indiceSolta++;
            if (indiceSolta<qtdPontosSolta) estadoAtual=Estado::PIPETA_SOLTA; else { cicloEmAndamento=false; estadoAtual=Estado::MENU; mostrarMenu(); }
            break; }
        }
        ThisThread::yield();
    }
}
