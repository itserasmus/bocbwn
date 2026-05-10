#include "bf.hpp"
#include "bf_asm_be_asm_man.cpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>


using namespace std;



int dump_assembly(string& output_file_name, vector<command>& commands, bool assume_no_overflow) {
    ofstream of(output_file_name);

    if(!of) {
        cerr << "Fatal: Failed to open output file " << output_file_name << endl;
        return 1;
    }

    of << R"(.text
.global run
.extern putchar
.extern getchar

run:
    # prologue
)";
    AsmMan am;


    static_assert(AsmMan::callee_saved.size() % 2 == 0, "number of callee saved registers is odd");
    for(GPR reg : AsmMan::callee_saved) {
        of << "    push " << QR[reg] <<"\n";
    }
    of << "    subq $" << (8+(1-COMPILE_FOR_SYS_V)*32) << ", %rsp\n"; // align stack + maybe allocate shadow space
    static_assert(AsmMan::argument_reg.size() >= 2, "not enough argument registers");
    of << "\n    movq " << QR[AsmMan::argument_reg[0]] << ", " << QR[RBX]
        << "\n    movq " << QR[AsmMan::argument_reg[1]] << ", " << QR[RBP]
        << "\n    xorq " << QR[R14] << ", " << QR[R14] << "\n\n";
    
    uint64_t putchar_name = create_function("putchar");
    uint64_t getchar_name = create_function("getchar");
    int32_t label_ctr = 0;
    for(command& cmd : commands) {
        switch(cmd.opc) {
            case MOV:
                am.push_instr(addx, quad_r, Loc(cmd.aux), Loc(R14));
                am.push_instr(andx, quad_r, Loc(RBP), Loc(R14));
                break;
            case ADD:
                am.push_instr(addx, byte_r, Loc(cmd.aux), Loc(0, RBX, R14));
                break;
            case OUT:
                am.push_instr(movzxq, byte_r, Loc(0, RBX, R14), Loc(AsmMan::argument_reg[0]));
                am.push_instr(call, putchar_name);
                break;
            case OUTC:
                am.push_instr(movx, dword_r, Loc(cmd.aux), Loc(AsmMan::argument_reg[0]));
                am.push_instr(call, putchar_name);
                break;
            case IN:
                am.push_instr(call, getchar_name);
                am.push_instr(movx, byte_r, Loc(AsmMan::return_reg[0]), Loc(0, RBX, R14));
                break;
            case BRZ:
                am.push_instr(cmpx, byte_r, Loc(0), Loc(0, RBX, R14));
                am.push_instr(jz, label_ctr + 1);
                am.push_instr(label, label_ctr);
                cmd.aux = label_ctr;
                label_ctr += 2;
                break;
            case BRNZ: {
                int32_t l_id = commands[cmd.aux].aux;
                am.push_instr(cmpx, byte_r, Loc(0), Loc(0, RBX, R14));
                am.push_instr(jnz, l_id);
                am.push_instr(label, l_id + 1);
                break;}
            case SET:
                am.push_instr(movx, byte_r, Loc(cmd.aux), Loc(0, RBX, R14));
                break;
            case PUTA:
                am.push_instr(movx, byte_r, Loc(0, RBX, R14), Loc(R12));
                break;
            case ACCUMA:
                am.push_instr(addx, byte_r, Loc(R12), Loc(0, RBX, R14));
                break;
            case MACMA:
                am.push_instr(imulx, byte_r, Loc(R12), Loc(RAX), cmd.aux, true);
                am.push_instr(addx, byte_r, Loc(RAX), Loc(0, RBX, R14));
                break;
            case RKILL:
                break;
            case NOP:
                break;
            case HLT:
                am.push_instr(jmp, UINT64_MAX);
                break;
            default:
                cout << "Unknown Opcode " << (int)cmd.opc << "\n";
                break;
        }
    }
    am.push_instr(label, UINT64_MAX);

    am.dump_assembly(of);

    of << "    addq $" << (8 + (1-COMPILE_FOR_SYS_V)*32) << ", %rsp\n";
    for(int i = AsmMan::callee_saved.size()-1; i >= 0; i--) {
        of << "    pop " << QR[AsmMan::callee_saved[i]] <<"\n";
    }
    of << "    ret\n";
#if COMPILE_FOR_SYS_V
    of << "\n\n.section .note.GNU-stack,\"\",@progbits\n";
#endif

    return 0;
}
