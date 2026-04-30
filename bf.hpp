#pragma once

#include <cstdint>


struct command {
    uint8_t opc;
    int32_t aux;
    uint8_t f0;     // it's already probably 8 bytes due to padding,
    uint8_t f1;     // might as well use all 8 bytes
    uint8_t f2;
};
enum OP_CODES {
    // basic BF
    MOV,        // mov ptr
    ADD,        // add fixed value to cell
    OUT,        // output value as ascii
    IN,         // input ascii value
    BRZ,        // branch if zero
    BRNZ,       // branch if not zero
    SET,        // set value of cell

    // math
    INV,        // modular inverse of cell
    MULINV,     // multiply by modular inverse
    
    // registers
    PUTA,       // cell -> reg
    PULLA,      // reg  -> cell
    ACCUMA,     // cell + reg -> cell
    MACMA,      // cel + regA*val -> cell
    NCRAB,      // regA C val -> regB
    NCBAB,      // regA C regB -> regB
    MULRAB,     // reg + reg -> reg
    INVRA,      // reg -> reg
    PUTB,       // cell -> reg
    PULLB,      // reg  -> cell
    SWAP,       // swar reg
    ACCUMB,     // cell + reg -> cell
    MACMB,      // cel + regB*val -> cell
    MULRBA,     // reg + reg -> reg
    RKILL,      // put registers in an undefined state
    INVRB,      // reg -> reg

    OUTC,       // outputs an ASCII charater
    // additional control flow
    NOP,        // do nothing
    HLT,        // halt program
};


#define has_four_byte_aux(c) \
    c == MOV    || c == ADD     || c == BRZ     || c == BRNZ    ||\
    c == SET    || c == MULINV  || c == MACMA   || c == MACMB   ||\
    c == NCRAB  || c == OUTC

inline const char* const op_name(uint8_t opc) {
    static const char* const cmd_names[] = {
        "MOV     ", "ADD     ", "OUT     ", "IN      ", "BRZ     ", "BRNZ    ", "SET     ", "INV     ",
        "MULINV  ", "PUTA    ", "PULLA   ", "ACCUMA  ", "MACMA   ", "NCRAB   ", "NCBAB   ", "MULRAB  ",
        "INVRA   ", "PUTB    ", "PULLB   ", "SWAP    ", "ACCUMB  ", "MACMB   ", "MULRBA  ", "RKILL   ",
        "INVRB   ", "OUTC    ", "NOP     ", "HLT     ",
    };
    return cmd_names[opc];
}