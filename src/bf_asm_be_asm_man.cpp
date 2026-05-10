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





static inline bool is_pow2_u64(uint64_t v) {
    return v && !(v & (v - 1));
}

static inline uint32_t log2_uint64(uint64_t v) { // nonzero input only
    return 63 - __builtin_clzll(v);
}

static inline uint32_t rem_pow2_uint64(uint64_t v) { // nonzero input only
    return v >> (__builtin_ctzll(v));
}

static inline uint32_t v2_uint64(uint64_t v) { // again, nonzero input only
    return __builtin_ctzll(v);
}





enum reg_size : uint8_t {
    byte_r,
    word_r,
    dword_r,
    quad_r
};

static inline reg_size promote_reg(reg_size reg, reg_size min) {
    return reg > min ? reg : min;
}
static inline uint64_t reg_mask(reg_size reg) {
    return reg == byte_r ? 0xFF : reg == word_r ? 0xFFFF : reg == dword_r ? 0xFFFFFFFF : 0xFFFFFFFFFFFFFFFF;
}

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

const string& RegName(GPR reg, reg_size size) {
    return (size == byte_r ? BR : size == word_r ? WR : size == dword_r ? DR : QR)[reg];
};

enum AsmOpc : uint8_t {
    addx, andx, imulx, movx, movzxq, call, cmpx, label, jz, jnz, jmp
};

struct Loc {
    enum { reg, imm, mem } type;
    union {
        GPR r;
        uint64_t i;
        struct {
            int32_t disp;
            GPR base;
            GPR index;
            uint8_t scale;
        };
    };
    
    Loc() {};
    explicit Loc(GPR reg_) : type(reg), r(reg_) {};
    explicit Loc(uint64_t imm_) : type(imm), i(imm_) {};
    Loc(int32_t disp, GPR base, GPR index = NUM_GPR, uint8_t scale = 1)
        : type(mem), disp(disp), base(base), index(index), scale(scale) {};
};


bool loc_equal(Loc a, Loc b) {
    if(a.type != b.type) {return false;}
    if(a.type == Loc::imm) {return a.i == b.i;}
    if(a.type == Loc::reg) {return a.r == b.r;}
    if(a.type == Loc::mem) {
        return a.disp == b.disp && a.base == b.base && a.index == b.index && a.scale == b.scale;
    }
    __builtin_unreachable();;
}


struct AsmInstr {
    AsmOpc opc;
    reg_size size;
    union {
        struct {
            Loc src;
            Loc dst;
            uint64_t imm; // for imulx
            bool three_op; // also for imulx
        }; // addx, imulx, movx, movxzq, cmpx
        uint64_t uid; // call, label, jz, jnz, jmp
    };
};

vector<string> asm_name_by_uid;

