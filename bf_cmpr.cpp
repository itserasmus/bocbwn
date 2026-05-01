#include "bf.hpp"
#include "bf_asm_be.cpp"

#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>

using namespace std;




int read_bytecode(uint32_t& len, string&, vector<command>&);

int main(int argc, char** argv) {
    string input_file_name = "";
    string output_file_name = "";
    bool is_output = false;
    bool assume_no_overflow = false;
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
            } else if(arg.find("ano") == 0) {
                string aux = arg.substr(3);
                if(aux == "1" || aux == "true" || aux == "True" || aux == "TRUE" || aux == "yes") {
                    assume_no_overflow = true;
                } else {
                    assume_no_overflow = false;
                }
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
    if(dump_assembly(output_file_name, commands, assume_no_overflow)) {return 1;}

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
