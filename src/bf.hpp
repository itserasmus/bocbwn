#pragma once

#include <cstdint>
#include <vector>
#include <string>


using namespace std;


class command {
public:
    uint8_t opc;
    uint8_t aux;
    int32_t off;
    int32_t match; // for brz
    
    command()
        : opc(-1), aux(0), off(0) {}
    command(uint8_t opc)
        : opc(opc), aux(0), off(0) {}
    command(uint8_t opc, int32_t off)
        : opc(opc), aux(0), off(off) {}
    static inline command make_match(uint8_t opc, int32_t match, int32_t off = 0) {
        command m;
        m.opc = opc;
        m.off = off;
        m.match = match;
        return m;
    }
    command(uint8_t opc, uint8_t var, int32_t off)
        : opc(opc), aux(var), off(off) {}
};

// all instructions except
// MOV, ADD, OUT, IN, BRZ, BRNZ, SET, PUTA, ACCUMA, MACMA, RKILL, OUTC, NOP,
// and HLT
// have been removed in this version. View the `main` branch for details on
// the full IR.
enum OP_CODES : uint8_t {
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

constexpr bool uses_aux(uint8_t opc) {
    return opc == ADD || opc == SET || opc == OUTC || opc == MACMA;
}
constexpr bool uses_off(uint8_t opc) {
    return opc == MOV || opc == ADD || opc == OUT || opc == IN || opc == BRZ
        || opc == BRNZ || opc == SET || opc == PUTA || opc == ACCUMA || opc == MACMA;
}
constexpr bool uses_match(uint8_t opc) {
    return opc == BRZ || opc == BRNZ;
}


inline const char* op_name(uint8_t opc) {
    static const char* const cmd_names[] = {
        "MOV     ", "ADD     ", "OUT     ", "IN      ", "BRZ     ", "BRNZ    ", "SET     ", "OUTC    ",
        "PUTA    ", "ACCUMA  ", "MACMA   ", "RKILL   ", "NOP     ", "HLT     ",
    };
    return cmd_names[opc];
}



#ifndef NUKE_ON_ENTER
    #define NUKE_ON_ENTER true
#endif
#if NUKE_ON_ENTER
    #define nuke(str) cerr << str << endl; exit(1)
#else
    #define nuke(str) __builtin_unreachable()
#endif


int dump_assembly(string&, vector<command>&, bool);
int optimize(vector<command>&, bool);