uint64_t create_function(const string& s) {
    asm_name_by_uid.push_back(s);
    return asm_name_by_uid.size() - 1;
}

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



    inline void push_instr(AsmOpc opc, reg_size size, const Loc& src, const Loc& dst = Loc(), uint64_t imm = 0,
        bool three_op = false, bool touch = true) {
        asm_cmd.push_back({
            .opc = opc,
            .size = size,
            .src = src,
            .dst = dst,
            .imm = imm,
            .three_op = three_op
        });
        // mark registers as touched
        if(touch && src.type == Loc::reg && src.r < NUM_GPR) { // NUM_GPR is a (empty) sentinel
            registers[src.r].last_used = ++counter; // if not allocated out, then last_used is garbage anyway
        }
        if(touch && dst.type == Loc::reg && dst.r < NUM_GPR) {
            registers[dst.r].last_used = ++counter; // if both src & dst are there, then dst gets a
                // higher "priority"
        }
    }
    inline void push_instr(AsmOpc opc, uint64_t uid) {
        asm_cmd.push_back({
            .opc = opc,
            .uid = uid
        });
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
                push_instr(
                    movx,
                    registers[peasant_crsr_ind].size,
                    Loc((GPR)peasant_cesr_ind),
                    Loc((GPR)peasant_crsr_ind),
                    0, false, false
                );
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

    const string loc_str(const Loc& loc, reg_size size) const {
        switch(loc.type) {
            case Loc::reg:
                return RegName(loc.r, size);
            case Loc::imm:
                return "$" + to_string(loc.i);
            case Loc::mem: {
                string out;
                if(loc.disp) {
                    out += to_string(loc.disp);
                }

                out += "(";
                out += RegName(loc.base, quad_r);
                if(loc.index != NUM_GPR) {
                    out += ",";
                    out += RegName(loc.index, quad_r);

                    if(loc.scale != 1) {
                        out += ",";
                        out += to_string(loc.scale);
                    }
                }
                out += ")";
                return out;
            }
        }
    }

    void dump_assembly(ofstream& of) {
        // handles only the assembly inside the function, user must generate the boilerplate and entrypoint
        // addx, imulx, movx, movzxq, call, cmpx, label, jz, jnz, jmp

        static const constexpr char suffix[] = "bwlq"; // since byte_r = 1, not 0

        string src;
        string dst;
        reg_size size;
        for(AsmInstr& ins : asm_cmd) {
            switch(ins.opc) {
                case addx: {
                    if(ins.src.type == Loc::imm && ins.src.imm == 0) {continue;}
                    size = ins.dst.type == Loc::mem ? ins.size : promote_reg(ins.size, dword_r);
                    of << "    add" << suffix[size] << " " << loc_str(ins.src, size)
                        << ", " << loc_str(ins.dst, size) << "\n";
                    break;}
                case andx: {
                    if(ins.src.type == Loc::imm && ins.src.imm == 0) {continue;}
                    size = ins.dst.type == Loc::mem ? ins.size : promote_reg(ins.size, dword_r);
                    of << "    and" << suffix[size] << " " << loc_str(ins.src, size)
                        << ", " << loc_str(ins.dst, size) << "\n";
                    break;}
                case imulx: {
                    // Tthe unholy offspring of lightning and death itself. Your only hope, hide, and pray it
                    // does not find you.
                    size = ins.dst.type == Loc::mem ? ins.size : promote_reg(ins.size, dword_r);
                    src = loc_str(ins.src, size);
                    dst = loc_str(ins.dst, size);   // imulx $imm, (mem), (mem) is illegal both in x86 and the IR

                    uint64_t mult_mask = reg_mask(ins.size);
                    uint64_t mult = ins.three_op ? ins.imm : ins.src.imm;
                    mult = mult & mult_mask;
                    uint64_t odd_mult = mult == 0 ? 0 : rem_pow2_uint64(mult); // rem_pow2_uint64 cannot accept 0
                    // one shl, one lea, two shl, or one lea then one shl, are cheaper than imul
                    // one mov and lea/shl+lea/shl may or may not be cheaper -> don't do it
                    if(!ins.three_op) {
                        if(ins.src.type != Loc::imm) {
                            of << "    imul" << suffix[size] << " " << src << ", " << dst << "\n";
                            continue;
                        }
                        if(mult == 0) {
                            if(ins.dst.type == Loc::mem) {
                                of << "    mov" << suffix[size] << " $0, " << dst << "\n";
                            } else {
                                of << "    xor" << suffix[size] << " " << dst << ", " << dst << "\n";
                            }
                        } else if(mult == 1) {
                            // kill on sight
                        } else if(mult == mult_mask) {
                            of << "    neg" << suffix[size] << " " << dst << "\n";
                        } else if(is_pow2_u64(mult)) {
                            of << "    shl" << suffix[size] << " $" << log2_uint64(mult) << ", " << dst << "\n";
                        } else if(ins.dst.type == Loc::reg && (odd_mult == 3 || odd_mult == 5 || odd_mult == 9)) {
                            // I could swear you had... ..._teeth!_
                            string tmp_dst = loc_str(ins.dst, quad_r);
                            of << "    lea (" << tmp_dst << "," << tmp_dst << "," << (odd_mult-1) << "), " << dst << "\n";
                            if(odd_mult != mult) {
                                of << "    shl" << suffix[size] << " $" << v2_uint64(mult) << ", " << dst << "\n";
                            }
                        } else if(ins.dst.type == Loc::reg
                            && (mult == 15 || mult == 25 || mult == 27 || mult == 45 || mult == 81)) {
                            // mult in {3,5,9}*{3,5,9}, since 2 is taken care of earlier with shl
                            int mul1 = (mult == 15 || mult == 27) ? 3 : (mult == 25 || mult == 45) ? 5 : 9;
                            int mul2 = (mult == 15 || mult == 25) ? 5 : 9;
                            dst = loc_str(ins.dst, quad_r);
                            of << "    lea (" << dst << "," << dst << "," << (mul1-1) << "), " << dst;
                            of << "\n    lea (" << dst << "," << dst << "," << (mul2-1) << "), " << dst << "\n";
                        } else {
                            of << "    imul" << suffix[size] << " " << src << ", " << dst << "\n";
                        }
                    } else {
                        if(ins.src.type == Loc::imm) {
                            of << "    mov" << suffix[size] << " $" // this is berk
                                << ((mult*ins.src.imm) & mult_mask) << ", " << dst;
                            continue;
                        }
                        if(mult == 0) {
                            if(ins.dst.type == Loc::mem) {
                                of << "    mov" << suffix[size] << " $0, " << dst << "\n";
                            } else {
                                of << "    xor" << suffix[size] << " " << dst << ", " << dst << "\n";
                            }
                        } else if(mult == 1) {
                            if(loc_equal(ins.src, ins.dst)) {continue;}
                            of << "    mov" << suffix[size] << " " << src << ", " << dst << "\n";
                        } else if(mult == mult_mask) {
                            if(loc_equal(ins.src, ins.dst)) {
                                of << "    neg" << suffix[size] << " " << dst << "\n";
                                continue;
                            }
                            if(ins.dst.type == Loc::reg) {
                                of << "    xor" << suffix[size] << " " << dst << ", " << dst // xor, sub better than mov,neg
                                    << "\n    sub" << suffix[size] << " " << src << ", " << dst << "\n"; // cuz xor = DAG breaker
                            } else {
                                of << "    mov" << suffix[size] << " " << src << ", " << dst
                                    << "\n    neg" << suffix[size] << dst << "\n";
                            }
                        } else if(is_pow2_u64(mult)) {
                            if(!loc_equal(ins.src, ins.dst)) {
                                if(ins.src.type == Loc::reg && ins.dst.type == Loc::reg
                                    && (mult == 2 || mult == 4 || mult == 8)) {
                                    // lea < mov+shl
                                    of << "    lea (," << loc_str(ins.src, quad_r) << "," << mult << "), " << dst << "\n";
                                    continue;
                                }
                                of << "    mov" << suffix[size] << " " << src << ", " << dst << "\n";
                            }
                            of << "    shl" << suffix[size] << " $" << log2_uint64(mult) << ", " << dst << "\n";
                        } else if(ins.dst.type == Loc::reg && ins.src.type == Loc::reg ?
                            (mult == 3 || mult == 5 || mult == 9) : (odd_mult == 3 || odd_mult == 5 || odd_mult == 9)) {
                            if(ins.src.type == Loc::reg) {
                                src = loc_str(ins.src, quad_r);
                                of << "    lea (" << src << "," << src << "," << (odd_mult-1)
                                    << "), " << dst << "\n";
                                if(mult != odd_mult) {
                                    of << "    shl" << suffix[size] << " $" << v2_uint64(mult) << ", " << dst << "\n";
                                }
                            } else {
                                of << "    mov" << suffix[size] << " " << src << ", " << dst << "\n";
                                dst = loc_str(ins.dst, quad_r);
                                of << "    lea (" << dst << "," << dst << "," << (mult-1)
                                    << "), " << dst << "\n";
                            }
                        } else if(ins.dst.type == Loc::reg && ins.src.type == Loc::reg &&
                            (mult == 15 || mult == 25 || mult == 27 || mult == 45 || mult == 81)) {
                            int mul1 = (mult == 15 || mult == 27) ? 3 : (mult == 25 || mult == 45) ? 5 : 9;
                            int mul2 = (mult == 15 || mult == 25) ? 5 : 9;
                            src = loc_str(ins.src, quad_r);
                            string tmp_dst = loc_str(ins.dst, quad_r);
                            of << "    lea (" << src << "," << src << "," << (mul1-1) << "), " << dst;
                            of << "\n    lea (" << tmp_dst << "," << tmp_dst << "," << (mul2-1) << "), " << dst << "\n";
                        } else if(ins.dst.type == Loc::reg && ins.src.type == Loc::reg && !loc_equal(ins.src, ins.dst)
                            && (mult == 7 || mult == 11 || mult == 13 || mult == 19 || mult == 21 || mult == 37
                            || mult == 41 || mult == 73)) {
                            // on -O1 and higher, reduced to a bitmask anyway
                            // 1 + (1+{1,2,4,8})*{1,2,4,8}
                            // = {3,4,5,6,7,9,10,11,13,17,19,21,25,37,41,73}
                            // => {7,11,13,17,19,21,37,41,73}
                            int mul1 = (mult == 7 || mult == 13) ? 3 : (mult == 11 || mult == 21 || mult ==41) ? 5 : 9;
                            int mul2 = 1 << v2_uint64(mult-1);
                            src = loc_str(ins.src, quad_r);
                            of << "    lea (" << src << "," << src << "," << (mul1-1) << "), " << dst;
                            string tmp_dst = loc_str(ins.dst, quad_r);
                            of << "\n    lea (" << src << "," << tmp_dst << "," << mul2 << "), " << dst << "\n";
                        } else if(ins.dst.type == Loc::reg && ins.src.type == Loc::reg && !loc_equal(ins.src, ins.dst)
                            && (mult == 17 || mult == 33 || mult == 65)) {
                            // 1 + {1,2,4,8}*{1,2,4,8}
                            // => {17,33,65}
                            // mul1 = 8;
                            int mul2 = (mult == 17) ? 2 : (mult == 33 )? 4 : 8;
                            src = loc_str(ins.src, quad_r);
                            of << "    lea (," << src << ",8), " << dst;
                            string tmp_dst = loc_str(ins.dst, quad_r);
                            of << "\n    lea (" << src << "," << tmp_dst << "," << mul2 << "), " << dst << "\n";
                        } else {
                            of << "    imul" << suffix[size] << " $" << mult << ", " << src << ", " << dst << "\n";
                        }
                    }
                    break;}
                case movx: 
                case movzxq: { // for movzxq, high bits are dead so fuck it anyway
                    if(ins.dst.type == Loc::mem) {
                        of << "    mov" << suffix[ins.size] << " " << loc_str(ins.src, ins.size) << ", "
                            << loc_str(ins.dst, ins.size) << "\n";
                        continue;
                    }
                    if(loc_equal(ins.src, ins.dst)) {
                        // *technically, for `movl`, this is not a nop, but in this IR, high
                        // bits are dead
                        continue;
                    }
                    if(ins.size <= word_r) {
                        of << "    movz" << suffix[ins.size] << "l " << loc_str(ins.src, ins.size) << ", "
                            << loc_str(ins.dst, dword_r) << "\n";
                    } else {
                        of << "    mov" << suffix[ins.size] << " " << loc_str(ins.src, ins.size) << ", "
                            << loc_str(ins.dst, ins.size) << "\n";
                    }
                    break;}
                case call: {
                    of << "    call " << asm_name_by_uid[ins.uid] << "\n";
                    break;}
                case cmpx: { // no optimizations
                    of << "    cmp" << suffix[ins.size] << " " << loc_str(ins.src, ins.size) << ", "
                        << loc_str(ins.dst, ins.size) << "\n";
                    break;}
                case label: {
                    of << ".label_uid_" << ins.uid << ":\n";
                    break;}
                case jz: {
                    of << "    jz .label_uid_" << ins.uid << "\n";
                    break;}
                case jnz: {
                    of << "    jnz .label_uid_" << ins.uid << "\n";
                    break;}
                case jmp: {
                    of << "    jmp .label_uid_" << ins.uid << "\n";
                    break;}
                
            }
        }
    }
};




