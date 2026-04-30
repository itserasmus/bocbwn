#include "bf.hpp"

#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>

using namespace std;


int read_bytecode(uint32_t& len, string&, vector<command>&);
int dump_assembly(string&, vector<command>&);

int main(int argc, char** argv) {
    string input_file_name = "";
    string output_file_name = "";
    bool is_output = false;
    if(argc < 2) {
        cerr << "Fatal: No input file" << endl;
        return 1;
    }
    for(int i = 1; i < argc; i++) {
        string arg = string(argv[i]);
        if(arg == "") {continue;}
        if(arg[0] == '-') { // it's a flag!
            if(arg.length() == 1) {continue;}
            arg = arg.substr(1 + (arg[1] == '-'));
            if(arg == "o") {
                is_output = true;
            }
        } else { // it's the input file
            if(!is_output) {
                if(input_file_name != "") {
                    cerr << "Warning: Multiple files entered. Using first file (" << input_file_name << ")." << endl;
                } else {
                    input_file_name = arg;
                }
            } else {
                is_output = false;
                if(output_file_name != "") {
                    cerr << "Warning: Multiple files entered. Using first file (" << output_file_name << ")." << endl;
                } else {
                    output_file_name = arg;
                }
            }
        }
    }
    if(output_file_name == "") {
        if(input_file_name.size() >= 5 && input_file_name.substr((input_file_name.size() - 5)) == ".bfvm") {
            output_file_name = input_file_name.substr(0, input_file_name.size() - 5) + ".s";
        } else {
            output_file_name = input_file_name + ".s";
        }
    }

    vector<command> commands;
    uint32_t len;
    if(read_bytecode(len, input_file_name, commands)) {return 1;}
    if(dump_assembly(output_file_name, commands)) {return 1;}

    return 0;
}


uint8_t inv8(uint8_t a) {
    uint8_t t = a*(2-a*a);
    return t*(2-a*t);
}

int read_bytecode(uint32_t& len, string& input_file_name,vector<command>& commands) {
    ifstream f(input_file_name, ios::binary);
    if(!f) {
        cerr << "Fatal: Failed to open " << input_file_name << endl;
        return 1;
    }
    f.read(reinterpret_cast<char*>(&len), sizeof(len));

    uint8_t c = 0;
    int32_t a = 0;
    commands.resize(len);
    for(int i = 0; i < len; i++) {
        if(!f.read(reinterpret_cast<char*>(&c), sizeof(c))) {
            cerr << "Unexpected EOF" << endl;
            return 1;
        }
        if(has_four_byte_aux(c)) {
            if(!f.read(reinterpret_cast<char*>(&a), sizeof(a))) {
                cerr << "Unexpected EOF" << endl;
                return 1;
            }
        }
        commands[i] = {c, a};
    }
    
    return 0;
}

int dump_assembly(string& output_file_name, vector<command>& commands) {
    ofstream fo(output_file_name);

    if(!fo) {
        cerr << "Fatal: Failed to open output file " << output_file_name << endl;
        return 1;
    }
    uint8_t c;
    int32_t a = commands.size();

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
    
    string num;
    fo << prologue;
    int32_t loop_id = 0;
    int32_t ncrab_id = 0;
    for(command& cmd : commands) {
        switch(cmd.opc) {
            case MOV:
                fo << "    addq $" << cmd.aux << ", %rdi\n    andq %rbp, %rdi\n";
                break;
            case ADD:
                fo << "    addb $" << cmd.aux << ", (%rbx,%rdi)\n";
                break;
            case OUT:
                fo << "    movzbl (%rbx,%rdi), %ecx\n    call putchar\n";
                break;
            case OUTC:
                fo << "    movl $" << cmd.aux << ", %ecx\n    call putchar\n";
                break;
            case IN:
                fo << "    call getchar\n    movb %al, (%rbx,%rdi)\n";
                break;
            case BRZ:
                fo << "    cmpb $0, (%rbx,%rdi)\n    jz .loop_end_" << loop_id
                    << "\n.loop_begin_" << loop_id << ":\n";
                cmd.aux = loop_id++;
                break;
            case BRNZ: {
                int32_t l_id = commands[cmd.aux].aux;
                fo << "    cmpb $0, (%rbx,%rdi)\n    jnz .loop_begin_" << l_id
                    << "\n.loop_end_" << l_id << ":\n";
                break;}
            case SET:
                fo << "    movb $" << cmd.aux << ", (%rbx,%rdi)\n";
                break;
            case INV:
                fo << "    movb (%rbx,%rdi), %al\n    movb %al, %cl\n    mulb %al\n\
    notb %al\n    addb $3, %al\n    mulb %cl\n    movb %al, %r8b\n\
    mulb %cl\n    notb %al\n    addb $3, %al\n    mulb %r8b\n\
    movb %al, (%rbx,%rdi)\n";
                break;
            case MULINV:
                fo << "    movb $" << inv8(cmd.aux) << ", %al\n    mulb (%rbx,%rdi)\n\
    movb %al, (%rbx,%rdi)\n";
                break;
            case PUTA:
                fo << "    movb (%rbx,%rdi), %r12b\n";
                break;
            case PULLA:
                fo << "    movb %r12b, (%rbx,%rdi)\n";
                break;
            case ACCUMA:
                fo << "    addb %r12b, (%rbx,%rdi)\n";
                break;
            case MACMA:
                fo << "    movb $" << cmd.aux << ", %al\n    mulb %r12b\n\
    addb %al, (%rbx,%rdi)\n";
                break;
            case NCRAB:
                // for now, a quick dirty hardcode for n <= 8
                // for %r12b choose n -> %r13b
                // no need to worry about overflow because
                // for n<=8, we can just use a quad register
                switch(cmd.aux) {
                    case 0:
                        fo << "    movb $1, %r13b\n";
                        break;
                    case 1:
                        fo << "    movb %r12b, %r13b\n";
                        break;
                    case 2:
                        fo <<
R"(    movzbl %r12b, %eax
    leal -1(%eax), %ebx
    imull %ebx, %eax
    shrl $1, %eax
    movb %al, %r13b
)";
                        break;
                    case 3:
                        fo <<
R"(    movzbl %r12b, %eax
    leal -1(%eax), %ebx
    imull %ebx, %eax
    decl %ebx
    imull %ebx, %eax
    shrl $1, %eax
    imulw $171, %ax
    movb %al, %r13b
)";
                        break;
                    case 4:
                        fo <<
R"(    movzbl %r12b, %eax
    leal -1(%eax), %ebx
    imull %ebx, %eax
    decl %ebx
    imull %ebx, %eax
    decl %ebx
    imull %ebx, %eax
    shrl $3, %eax
    imulw $171, %ax
    movb %al, %r13b
)";
                        break;
                    case 5:
                        fo <<
R"(    movzbq %r12b, %rax
    leaq -1(%rax), %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    shrq $3, %rax
    imulw $239, %ax
    movb %al, %r13b
)";
                        break;
                    case 6:
                        fo <<
