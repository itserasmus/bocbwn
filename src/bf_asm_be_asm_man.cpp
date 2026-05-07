#include <array>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

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


enum reg_size : uint8_t {
    byte_r,
    word_r,
    dword_r,
    quad_r
};

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
const array<const string, 15> DR = {
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
    && DR.size() == NUM_GPR
    && WR.size() == NUM_GPR
    && BR.size() == NUM_GPR, "invalid register name format");

const string RgeName(GPR reg, reg_size size) {
    return (size == byte_r ? BR : size == word_r ? WR : size == dword_r ? DR : QR)[reg];
};

enum AsmOpc : uint8_t {
    addx, mulx, movx, movzxq, call, cmpx, label, jz, jnz, jmp
};

struct Loc {
    enum { reg, imm, mem } type;
    union {
        GPR r;
        uint64_t i;
        struct {
            int32_t off;
            GPR head;
            GPR offset;
            uint8_t stride;
        };
    };
};

struct AsmInstr {
    AsmOpc opc;
    reg_size size;
    union {
        struct {
            Loc src;
            Loc dst;
        }; // addx, mulx, movx, movxzq, cmpx
        uint64_t uid; // call, label, jz, jnz, jmp
    };
};



class AsmMan {
public:
    struct Register;
    typedef void(*EvictFn)(Register*);
    struct Register {
        enum : uint8_t { unused, noble, peasant } type = unused;
        reg_size size;
        uint32_t priority;
        uint32_t last_used;
        Register** held = nullptr;  // where the user holds their copy
        EvictFn evict;

        uint64_t importance() const {
            return (uint64_t{priority} << 32) | last_used;
        }
    };
    
    uint32_t counter = 0; // increments by 1 each time an instruction is called

    vector<AsmInstr> asm_cmd;
    array<Register, 15> registers;

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


    /* We define two types of registers, Nobles, and peasants (peasant should never be capitalized
     * even if it starts a sentence).
     * Nobles live in the callee-saved registers. If the user attempts to allocate more Nobles than
     * the number of caller-saved registers, `RegMan` will throw a runtime error. Nobles can only
     * be deallocated by the user.
     * peasants can live in both the callee-saved registers or the caller-saved registers. peasants
     * which live in caller-saved registers are called serfs. peasants must provide a priority and
     * an eviction callback. 
     * Deallocation does not trigger the eviction callback.
     */
    
    static void noble_evict_fn(Register*) {
        cerr << "The aristocracy has fallen." << endl;
        exit(1);
    }


    constexpr inline bool is_callee_saved(const GPR reg) const {
        static_assert(NUM_GPR == 15, "Invalid GPR");
        return (0b111100001110010 & (COMPILE_FOR_SYS_V*0b111111111001111)) & (uint32_t{1} << reg);
    }
    constexpr inline bool is_caller_saved(const GPR reg) const {
        return (0b000011110001101 | (COMPILE_FOR_SYS_V*0b000000000110000)) & (uint32_t{1} << reg);
    }



    void allocate_noble(Register*& handle, EvictFn evict,  reg_size size = dword_r, GPR pref = NUM_GPR) {
        GPR reg = NUM_GPR;

        if(pref != NUM_GPR) { // try to put it in the preferred register
            if(registers[pref].type == Register::unused) {
                reg = pref;
                goto allocate_noble_end;
            }
        }
        for(GPR i : callee_saved) {
            if(registers[i].type == Register::unused) {
                reg = i;
                goto allocate_noble_end;
            }
        }
        // get rid of a peasant
        {
            int peasant_cesr_ind = -1;
            uint64_t peasant_cesr_imp = UINT64_MAX;
            for(GPR i : callee_saved) {
                Register& reg = registers[i];
                if(reg.type == Register::peasant) {
                    if(peasant_cesr_imp >= reg.importance()) {
                        peasant_cesr_ind = i;
                        peasant_cesr_imp = reg.importance();
                    }
                }
            }
            if(peasant_cesr_ind == -1) {
                goto allocate_noble_end; // error!
            }
            int peasant_crsr_ind = -1;
            uint64_t peasant_crsr_imp = UINT64_MAX;
            for(GPR i : caller_saved) {
                Register& reg = registers[i];
                if(reg.type == Register::peasant) {
                    if(peasant_crsr_imp >= reg.importance()) {
                        peasant_crsr_ind = i;
                        peasant_crsr_imp = reg.importance();
                    }
                }
            }
            if(peasant_crsr_imp < peasant_cesr_imp) {
                // move it there
                registers[peasant_crsr_ind].evict(&registers[peasant_crsr_ind]);
                registers[peasant_crsr_ind] = registers[peasant_cesr_ind];
                *registers[peasant_crsr_ind].held = &registers[peasant_crsr_ind];
                asm_cmd.push_back({
                    .opc = movx,
                    .size = registers[peasant_crsr_ind].size,
                    .src  = Loc{ Loc::reg, (GPR)peasant_cesr_ind },
                    .dst  = Loc{ Loc::reg, (GPR)peasant_crsr_ind }
                });
            } else {
                registers[peasant_cesr_ind].evict(&registers[peasant_cesr_ind]);
            }
        }

    allocate_noble_end:
        if(reg == NUM_GPR) {
            cerr << "Too many nobles; the aristicracy is doomed!" << endl;
            exit(1);
        }
        registers[reg] = Register{
            Register::noble, size, 0, ++counter, &handle, evict
        };
        handle = &registers[reg];
    }

