#include "bf.hpp"
#include <array>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstdint>

using namespace std;

#ifndef COMPILE_FOR_SYS_V
    #ifdef _WIN32
        #define COMPILE_FOR_SYS_V 0
    #else
        #define COMPILE_FOR_SYS_V 1
    #endif
#endif
static_assert(COMPILE_FOR_SYS_V == 0 || COMPILE_FOR_SYS_V == 1, "COMPILE_FOR_SYS_V must be 0 or 1");

// if you ever change this, change everything related to it too... or just... don't.
enum GPR : uint8_t {
    RAX,    RBX,    RCX,    RDX,    RSI,    RDI,    RBP, /* we do not take its name in public */
    R8,     R9,     R10,    R11,    R12,    R13,    R14,    R15,
    NUM_GPR
};
const array<const string, 15> QR = {
    "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%rbp",
    "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
};
const array<const string, 15> LR = {
    "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "%ebp",
    "%r8d", "%r9d", "%r10d","%r11d","%r12d","%r13d","%r14d","%r15d"
};
const array<const string, 15> WR = {
    "%ax",  "%bx",  "%cx",  "%dx",  "%si",  "%di",  "%bp",
    "%r8w", "%r9w", "%r10w","%r11w","%r12w","%r13w","%r14w","%r15w"
};
const array<const string, 15> BR = {
    "%al",  "%bl",  "%cl",  "%dl",  "%sil", "%dil", "%bpl",
    "%r8b", "%r9b", "%r10b","%r11b","%r12b","%r13b","%r14b","%r15b"
};
static_assert(QR.size() == NUM_GPR
    && LR.size() == NUM_GPR
    && WR.size() == NUM_GPR
    && BR.size() == NUM_GPR, "invalid register name format");

typedef void(*EvictFn)(void*);

class RegMan {
public:
    // it would probably be cleaner to do this with only one #if-#else-#endif block, but my
    // clangd, for some reason, doesn't fold preprocessor blocks, and I want these to be foldable
    // so I'm defining them like this
    static constexpr array<GPR, 7+2*COMPILE_FOR_SYS_V> caller_saved = {
        RAX, RCX, RDX,
    #if COMPILE_FOR_SYS_V
        RSI, RDI,
    #endif
        R8, R9, R10, R11
    };
    static constexpr array<GPR, 8-2*COMPILE_FOR_SYS_V> callee_saved = {
        RBX, RBP,
    #if !COMPILE_FOR_SYS_V
        RSI, RDI,
    #endif
        R12, R13, R14, R15
    };
    static constexpr array<GPR, 4+2*COMPILE_FOR_SYS_V> argument_reg = {
    #if COMPILE_FOR_SYS_V
        RDI, RSI, RDX, RCX,
    #else
        RCX, RDX,
    #endif
        R8,  R9
    };
    static constexpr array<GPR, 1> return_reg = {
        RAX
    };

    /* We define two types of registers, Nobles, and peasants (peasants should never be capitalized
     * even if it starts a sentence).
     * Nobles live in the callee-saved registers. If the user attempts to allocate more Nobles than
     * the number of caller-saved registers, `RegMan` will throw a runtime error. Nobles can only
     * be deallocated by the user.
     * peasants can live in both the callee-saved registers or the caller-saved registers. peasants
     * which live in caller-saved registers are called serfs. peasants must provide a priority and
     * an eviction callback. 
     * Deallocation does not trigger the eviction callback.
     */
    
    static void noble_evict_fn(void*) {
        cerr << "The aristocracy has fallen." << endl;
        exit(1);
    }
    struct Register {
        GPR loc;
        bool noble;
        void* aux;
        EvictFn evict;
    };

    vector<GPR> free_callee_saved;
    vector<GPR> free_caller_saved;
    vector<Register> occupied_registers;

    inline bool is_callee_saved(GPR reg) {
        static_assert(NUM_GPR == 15, "Invalid GPR");
        return (0b111100001110010 & (COMPILE_FOR_SYS_V*0b111111111001111)) & (uint32_t{1} << reg);
    }
    inline bool is_caller_saved(GPR reg) {
        return (0b000011110001101 | (COMPILE_FOR_SYS_V*0b000000000110000)) & (uint32_t{1} << reg);
    }

    RegMan() {
        for(const GPR& reg : callee_saved) {
            free_callee_saved.push_back(reg);
        }
        for(const GPR& reg : caller_saved) {
            free_caller_saved.push_back(reg);
        }
    }

    Register* allocate_noble(GPR pref = NUM_GPR) {
        
    }

};



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
    RegMan rm;


    static_assert(RegMan::callee_saved.size() % 2 == 0, "number of callee saved registers is odd");
    for(GPR reg : RegMan::callee_saved) {
        fo << "    push " << QR[reg] <<"\n";
    }
#if !COMPILE_FOR_SYS_V
    fo << "    sub $32, %rsp # reserve shadow space\n";
#endif
    static_assert(RegMan::argument_reg.size() >= 2, "not enough argument registers");
    fo << "\n    mov " << QR[RegMan::argument_reg[0]] << ", " << QR[RBX]
        << "\n    mov " << QR[RegMan::argument_reg[1]] << ", " << QR[RBP]
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
    for(int i = RegMan::callee_saved.size()-1; i >= 0; i--) {
        fo << "    pop " << QR[RegMan::callee_saved[i]] <<"\n";
    }
    fo << "    ret\n";
#if COMPILE_FOR_SYS_V
    fo << "\n\n.section .note.GNU-stack,\"\",@progbits\n";
#endif

    return 0;
}