R"(    movzbq %r12b, %rax
    leaq -1(%rax), %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    shrq $4, %rax
    imulw $165, %ax
    movb %al, %r13b
)";
                        break;
                    case 7:
                        fo <<
R"(    movzbq %r12b, %rax
    leaq -1(%rax), %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    shrq $4, %rax
    imulw $243, %ax
    movb %al, %r13b
)";
                        break;
                    case 8:
                        fo <<
R"(    movzbq %r12b, %rax
    leaq -1(%rax), %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    decq %rbx
    imulq %rbx, %rax
    shrq $7, %rax
    imulw $243, %ax
    movb %al, %r13b
)";
                        break;
                    default:
                        cerr << "NCRAB not supported for n > 8\n";
                }
                break;
            case NCBAB: {
                fo << R"(    cmpb %r12b, %r13b
    jbe .start_ncrab_)" << ncrab_id << R"(
    movb $0, %r13b
    jmp .end_ncrab_)" << ncrab_id << R"(
.start_ncrab_)" << ncrab_id << R"(:
    movb %r13b, %r10b
    shlb $1, %r10b
    cmpb %r12b, %r10b
    movb %r13b, %r10b
    jbe .ncrab_r_ready_)" << ncrab_id << R"(
    subb %r12b, %r10b
    negb %r10b
.ncrab_r_ready_)" << ncrab_id << R"(:
    movb $1, %al
    movb $1, %r8b
    movb %r12b, %r9b
    cmpb $0, %r10b
    jz .ncrab_loop_end_)" << ncrab_id << R"(
.ncrab_loop_start_)" << ncrab_id << R"(:
    tzcntw %r9w, %cx
    movb %r9b, %r11b
    shrb %cl, %r11b
    mulb %r11b
    xchgb %al, %r8b
    tzcntw %r10w, %cx
    movb %r10b, %r11b
    shrb %cl, %r11b
    mulb %r11b
    xchgb %al, %r8b
    decb %r9b
    decb %r10b
    cmpb $0, %r10b
    jnz .ncrab_loop_start_)" << ncrab_id << R"(
.ncrab_loop_end_)" << ncrab_id << R"(:
    movb %al, %r15b
    movb %r8b, %al
    movb %al, %cl
    mulb %al
    notb %al
    addb $3, %al
    mulb %cl
    movb %al, %r8b
    mulb %cl
    notb %al
    addb $3, %al
    mulb %r8b
    mulb %r15b
    movzbw %r13b, %r13w
    popcntw %r13w, %cx
    movzbw %r12b, %r12w
    popcntw %r12w, %dx
    subb %dl, %cl
    movw %r12w, %r8w
    subw %r13w, %r8w
    popcntw %r8w, %dx
    addb %dl, %cl
    shlb %cl, %al
    movb %al, %r13b
.end_ncrab_)" << ncrab_id << R"(:
)";
                ncrab_id++;
                break;}
            case INVRA:
                fo << "    movb %r12b, %al\n    mulb %al\n    notb %al\n\
    addb $3, %al\n    mulb %r12b\n    movb %al, %r8b\n    mulb %r12b\n\
    notb %al\n    addb $3, %al\n    mulb %r8b\n    movb %al, %r12b\n";
                break;
            case PUTB:
                fo << "    movb (%rbx,%rdi), %r13b\n";
                break;
            case PULLB:
                fo << "    movb %r13b, (%rbx,%rdi)\n";
                break;
            case ACCUMB:
                fo << "    addb %r13b, (%rbx,%rdi)\n";
                break;
            case MACMB:
                fo << "    movb $" << cmd.aux << ", %al\n    mulb %r13b\n\
                addb %al, (%rbx,%rdi)\n";
                break;
            case INVRB:
                fo << "    movb %r13b, %al\n    mulb %al\n    notb %al\n\
                addb $3, %al\n    mulb %r13b\n    movb %al, %r8b\n    mulb %r13b\n\
                notb %al\n    addb $3, %al\n    mulb %r8b\n    movb %al, %r13b\n";
                break;
            case RKILL:
                break;
            case NOP:
                fo << "    nop\n";
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
