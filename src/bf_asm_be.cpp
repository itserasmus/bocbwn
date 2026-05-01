#include "bf.hpp"
#include <array>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstdint>

using namespace std;


// #define COMPILE_FOR_SYS_V 1
#ifndef COMPILE_FOR_SYS_V
    #ifdef _WIN32
        #define COMPILE_FOR_SYS_V 0
    #else
        #define COMPILE_FOR_SYS_V 1
    #endif
#endif



class RegisterManager {
public:
    enum GPR {
        RAX,    RBX,    RCX,    RDX,    RSI,    RDI,    RBP, /*no*/
        R8,     R9,     R10,    R11,    R12,    R13,    R14,    R15,
    };
#if COMPILE_FOR_SYS_V
    static constexpr std::array<GPR, 9> caller_saved = {
        RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11
    };
    static constexpr std::array<GPR, 6> callee_saved = {
        RBX, RBP, R12, R13, R14, R15
    };
    static constexpr std::array<GPR, 6> argument_reg = {
        RDI, RSI, RDX, RCX, R8,  R9
    };
    static constexpr std::array<GPR, 1> return_reg = {
        RAX
    };
    
#else
    static constexpr std::array<GPR, 7> caller_saved = {
        RAX, RCX, RDX, R8, R9, R10, R11
    };
    static constexpr std::array<GPR, 8> callee_saved = {
        RBX, RBP, RDI, RSI, R12, R13, R14, R15
    };
    static constexpr std::array<GPR, 4> argument_reg = {
        RCX, RDX, R8,  R9
    };
    static constexpr std::array<GPR, 1> return_reg = {
        RAX
    };
#endif
};





int dump_assembly(string& output_file_name, vector<command>& commands, bool assume_no_overflow) {
    ofstream fo(output_file_name);

    if(!fo) {
        cerr << "Fatal: Failed to open output file " << output_file_name << endl;
        return 1;
    }

#if COMPILE_FOR_SYS_V
    static const char* const prologue = R"(.text
.globl run
.extern putchar
.extern getchar

run:
    # Prologue
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rdi, %rbx
    mov %rsi, %rbp
    xor %r14, %r14

)";
    static const char* const epilogue = R"(

    # Epilogue
.epilogue:
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    ret


.section .note.GNU-stack,"",@progbits
)";
#else
    static const char* const prologue = R"(.text
.globl run
.extern putchar
.extern getchar

run:
    # Prologue
    push %rbx
    push %rbp
    push %rdi
    push %rsi
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rcx, %rbx
    mov %rdx, %rbp
    xor %rdi, %rdi

)";
    static const char* const epilogue = R"(

    # Epilogue
.epilogue:
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rsi
    pop %rdi
    pop %rbp
    pop %rbx
    ret
)";
#endif
    
    string num;
    fo << prologue;
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
                fo << "    movzbl (%rbx,%r14), %edi\n    call putchar\n";
                break;
            case OUTC:
                fo << "    movl $" << cmd.aux << ", %edi\n    call putchar\n";
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
    addb %al, (%rbx,%rdi)\n";
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
    fo << epilogue;

    return 0;
}
