#pragma once

#include <cstdint>


struct command {
    uint8_t opc;
    int32_t aux;
    uint8_t f0;     // it's already probably 8 bytes due to padding,
    uint8_t f1;     // might as well use all 8 bytes
    uint8_t f2;
};

// all instructions except
// MOV, ADD, OUT, IN, BRZ, BRNZ, SET, PUTA, ACCUMA, MACMA, RKILL, OUTC, NOP,
// and HLT
// have been removed in this version. View the `main` branch for details on
// the full IR.
enum OP_CODES {
    // basic BF
    MOV,        // mov ptr
    ADD,        // add fixed value to cell
    OUT,        // output value as ascii
    IN,         // input ascii value
    BRZ,        // branch if zero
    BRNZ,       // branch if not zero
    SET,        // set value of cell
    OUTC,       // outputs an ASCII charater

    // register
    PUTA,       // cell -> A
    ACCUMA,     // cell + A -> cell
    MACMA,      // cel + A*val -> cell
    RKILL,      // put A in an undefined state
    
    // additional control flow
    NOP,        // do nothing
    HLT,        // halt program
};


#define has_four_byte_aux(c) \
    c == MOV    || c == ADD     || c == BRZ     || c == BRNZ    ||\
    c == SET    || c == MACMA   || c == OUTC

inline const char* const op_name(uint8_t opc) {
    static const char* const cmd_names[] = {
        "MOV     ", "ADD     ", "OUT     ", "IN      ", "BRZ     ", "BRNZ    ", "SET     ", "OUTC    ",
        "PUTA    ", "ACCUMA  ", "MACMA   ", "RKILL   ", "NOP     ", "HLT     ",
    };
    return cmd_names[opc];
}