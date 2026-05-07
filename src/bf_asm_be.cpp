#include "bf.hpp"
#include "bf_asm_be_asm_man.cpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>


using namespace std;



int dump_assembly(string& output_file_name, vector<command>& commands, bool assume_no_overflow) {
    ofstream fo(output_file_name);

    if(!fo) {
        cerr << "Fatal: Failed to open output file " << output_file_name << endl;
        return 1;
    }

    fo << R"(.text
.global run
.extern putchar
.extern getchar

run:
    # prologue
)";
    AsmMan rm;


    static_assert(AsmMan::callee_saved.size() % 2 == 0, "number of callee saved registers is odd");
    for(GPR reg : AsmMan::callee_saved) {
        fo << "    push " << QR[reg] <<"\n";
    }
#if !COMPILE_FOR_SYS_V
    fo << "    sub $32, %rsp # reserve shadow space\n";
#endif
    static_assert(AsmMan::argument_reg.size() >= 2, "not enough argument registers");
    fo << "\n    mov " << QR[AsmMan::argument_reg[0]] << ", " << QR[RBX]
        << "\n    mov " << QR[AsmMan::argument_reg[1]] << ", " << QR[RBP]
        << "\n    xor " << QR[R14] << ", " << QR[R14] << "\n\n";
    
    string num;
    int32_t loop_id = 0;
    int32_t ncrab_id = 0;
    for(command& cmd : commands) {
        switch(cmd.opc) {
            case MOV:
                fo << "    addq $" << cmd.aux << ", %r14\n    andq %rbp, %r14\n";
                break;
            case ADD:
                fo << "    addb $" << cmd.aux << ", (%rbx,%r14)\n";
                break;
            case OUT:
                fo << "    movzbl (%rbx,%r14), %ecx\n    call putchar\n";
                break;
            case OUTC:
                fo << "    movl $" << cmd.aux << ", %ecx\n    call putchar\n";
                break;
            case IN:
                fo << "    call getchar\n    movb %al, (%rbx,%r14)\n";
                break;
            case BRZ:
                fo << "    cmpb $0, (%rbx,%r14)\n    jz .loop_end_" << loop_id
                    << "\n.loop_begin_" << loop_id << ":\n";
                cmd.aux = loop_id++;
                break;
            case BRNZ: {
                int32_t l_id = commands[cmd.aux].aux;
                fo << "    cmpb $0, (%rbx,%r14)\n    jnz .loop_begin_" << l_id
                    << "\n.loop_end_" << l_id << ":\n";
                break;}
            case SET:
                fo << "    movb $" << cmd.aux << ", (%rbx,%r14)\n";
                break;
            case PUTA:
                fo << "    movb (%rbx,%r14), %r12b\n";
                break;
            case ACCUMA:
                fo << "    addb %r12b, (%rbx,%r14)\n";
                break;
            case MACMA:
                fo << "    movb $" << cmd.aux << ", %al\n    mulb %r12b\n\
    addb %al, (%rbx,%r14)\n";
                break;
            case RKILL:
                break;
            case NOP:
                break;
            case HLT:
                fo << "    jmp .epilogue\n";
                break;
            default:
                cout << "Unknown Opcode " << (int)cmd.opc << "\n";
                break;
        }
    }
    
    fo << R"(
    # epilogue
.epilogue:
)";
#if !COMPILE_FOR_SYS_V
    fo << "    add $32, %rsp # free shadow space\n";
#endif
    for(int i = AsmMan::callee_saved.size()-1; i >= 0; i--) {
        fo << "    pop " << QR[AsmMan::callee_saved[i]] <<"\n";
    }
    fo << "    ret\n";
#if COMPILE_FOR_SYS_V
    fo << "\n\n.section .note.GNU-stack,\"\",@progbits\n";
#endif

    return 0;
}
