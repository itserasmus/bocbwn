#include "bf.hpp"

#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>
#include <map>

using namespace std;



enum type { bool_c, int_c, string_c };
struct cla_type {
    const type t;
    void* ptr;
};




int read_bytecode(uint32_t& len, string&, vector<command>&);
int read_source(string&, vector<command>&, bool);

int dump_bfassembly(string&, vector<command>&);
int dump_bytecode(string&, vector<command>&);

int parse(int, char**, std::string&, std::string&, std::map<std::string, cla_type>&, bool=true);



int main(int argc, char** argv) {
    /* Usage: 
     *   bf_cmplr <input> [-o output] [-flags]
     * flags:
     *  -in=0/bf/brainfuck|1/bfvm           : specify input type
     *  -out=0/bfvm|1/bfasm|2/asm/s         : specify output type
     *  -strict=0/false|1/true              : strict/non-strict brainfuck parsing
     *  -ano=0/false|1/true                 : assume tape overflow/no tape overflow
     *  -te=0/false|1/true                  : tape starts empty/non-empty
     * all flags' parameters default to 1 (ie -strict is the same as -strict=1)
     * if not given, the output file name is inferred
     */
    string input_file_name;
    string output_file_name;
    int in_type;
    int out_type;
    string str_in_type = "";
    string str_out_type = "";
    bool strict = true;
    bool assume_no_overflow = false;
    bool tape_empty = true;
    map<string, cla_type> flags = {
        {"in", {string_c, &str_in_type}},
        {"out", {string_c, &str_out_type}},
        {"strict", {bool_c, &strict}},
        {"ano", {bool_c, &assume_no_overflow}},
        {"te", {bool_c, &tape_empty}},
    };

    parse(argc, argv, input_file_name, output_file_name, flags, false);
    if(input_file_name == "") {
        cout << "Fatal: No input file provided" << endl;
        return 1;
    }
    // infer in_type and out_type
    if(str_in_type == "") {
        size_t pos = input_file_name.rfind('.');
        string in_ext = pos>input_file_name.size()?"":input_file_name.substr(pos);
        if(in_ext == ".b" || in_ext == ".bf" || in_ext == ".brainfuck") {
            in_type = 0;
        } else if(in_ext == ".bfvm") {
            in_type = 1;
        } else {
            in_type = 0; // bf is the default
        }
    } else {
        if(str_in_type == "0" || str_in_type == "bf" || str_in_type == "brainfuck") {
            in_type = 0;
        } else if(str_in_type == "1" || str_in_type == "bfvm") {
            in_type = 1;
        } else {
            cout << "Fatal: invalid input format" << endl;
            return 1;
        }
    }
    if(str_out_type == "") {
        size_t pos = output_file_name.rfind('.');
        string out_ext = pos>output_file_name.size()?"":output_file_name.substr(pos);
        if(out_ext == ".bfvm") {
            out_type = 0;
        } else if(out_ext == ".bfasm") {
            out_type = 1;
        } else {
            out_type = 2;
        }
    } else {
        if(str_out_type == "0" || str_out_type == "bfvm") {
            out_type = 0;
        } else if(str_out_type == "1" || str_out_type == "bfasm") {
            out_type = 1;
        } else if(str_out_type == "2" || str_out_type == "asm" || str_out_type == "s") {
            out_type = 2;
        } else {
            cout << "Fatal: invalid output format" << endl;
            return 1;
        }
    }
    if(output_file_name == "") {
        size_t pos = input_file_name.rfind('.');
        output_file_name = pos>input_file_name.size()?input_file_name:input_file_name.substr(0, pos);
        if(out_type == 0) {
            output_file_name += ".bfvm";
        } else if(out_type == 1) {
            output_file_name += ".bfasm";
        } else {
            output_file_name += ".s";
        }
    }

    vector<command> commands;
    uint32_t len;

    if(in_type == 0) { // *.bf
        if(read_source(input_file_name, commands, strict)) {return 1;}
        if(optimize(commands, tape_empty)) {return 1;}
    } else { // *.bfvm
        if(read_bytecode(len, input_file_name, commands)) {return 1;}
    }

    if(out_type == 0) { // *.bfvm
        if(dump_bytecode(output_file_name, commands)) {return 1;}
    } else if(out_type == 1) { // *.bfasm
        if(dump_bfassembly(output_file_name, commands)) {return 1;}
    } else { // *.s
        if(dump_assembly(output_file_name, commands, assume_no_overflow)) {return 1;}
    }
    
    return 0;
}


