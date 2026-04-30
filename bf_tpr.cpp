#include "bf.hpp"
#include "bf_optimize.cpp"

#include <vector>
#include <string>
#include <iostream>
#include <fstream>


using namespace std;



int read_source(string&, vector<command>&, bool);
int dump_assembly(string&, vector<command>&);
int dump_bytecode(string&, vector<command>&);


int main(int argc, char** argv) {
    string input_file_name = "";
    string output_file_name = "";
    int optimization_level = 3;

    bool strict = true;
    bool output_as_assembly = false;
    bool tape_empty = true;
    if(argc < 2) {
        cerr << "Fatal: No input file" << endl;
        return 1;
    }
    bool is_output = false;
    for(int i = 1; i < argc; i++) {
        string arg = string(argv[i]);
        if(arg == "") {continue;}
        if(arg[0] == '-') { // it's a flag!
            if(arg.length() == 1) {continue;}
            arg = arg.substr(1 + (arg[1] == '-'));
            if(arg == "strict" || arg == "strict=true") {
                strict = true;
            } else if(arg == "loose" || arg == "strict=false") {
                strict = false;
            } else if(arg == "o") {
                is_output = true;
            } else if(arg == "o-asm" || arg == "asm") {
                output_as_assembly = true;
            } else if(arg == "O0") {
                optimization_level = 0;
            } else if(arg == "O1") {
                optimization_level = 1;
            } else if(arg == "O2") {
                optimization_level = 2;
            } else if(arg == "O3") {
                optimization_level = 3;
            } else if(arg.find("tape-empty=") == 0) {
                string suf = arg.substr(11);
                if(suf == "true" || suf == "1" || suf == "yes" || suf == "TRUE" || suf == "True") {
                    tape_empty = true;
                } else {
                    tape_empty = false;
                }
            }
        } else { // it's the input or output file
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
        if(input_file_name.size() >= 3 && input_file_name.substr((input_file_name.size() - 3)) == ".bf") {
            output_file_name = input_file_name + (output_as_assembly?"asm":"vm");
        } else {
            output_file_name = input_file_name + (output_as_assembly?".bfasm":".bfvm");
        }
    }
    
    vector<command> commands;
    if(read_source(input_file_name, commands, strict)) {return 1;};
    if(optimize(commands, optimization_level, tape_empty)) {return 1;};
    if(output_as_assembly) {
        if(dump_assembly(output_file_name, commands)) {return 1;}
    } else {
        if(dump_bytecode(output_file_name, commands)) {return 1;};
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
            commands.push_back({BRZ, 0});
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
            commands.push_back({BRNZ, block_stack[block_stack.size() - 1]});
            commands[block_stack[block_stack.size() - 1]].aux = commands.size() - 1;
            block_stack.pop_back();
        } else if(byte == '+') {
            commands.push_back({ADD, 1});
        } else if(byte == '-') {
            commands.push_back({ADD, -1});
        } else if(byte == '.') {
            commands.push_back({OUT, 1});
        } else if(byte == ',') {
            commands.push_back({IN, 1});
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
            commands.push_back({BRNZ, block_stack[block_stack.size() - 1]});
            commands[block_stack[block_stack.size() - 1]].aux = commands.size() - 1;
            block_stack.pop_back();
        }
    }
    commands.push_back({HLT, 0});
    return 0;
}

int dump_assembly(string& output_file_name, vector<command>& commands) {
    ofstream fo(output_file_name, ios::binary);

    if(!fo) {
        cerr << "Fatal: Failed to open output file " << output_file_name << endl;
        return 1;
    }
    uint8_t c;
    int32_t a = commands.size();

    static const int cmd_names_len = 8;
    #ifdef _WIN32
    static const char* const newline = "\r\n";
    static const int newline_len = 2;
    #else
    static const char* const newline = "\n";
    static const int newline_len = 1;
    #endif
    string num;
    for(const command& cmd : commands) {
        a = cmd.aux;
        c = cmd.opc;

        fo.write(op_name(c), cmd_names_len);

        if(has_four_byte_aux(c)) {
            num = to_string(a);
            fo.write(num.c_str(), num.size());
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
        if(has_four_byte_aux(c)) {
            fo.write(reinterpret_cast<char*>(&a), sizeof(a));
        }
    }

    return 0;
}