    void allocate_peasant(Register*& handle, EvictFn evict,  reg_size size = dword_r, GPR pref = NUM_GPR, uint32_t priority = 0) {
        // can and will kick out lower or equal priority registers
        if(pref != NUM_GPR) {
            if(registers[pref].type == Register::unused) {
                registers[pref] = Register{
                    Register::peasant, size, priority, ++counter, &handle, evict
                };
                handle = &registers[pref];
                return;
            }
        }
        int free_cesr_ind = -1;
        int free_crsr_ind = -1;
        for(int i = 0; i < registers.size(); i++) {
            Register& reg = registers[i];
            if(reg.type == Register::unused) {
                if(is_callee_saved((GPR)i)) {
                    free_cesr_ind = i;
                    break;
                } else {
                    free_crsr_ind = i;
                }
            }
        }
        if(free_cesr_ind == -1 && free_crsr_ind == -1) {
            int least_imp_cesr_ind = -1;
            uint64_t least_imp_cesr_imp = UINT64_MAX;
            int least_imp_crsr_ind = -1;
            uint64_t least_imp_crsr_imp = UINT64_MAX;
            for(int i = 0; i < registers.size(); i++) {
                Register& reg = registers[i];
                if(reg.type == Register::peasant) {
                    uint64_t imp = reg.importance();
                    if(is_callee_saved((GPR)i)) {
                        if(least_imp_cesr_imp >= imp) {
                            least_imp_cesr_ind = i;
                            least_imp_cesr_imp = imp;
                        }
                    } else {
                        if(least_imp_crsr_imp >= imp) {
                            least_imp_crsr_ind = i;
                            least_imp_crsr_imp = imp;
                        }
                    }
                }
            }
            int mov_ind;
            if(least_imp_cesr_imp >> 32 <= priority && least_imp_cesr_ind != -1) {
                mov_ind = least_imp_cesr_ind;
            } else if(least_imp_crsr_imp >> 32 <= priority) {
                // least_imp_crsr_imp != -1 at this point, since there must be a least important peasant in
                // the caller-saved registers
                mov_ind = least_imp_crsr_ind;
            } else {
                handle = nullptr;
                return;
            }
            registers[mov_ind].evict(&registers[mov_ind]);
            registers[mov_ind] = Register{
                Register::peasant, size, priority, ++counter, &handle, evict
            };
            handle = &registers[mov_ind];
        } else {
            int free_ind = free_cesr_ind == -1 ? free_crsr_ind : free_cesr_ind;
            registers[free_ind] = Register{
                Register::peasant, size, priority, ++counter, &handle, evict
            };
            handle = &registers[free_ind];
        }
    }


    inline void deallocate_noble(Register* reg) {
        reg->type = Register::unused; // yep, literally that's it
    }
    
    inline void deallocate_peasant(Register* reg) {
        // don't call the eviction callback
        reg->type = Register::unused;
    }

    void evict_serfs() {
        for(GPR i : caller_saved) {
            if(registers[i].type != Register::unused) {
                registers[i].evict(&registers[i]);
                registers[i].type = Register::unused;
            }
        }
    }

    void dump_assembly(ofstream of) {
        // handles only the assembly inside the function, user must generate the boilerplate and labels
        // addx, mulx, movx, movzxq, call, cmpx, label, jz, jnz, jmp
        for(AsmInstr& ins : asm_cmd) {
            switch(ins.opc) {
                case addx:
                    
                    break;
            }
        }
    }
};