int read_bytecode(uint32_t& len, string& input_file_name,vector<command>& commands) {
    // TODO: fix
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
        // if(has_four_byte_aux(c)) {
        //     if(!f.read(reinterpret_cast<char*>(&a), sizeof(a))) {
        //         cerr << "Unexpected EOF" << endl;
        //         return 1;
        //     }
        // }
        commands[i] = {c, a};
    }
    
    return 0;
}
int read_source(string& input_file_name, vector<command>& commands, bool strict) {
    vector<int32_t> block_stack;

    ifstream f(input_file_name, ios::binary);
    if(!f) {
        cerr << "Fatal: Failed to open input file " << input_file_name << endl;
        return 1;
    }
    char byte;
    char prev = 0;
    int line_no = 0;
    int char_no = 0;
    while(f.get(byte)) {
        if(byte == '\r' || (byte == '\n' && prev != '\r')) {
            line_no++;
            char_no = -1;
        }
        // process > < [ ] + - . , in that order
        if(byte == '>') {
            commands.push_back({MOV, 1});
        } else if(byte == '<') {
            commands.push_back({MOV, -1});
        } else if(byte == '[') {
            commands.push_back(command::make_match(BRZ, 0));
            block_stack.push_back(commands.size() - 1);
        } else if(byte == ']') {
            if(block_stack.size() == 0) {
                cerr << "Error: Unexpected Symbol ']': " << line_no << ":" << char_no << endl;
                if(strict) {
                    return 1;
                } else {
                    continue;
                }
            }
            commands.push_back(command::make_match(BRNZ, block_stack[block_stack.size() - 1]));
            commands[block_stack[block_stack.size() - 1]].match = commands.size() - 1;
            block_stack.pop_back();
        } else if(byte == '+') {
            commands.push_back({ADD, 1, 0});
        } else if(byte == '-') {
            commands.push_back({ADD, (uint8_t)(-1), 0});
        } else if(byte == '.') {
            commands.push_back({OUT, 0});
        } else if(byte == ',') {
            commands.push_back({IN, 0});
        }
        prev = byte;
        char_no++;
    }
    if(block_stack.size() != 0) {
        cerr << "Error: " << block_stack.size() << " unmatched '[' found." << endl;
        if(strict) {
            return 1;
        }
        cerr << "Matching unmatched '['";
        for(int i = block_stack.size() - 1; i >= 0; i++) {
            commands.push_back(command::make_match(BRNZ, block_stack[block_stack.size() - 1]));
            commands[block_stack[block_stack.size() - 1]].match = commands.size() - 1;
            block_stack.pop_back();
        }
    }
    commands.push_back({HLT, 0});
    return 0;
}

int dump_bfassembly(string& output_file_name, vector<command>& commands) {
    ofstream fo(output_file_name, std::ios::binary);

    if(!fo) {
        cerr << "Fatal: Failed to open output file " << output_file_name << endl;
        return 1;
    }

    #ifdef _WIN32
    static constexpr const char* newline = "\r\n";
    static constexpr int newline_len = 2;
    #else
    static constexpr const char* newline = "\n";
    static constexpr int newline_len = 1;
    #endif

    string num;
    string indent;
    for(const command& cmd : commands) {
        const uint8_t opc = cmd.opc;

        
        if(opc == BRNZ) {
            indent.pop_back();
            indent.pop_back();
        }
        bool wrote_arg = false;
        
        fo.write(indent.c_str(), indent.size());
        
        fo.write(op_name(opc), 8);
        if(uses_aux(opc)) {
            num = std::to_string(cmd.aux);
            fo.write(num.c_str(), num.size());
            wrote_arg = true;
        }

        // if(uses_match(opc)) {
        //     if(wrote_arg) {
        //         fo.write("; ", 2);
        //     }

        //     num = std::to_string(cmd.match);
        //     fo.write(num.c_str(), num.size());
        //     wrote_arg = true;
        // }

        if(uses_off(opc) && cmd.off != 0) {
            if(wrote_arg) {
                fo.write("; ", 2);
            }

            fo.write("(", 1);

            num = std::to_string(cmd.off);
            fo.write(num.c_str(), num.size());

            fo.write(")", 1);
        }

        if(opc == BRZ) {
            indent.push_back(' ');
            indent.push_back(' ');
        }
        fo.write(newline, newline_len);
    }

    return 0;
}
int dump_bytecode(string& output_file_name, vector<command>& commands) {
    ofstream fo(output_file_name, ios::binary);

    if(!fo) {
        cerr << "Fatal: Failed to open output file " << output_file_name << endl;
        return 1;
    }
    uint8_t c;
    int32_t a = commands.size();
    fo.write(reinterpret_cast<char*>(&a), 4);
    for(const command& cmd : commands) {
        a = cmd.aux;
        c = cmd.opc;

        fo.write(reinterpret_cast<char*>(&c), sizeof(c));
        // if(has_four_byte_aux(c)) {
        //     fo.write(reinterpret_cast<char*>(&a), sizeof(a));
        // }
    }

    return 0;
}




int parse(int argc, char** argv, std::string& input, std::string& output,
    std::map<std::string, cla_type>& flags, bool die_loudly) {
    // anything not starting with "-" is considered either an input or output
    // file, everything else is a flag
    bool success = true;
    for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '\0') {
            if(die_loudly) {
                return 1;
            } else {
                success = false;
                continue;
            }
        }
        if(argv[i][0] != '-') {
            input = argv[i];
            continue;
        }
        if(argv[i][1] == 'o' && argv[i][2] == '\0') {
            i++;
            if(i == argc) {
                return die_loudly || success;
            }
            output = argv[i];
            continue;
        }
        std::string str = argv[i] + 1;
        std::string set;
        size_t eq_pos = str.find('=');
        if(eq_pos != std::string::npos) {
            set = str.substr(eq_pos + 1);
            str = str.substr(0, eq_pos);
        }
        auto it = flags.find(str);
        if(it == flags.end()) {
            if(die_loudly) {
                return 1;
            } else {
                success = false;
                continue;
            }
        }
        void* ptr = it->second.ptr;
        if(!ptr) {
            if(die_loudly) {
                return 1;
            } else {
                success = false;
                continue;
            }
        }
        switch(it->second.t) {
            case bool_c:
                if(set == "" || set == "1" || set == "true" || set == "True" || set == "yes") {
                    *(bool*)ptr = true;
                } else {
                    *(bool*)ptr = false;
                }
                break;
            case int_c: {
                char* endptr;
                errno = 0;
                int n = strtol(set.c_str(), &endptr, 0);
                if(*endptr != '\0' || errno) {
                    if(die_loudly) {
                        return 1;
                    } else {
                        success = false;
                        continue;
                    }
                }
                *(int*)ptr = n;
                break;}
            case string_c:
                *(std::string*)ptr = set;
                break;
        }
    }
    return success;
}
