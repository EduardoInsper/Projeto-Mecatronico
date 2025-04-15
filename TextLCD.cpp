#include "TextLCD.h"
#include "mbed.h"
#include "rtos/ThisThread.h"
#include <chrono>

using namespace std::chrono;
using namespace std::chrono_literals;

TextLCD::TextLCD(PinName rs, PinName e, PinName d4, PinName d5,
                 PinName d6, PinName d7, LCDType type) : _rs(rs),
        _e(e), _d(d4, d5, d6, d7),
        _type(type) {

    _e  = 1;
    _rs = 0;            // Modo comando

    // Aguarda 15ms para garantir que a alimentação estabilizou
    ThisThread::sleep_for(15ms);

    // Envia "Display Settings" 3 vezes (apenas o nibble superior de 0x30, pois usamos barramento de 4 bits)
    for (int i = 0; i < 3; i++) {
        writeByte(0x3);
        wait_us(1640);  // Esse comando demora aproximadamente 1.64ms
    }
    writeByte(0x2);     // Configura o display para 4-bit mode
    wait_us(40);        // Aguarda 40us (a maioria dos comandos demora 40us)

    writeCommand(0x28); // Função: Configuração do display (001 BW N F - -)
    writeCommand(0x0C);
    writeCommand(0x6);  // Configura o movimento do cursor e shift de display: 0000 01 CD S
    cls();
}

void TextLCD::character(int column, int row, int c) {
    int a = address(column, row);
    writeCommand(a);
    writeData(c);
}

void TextLCD::cls() {
    writeCommand(0x01); // Comando para limpar o display e posicionar o cursor em 0
    wait_us(1640);      // Comando leva cerca de 1.64ms para ser processado
    locate(0, 0);
}

void TextLCD::locate(int column, int row) {
    _column = column;
    _row = row;
}

int TextLCD::_putc(int value) {
    if (value == '\n') {
        _column = 0;
        _row++;
        if (_row >= rows()) {
            _row = 0;
        }
    } else {
        character(_column, _row, value);
        _column++;
        if (_column >= columns()) {
            _column = 0;
            _row++;
            if (_row >= rows()) {
                _row = 0;
            }
        }
    }
    return value;
}

int TextLCD::_getc() {
    return -1;
}

void TextLCD::writeByte(int value) {
    // Envia primeiro o nibble superior
    _d = value >> 4;
    wait_us(40); // Aguarda 40us
    _e = 0;
    wait_us(40);
    _e = 1;
    // Em seguida, envia o nibble inferior
    _d = value;  // equivalente a value >> 0
    wait_us(40);
    _e = 0;
    wait_us(40); // Aguarda para garantir a execução do comando
    _e = 1;
}

void TextLCD::writeCommand(int command) {
    _rs = 0;
    writeByte(command);
}

void TextLCD::writeData(int data) {
    _rs = 1;
    writeByte(data);
}

int TextLCD::address(int column, int row) {
    switch (_type) {
        case LCD20x4:
            switch (row) {
                case 0:
                    return 0x80 + column;
                case 1:
                    return 0xc0 + column;
                case 2:
                    return 0x94 + column;
                case 3:
                    return 0xd4 + column;
            }
        case LCD16x2B:
            return 0x80 + (row * 40) + column;
        case LCD16x2:
        case LCD20x2:
        default:
            return 0x80 + (row * 0x40) + column;
    }
}

int TextLCD::columns() {
    switch (_type) {
        case LCD20x4:
        case LCD20x2:
            return 20;
        case LCD16x2:
        case LCD16x2B:
        default:
            return 16;
    }
}

int TextLCD::rows() {
    switch (_type) {
        case LCD20x4:
            return 4;
        case LCD16x2:
        case LCD16x2B:
        case LCD20x2:
        default:
            return 2;
    }
}
