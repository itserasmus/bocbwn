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
