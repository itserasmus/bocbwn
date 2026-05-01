// specification:
// >/<      Tape wraparound is UB. If the tape too small, then certain
//          optimizations may break it. If you only use the portion [-a, b]
//          of the tape and the tape is larger than [-a, b] on both sides,
//          then the program will be fine. Otherwise not.
// +/-      Cells are all uint8_t. Wraparound is defined, all operations are
//          on Z/256Z
// [/]      So long as they are paired, they behave fine.
// ./,      Input and output ASCII characters.
// 
// This transpiler requires ~64 bytes per character of the program, so at
// around 100 million charactes, it will almost certainly crash, and you
// deserve it. This transpiler uses int32_t to represent indices, so if your
// program is longer than 2 billion instructions, it may be parsed wrong, but
// if you have a program like that anyways, so help me god, I will drag you
// to a psych ward myself
// 
// The bfasm operations that may be used by this program are
// MOV, ADD, OUT, IN, BRZ, BRNZ, SET, PUTA, ACCUMA, MACMA, RKILL, OUTC, NOP,
// and HLT

#include "bf.hpp"
#include <cstdint>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <unordered_map>






inline uint8_t mod_inv(uint8_t x) {
    uint8_t t = x*(2-x*x);
    t = t*(2-t*x);
    return t;
}


using namespace std;
struct cell_value { bool known; uint8_t val; };
class tape_metadata {
public:
    std::unordered_map<uint32_t, cell_value> accessed;
    uint32_t mp_pos = 0; // wraparound saves the day
    bool out_of_range_zero = true;

    void set_val(uint8_t val) {
        accessed[mp_pos] = { true, val };
    }
    void mark_unknown() {
        if(out_of_range_zero) {
            accessed[mp_pos] = { false };
        } else {
            auto it = accessed.find(mp_pos);
            if(it != accessed.end()) {
                it->second.known = false;
            }
        }
    }
    cell_value get_val() {
        auto it = accessed.find(mp_pos);
        if(it != accessed.end()) {
            return it->second;
        }
        if(out_of_range_zero) {
            return { true, 0 };
        } else {
            return { false };
        }
    }
    void clear_all() {
        accessed.clear();
        out_of_range_zero = false;
    }

    void merge(const tape_metadata& b) {
        if(mp_pos != b.mp_pos || out_of_range_zero != b.out_of_range_zero) {
            nuke("this is why we can't have good things\n");
        }
        for(const auto& [k, vb] : b.accessed) {
            auto it = accessed.find(k);
            if(it == accessed.end()) {
                accessed[k] = vb;
            } else {
                cell_value& va = it->second;
                if(va.known) {
                    if(vb.known && va.val != vb.val) {
                        nuke("dear god, what happened this time\n");
                    }
                } else if(vb.known) {
                    va.val = vb.val;
                    va.known = true;
                } else {
                    // do nothing
                }
            }
        }
    }

    tape_metadata operator&&(const tape_metadata& b) {
        if(mp_pos != b.mp_pos || out_of_range_zero != b.out_of_range_zero) {
            nuke("how many times must I tell you, they need to be the same\n");
        }
        tape_metadata ret;
        ret.out_of_range_zero = out_of_range_zero;
        for(const auto& [k, vb] : b.accessed) {
            auto it = accessed.find(k);
            if(!vb.known) {
                if(out_of_range_zero) {
                    // if oorz, then it's unknown anyways
                    ret.accessed[k] = { false };
                }
                continue;
            }
            if(it == accessed.end()) {continue;}
            cell_value& va = it->second;
            if(!va.known) { // deal with it later
                continue;
            }
            if(va.val != vb.val) {
                ret.accessed[k] = { false }; // mark it as unknown
                continue;           // and don't nuke it
            }
            ret.accessed[k] = vb;
        }
        if(out_of_range_zero) {
            for(const auto& [k, va] : accessed) {
                // fill in everything `this` doesn't know.
                if(!va.known) {
                    ret.accessed[k] = { false };
                }
            }
        }
        ret.mp_pos = mp_pos;
        return ret;
    }
};

    
enum type_t : uint8_t { OP, SEQ, LOOP };
struct node {
    type_t type;
    bool loops_mov_balanced;    // for children LOOPS
    bool has_io = false;
    bool basic_children;
    int32_t mov_offset;         // for child SEQ and OPs, if !loops_mov_balanced
                                // this value is meaningless
    // the phrase mov-balanced is shorthand for `loops_mov_balanced && mov_offset==0`

    command cmd;
    std::vector<node> children;
};


const char* node_type_name(type_t t) {
    switch(t) {
        case OP: return "OP";
        case SEQ: return "SEQ ";
        case LOOP: return "LOOP";
        default: return "???";
    }
}
void print_tree(const node& n, int depth = 0) {
    for(int i = 0; i < depth; i++) {
        std::cout << "  ";
    }

    std::cout << node_type_name(n.type);
    if(n.type != OP) {
        std::cout << " [mb=" << n.loops_mov_balanced << ", mo=" << n.mov_offset << ", io=" << n.has_io << "]";
    }
    if(n.type == OP) {
        std::cout << " " << op_name(n.cmd.opc);
        if(has_four_byte_aux(n.cmd.opc)) {
            std::cout << " " << n.cmd.aux;
        }
    }
    std::cout << "\n";

    for(const auto& child : n.children) {
        print_tree(child, depth + 1);
    }
}

node treeify(vector<command>& commands, size_t begin, size_t end) {
    // can only tree-ify the following commands
    // MOV, ADD, OUT, IN, BRZ, BRNZ, HLT
    // trusts caller
    node root = {
        SEQ,
        true,
        false,
        true,
        0,
        {},
        vector<node>(),
    };
    // Is it recursive? Yes. Is recursion slow? Technically, yes. Do I care?
    // Well, yes. Do I care enough to make it iterative? Absolutely not.

    if(commands[begin].opc == BRZ && commands[begin].aux == end-1) {
        // parsing a LOOP is identical to parsing a SEQ without the
        // `[` and `]`
        begin++;
        end--;
        root.type = LOOP;
    }
    for(size_t i = begin; i < end; i++) {
        if(commands[i].opc == BRZ) {
            root.basic_children = false;
            break;
        }
    }
    if(root.basic_children) {
        // if all the children are MOV, ADD, OUT, IN, and HLT
        root.mov_offset = 0;
        root.loops_mov_balanced = true; // no loop children, so all loop children are mov balanced
        for(size_t i = begin; i < end; i++) {
            uint8_t opc = commands[i].opc;
            root.children.push_back({
                OP,
                false,
                false,
                false,
                0,
                commands[i],
                vector<node>(0)
            });
            if(opc == MOV) {
                root.mov_offset += commands[i].aux;
            } else if(opc == IN || opc == OUT) {
                root.has_io = true;
            }
        }
        return root;
    }
    // If it has loop children, since !basic_children, even OP
    // children now must be nested into SEQs. This helps.
    size_t section_start = begin;
    for(size_t i = begin; i < end; i++) {
        uint8_t opc = commands[i].opc;
        int32_t aux = commands[i].aux;
        if(opc == BRZ) {
            if(section_start != i) {
                root.children.push_back(treeify(commands, section_start, i));
            }
            root.children.push_back(treeify(commands, i, aux+1));
            root.children.back().type = LOOP;
            i = aux;
            section_start = i+1;
            continue;
        }
    }
    if(section_start != end) {
        root.children.push_back((treeify(commands, section_start, end)));
    }
    for(node& n : root.children) {
        if(n.has_io) {
            root.has_io = true;
            break;
        }
    }
    for(node& n : root.children) {
        if(n.type == LOOP) {
            // now for a loop, n.loop_mov_balanced refers to the children
            // of the loop being balanced, not the loop itself, to make it
            // match... everything else
            if(n.mov_offset != 0 || !n.loops_mov_balanced) {
                root.loops_mov_balanced = false;
                break;
            }
        } else {
            if(!n.loops_mov_balanced) {
                root.loops_mov_balanced = false;
                break;
            }
            root.mov_offset += n.mov_offset;
        }
    }

    return root;
}

int clear_nop(vector<command>& commands) {
    size_t dst = 0;
    size_t src = 0;
    // find first NOP
    for(command& cmd : commands) {
        if(cmd.opc == NOP) {
            dst++;
            break;
        }
        dst++;
        src++;
    }
    if(dst == src) { // no NOP found
        return 0;
    }

    uint8_t opc;
    for(; dst < commands.size(); dst++) {
        opc = commands[dst].opc;
        if(opc == NOP) {continue;}

        commands[src] = commands[dst];
        if(opc == BRZ) {
            commands[commands[src].aux].aux = src;
        } else if(opc == BRNZ) {
            commands[commands[src].aux].aux = src;
        }

        src++;
    }

    commands.resize(src);

    return 0;
}

void recompute_properties(node& root) {
    if(root.basic_children) {
        root.loops_mov_balanced = true;
        root.mov_offset = 0;
        root.has_io = false;
        for(node& child : root.children) {
            uint8_t opc = child.cmd.opc;
            if(opc == MOV) {
                root.mov_offset += child.cmd.aux;
            } else if(opc == OUT || opc == OUTC || opc == IN) {
                root.has_io = true;
            }
        }
    } else {
        root.has_io = false;
        root.loops_mov_balanced = true;
        root.mov_offset = 0;
        for(node& child : root.children) {
            if(child.has_io) {
                root.has_io = true;
                break;
            }
        }
        for(node& child : root.children) {
            if(child.type == LOOP) {
                if(child.mov_offset != 0 || !child.loops_mov_balanced) {
                    root.loops_mov_balanced = false;
                    break;
                }
            } else { // SEQ
                if(!child.loops_mov_balanced) {
                    root.loops_mov_balanced = false;
                    break;
                }
                root.mov_offset += child.mov_offset;
            }
        }
    }
}

void flatten(node& root) {
    // converts:
    // LOOP:        ->  LOOP:
    //   LOOP:            ...
    //     ...
    // LOOP:        ->  LOOP:
    //   SEQ:             ...
    //     ...
    //   SEQ:
    //     ...
    // SEQ:         ->  LOOP:
    //   LOOP:            ...
    //      ...
    // SEQ:         ->  SEQ:
    //   SEQ:             ...
    //     ...
    //   SEQ:
    //     ...
    //
    // SEQ/LOOP     ->  SEQ/LOOP
    //   ...              ...
    //   SEQ              SEQ
    //   SEQ              ...
    //   ...
    if(root.basic_children) {return;} // duh

    for(node& child : root.children) {
        flatten(child);
    }
    if(root.children.size() == 0) {
        if(root.type == SEQ) {
            root.type = OP;
            root.cmd = { NOP, 0 };
        }
        return;
    }
    // since we are in a SEQ/LOOP, and SEQ without basic children
    // can be flattened.
    bool all_child_seq_have_basic_children = true;
    for(node& child : root.children) {
        if(child.type == SEQ && !child.basic_children) {
            all_child_seq_have_basic_children = false;
            break;
        }
    }
    if(!all_child_seq_have_basic_children) {
        // Now, unfortunately, we must flatten it
        vector<node> new_children;
        for(node& child : root.children) {
            if(!(child.type == SEQ || child.basic_children)) {
                // just copy it normally
                new_children.push_back(std::move(child));
                continue;
            }
            // since all the children have already done this step, all
            // the child's SEQ children should themselves have
            // `basic_children == true`, so we can do this just once
            for(node& grandchild : child.children) {
                new_children.push_back((std::move(grandchild)));
            }
        }
        root.children = std::move(new_children);
    }
    // since flatten can create a NOP, the children are
    // SEQ, LOOP, or NOP. Consecutive SEQ are concatenated,
    // NOP are removed.
    size_t seq_pos = 0;
    bool seq_run = false;
    for(size_t i = 0; i < root.children.size(); i++) {
        node& child = root.children[i];
        if(child.type == OP) { // => child.cmd.opc == NOP
            continue;
        } else if(child.type == LOOP) {
            if(seq_run) {
                // a run just ended
                recompute_properties(root.children[seq_pos]);
            }
            seq_run = false;
        } else {
            if(seq_run) {
                node& seq = root.children[seq_pos];
                for(node& n : child.children) {
                    // since all SEQ children now have basic children,
                    // this is fine
                    seq.children.push_back(n);
                }
                child.children.clear();
                child.type = OP;
                child.cmd = { NOP }; // mark it for deletion
                // all properties will be recomputed at the end anyways
            } else {
                seq_run = true;
                seq_pos = i;
            }
        }
    }
    if(seq_run) {
        recompute_properties(root.children[seq_pos]);
    }
    // and now remove NOPs
    size_t dst = 0;
    for(size_t i = 0; i < root.children.size(); i++) {
        node& child = root.children[i];
        if(child.type != OP) { // => child.cmd.opc == NOP
            if(dst != i) {
                root.children[dst] = std::move(child);
            }
            dst++;
            continue;
        }
    }
    root.children.resize(dst);
    if(root.children.size() == 1) {
        // no basic children, so child must be a SEQ or LOOP
        if(root.type == SEQ) {
            root = std::move(root.children[0]);
        } else if(root.children[0].type == SEQ) {
            root = std::move(root.children[0]);
            root.type = LOOP;
        } else {
            root = std::move(root.children[0]);
        }
    }
}

void linearize(const node& n, std::vector<command>& out) {
    switch(n.type) {
        case OP:
            if(n.cmd.opc != NOP) {
                out.push_back(n.cmd);
            }
            break;
        case SEQ:
            for(const node& child : n.children) {
                linearize(child, out);
            }
            break;
        case LOOP:
            int32_t brz_pos = out.size();
            out.push_back({BRZ, 0});
            for(const node& child : n.children) {
                linearize(child, out);
            }
            int32_t brnz_pos = out.size();
            out.push_back({BRNZ, brz_pos});
            out[brz_pos].aux = brnz_pos;
            break;
    }
}

void nuke_touched_values(const node& root, tape_metadata& tmd) {
    if(root.type == OP || !root.loops_mov_balanced || (root.type == LOOP && root.mov_offset != 0)) {
        nuke("maybe don't\n");
    }

    // now for both SEQ and LOOP, since we're just nuking stuff, we
    // can use the same logic in both places
    if(root.basic_children) {
        for(const node& child : root.children) {
            uint8_t opc = child.cmd.opc;
            int32_t aux = child.cmd.aux;
            if(opc == MOV) {
                tmd.mp_pos += aux; // since root is mov-balanced, mp_pos will return to 0
            } else if(opc == ADD || opc == IN || opc == SET || opc == ACCUMA || opc == MACMA) {
                // nuke it
                tmd.mark_unknown();
            }
        }
        return;
    }
    for(const node& child : root.children) {
        nuke_touched_values(child, tmd);
    }

    // don't add a zeroing of the current cell if we're on a loop
    // because zeroing is still touching a value
}

void simplified_pass(const node& root, tape_metadata& tmd) {
    // does a sort-of simulation of the root on the given tape metadata
    // as the name suggests, this is a relatively weak pass to make sure
    // it can be done in O(n) time to prevent the time complexity of
    // main_pass from becoming unmanageable.

    // can support the operations
    // MOV, ADD, OUT, IN, SET, PUTA, ACCUMA, MACMA, NCRAB, MULRAB, PUTB
    // ACCUMB, MACMB, RKILL, NOP, HLT

    if(root.type == OP) {
        nuke("major whoopsie\n");
    }

    // for loops, we use a slightly different approach from main_pass. To ensure
    // the simplified pass is O(n), we do not check if a loop is executed
    // only once

    if(root.type == LOOP) {
        cell_value cell_at_start = tmd.get_val();
        if(cell_at_start.known && cell_at_start.val == 0) {
            // if the cell is 0, the loop isn't run (duh)
            return;
        }

        if(!root.loops_mov_balanced || root.mov_offset != 0) {
            // we nuke the tape metadata no matter what.
            tmd.clear_all();
            // we cannot make any comment on the tape
            tmd.set_val(0); // because after a loop, the cell at the loop is 0
            return;
        }
        // for mov-balanced loops, we nuke the parts of tape metadata that
        // are touched and proceed as if it was a SEQ
        nuke_touched_values(root, tmd);
    }
    

    if(root.basic_children) {
        bool regA_known = false;
        uint8_t regA;
        for(const node& child : root.children) {
            uint8_t opc = child.cmd.opc;
            int32_t aux = child.cmd.aux;
            if(opc == MOV) {
                tmd.mp_pos += aux;
            } else if(opc == ADD) {
                cell_value cell = tmd.get_val();
                if(cell.known) {
                    tmd.set_val(cell.val + (uint8_t)aux);
                }
            } else if(opc == OUT) {
                // do nothing
            } else if(opc == IN) {
                tmd.mark_unknown();
            } else if(opc == SET) {
                tmd.set_val((uint8_t)aux);
            } else if(opc == PUTA) {
                cell_value cell = tmd.get_val();
                if(cell.known) {
                    regA_known = true;
                    regA = cell.val;
                } else {
                    regA_known = false;
                }
            } else if(opc == ACCUMA) {
                cell_value cell = tmd.get_val();
                if(regA_known && cell.known) {
                    tmd.set_val(cell.val + regA);
                } else if(!regA_known) {
                    tmd.mark_unknown();
                }
            } else if(opc == MACMA) {
                cell_value cell = tmd.get_val();
                if(regA_known && cell.known) {
                    tmd.set_val(cell.val + regA*(uint8_t)aux);
                } else if(!regA_known) {
                    tmd.mark_unknown();
                }
            } else if(opc == RKILL) {
                regA_known = false;
            } else if(opc == OUTC) {
                // do nothing
            } else if(opc == NOP) {
                // do nothing
            } else if(opc == HLT) {
                // uh... idek what to do here
            } else {
                nuke("this shouldn't ever be printed\n");
            }
        }
        return;
    }
    for(const node& child : root.children) {
        simplified_pass(child, tmd);
    }
}

void translational_affine_loop_solving(node& root) {
    // technically, basic mov-balanced translational homogenous affine loop solving, or TALS
    // for short

    // a quite simple pass, it searches for basic mov-balanced LOOPs without I/O. i.e. loops
    // which only contain MOV and ADD
    if(root.type == OP) {
        nuke("tres interessant\n");
    }

    if(!root.basic_children) {
        for(node& child : root.children) {
            translational_affine_loop_solving(child);
        }
        return;
    }
    if(root.type == LOOP && root.loops_mov_balanced && root.mov_offset == 0 && !root.has_io) {
        // a candidate
        unordered_map<int32_t, uint8_t> addends;
        int32_t rel_ptr = 0;
        for(node& child : root.children) {
            uint8_t opc = child.cmd.opc;
            int32_t aux = child.cmd.aux;
            if(opc == MOV) {
                rel_ptr += aux;
            } else if(opc == ADD) {
                auto it = addends.find(rel_ptr);
                if(it == addends.end()) {
                    addends[rel_ptr] = (uint8_t)aux;
                } else {
                    it->second = it->second + (uint8_t)aux;
                }
            } else { // any other operations should not be here
                return;
            }
        }
        if(rel_ptr != 0) {
            nuke("what have you done\n");
        }
        const uint8_t c0_addend = addends[0];
        if(c0_addend % 2 == 0) {
            return; // unfortunately
        }
        const uint8_t c0_addend_neg_inverse = mod_inv(-c0_addend);

        for(auto it = addends.begin(); it != addends.end();) { // remove 0 values
            if(it->second == 0) {
                it = addends.erase(it);  // apparently this returns the next iterator
            } else {
                ++it;   // ew, prefix increment
            }
        }

        root.type = SEQ;
        root.children.clear();
        if(addends.size() == 1) {
            // zeroing idiom
            root.children.push_back({
                OP,
                true,
                false,
                true,
                0,
                { SET, 0 },
            });
            root.children.shrink_to_fit();
            return;
        }
        // now, we do some very complex math :(
        root.children.reserve(2 + 2*addends.size());
        root.children.push_back({ OP, true, false, true, 0,
                { PUTA, 0 },
        });
        root.children.push_back({ OP, true, false, true, 0,
                { SET, 0 },
        });
        // since rel_ptr == 0
        for(const auto& [ ptr, addend ] : addends) {
            if(ptr == 0) {continue;}
            uint8_t net_addend = addend * c0_addend_neg_inverse;
            root.children.push_back({ OP, true, false, true, 0,
                { MOV, ptr - rel_ptr },
            });
            rel_ptr = ptr;
            if(net_addend == 1) {
                root.children.push_back({ OP, true, false, true, 0,
                    { ACCUMA, 0 },
                });
            } else {
                root.children.push_back({ OP, true, false, true, 0,
                    { MACMA, net_addend },
                });
            }
        }
        root.children.push_back({ OP, true, false, true, 0,
                { MOV, -rel_ptr },
        });
        root.children.push_back({ OP, true, false, true, 0,
                { RKILL, 0 },
        });
    }
}

void main_pass(node& root, tape_metadata& tmd) {
    // The state of the currently known tape is passed as an argument. Basic constructs like
    // +/-, >/< modify the tape predictably, and notably, in a loop, [...], if the loop ever
    // exits (which can be assumed true since if it never exits, then the code after it is
    // never reached), the cell at the mp will be zero. mov-unbalanced loops clear the known
    // tape. Also does some constant folding.

    // can support the operantions
    // MOV, ADD, OUT, IN, SET, PUTA, ACCUMA, MACMA, NCRAB, MULRAB, PUTB
    // ACCUMB, MACMB, RKILL, NOP, HLT


    if(root.type == OP) {
        nuke("what's happening\n");
    } // should never happen
    
    // first, handle SEQ
    if(root.type == SEQ) {
        if(root.basic_children) {
            bool regA_known = false;
            uint8_t regA;
            for(node& child : root.children) {
                uint8_t opc = child.cmd.opc;
                int32_t aux = child.cmd.aux;
                if(opc == MOV) {
                    tmd.mp_pos += aux;
                } else if(opc == ADD) {
                    cell_value cell = tmd.get_val();
                    if(cell.known) {
                        tmd.set_val(cell.val + (uint8_t)aux);
                        child.cmd = { SET, (uint8_t)(cell.val + aux) }; // constant folding
                    }
                } else if(opc == OUT) {
                    cell_value cell = tmd.get_val();
                    if(cell.known) {
                        child.cmd = { OUTC, cell.val };
                    }
                } else if(opc == IN) {
                    tmd.mark_unknown();
                } else if(opc == SET) {
                    cell_value cell = tmd.get_val();
                    if(cell.known && cell.val == (uint8_t)aux) {
                        child.cmd.opc = NOP;
                    } else {
                        tmd.set_val((uint8_t)aux);
                    }
                } else if(opc == PUTA) {
                    cell_value cell = tmd.get_val();
                    if(cell.known) {
                        regA_known = true;
                        regA = cell.val;
                    } else {
                        regA_known = false;
                    }
                } else if(opc == ACCUMA) {
                    cell_value cell = tmd.get_val();
                    if(regA_known && cell.known) {
                        tmd.set_val(cell.val + regA);
                        child.cmd = { SET, (uint8_t)(cell.val + regA) }; // remove dependency on A
                    } else if(!regA_known) {
                        tmd.mark_unknown();
                    } else if(regA_known && !cell.known) {
                        child.cmd = { ADD, regA }; // this still removes the dependency on A
                    }
                } else if(opc == MACMA) {
                    cell_value cell = tmd.get_val();
                    if(regA_known && cell.known) {
                        tmd.set_val(cell.val + regA*(uint8_t)aux);
                        child.cmd = { SET, (uint8_t)(cell.val + regA*(uint8_t)aux) };
                    } else if(!regA_known) {
                        tmd.mark_unknown();
                    } else if(regA_known && !cell.known) {
                        child.cmd = { ADD, (uint8_t)(regA*aux) };
                    }
                } else if(opc == RKILL) {
                    regA_known = false;
                } else if(opc == OUTC) {
                    // do nothing
                } else if(opc == NOP) {
                    // do nothing
                } else if(opc == HLT) {
                    // uh... idek what to do here
                } else {
                    nuke("something happened and I don't know what\n");
                }
            }
            return;
        }
        // if the children aren't basic, call `main_pass` on them.
        for(node& child : root.children) {
            main_pass(child, tmd);
        }
        return;
    }

    // LOOPS are significantly more complicated and not really easy to do losslessly.
    // However, we can get a decent approximation in what is technically O(n*depth)
    // time, but for normal BF programs, depth is small enough that this is fine. The
    // more complex your programs take the more time they take to compile, who would've
    // thought.
    {
        cell_value cell_at_start = tmd.get_val();
        if(cell_at_start.known && cell_at_start.val == 0) {
            // this loop is dead, convert it to an empty SEQ
            root.children.clear();
            root.type = SEQ;
            root.basic_children = true;
            root.has_io = false;
            root.mov_offset = 0;
            root.loops_mov_balanced = true;
            return;
        }
    }
    if(!root.loops_mov_balanced || root.mov_offset != 0) {
        // if the root isn't mov-balanced, just nuke tmd immediately and optimize it
        // conservatively
        
        tmd.clear_all();
        root.type = SEQ;
        main_pass(root, tmd);
        root.type = LOOP;

        tmd.clear_all();
        tmd.set_val(0);
        return;
    }

    // now finally, we can move on to mov-balanced LOOPS
    if(!root.basic_children) {
        tape_metadata tmd_dup = tmd; // deep copy, apparently
        nuke_touched_values(root, tmd_dup);
        simplified_pass(root, tmd_dup);
        
        // if the loop is effectively an if-statement
        cell_value cell_at_mp = tmd_dup.get_val();
        if(cell_at_mp.known && cell_at_mp.val == 0) {
            tmd_dup = tmd;
            for(node& child : root.children) {
                main_pass(child, tmd_dup);
            }
            tmd = tmd && tmd_dup;
            tmd.set_val(0);
            return;
        }

        tmd = tmd && tmd_dup;
        tmd_dup = tmd;
        for(node& child : root.children) {
            main_pass(child, tmd_dup);
        }
        tmd.set_val(0);  // because like... duh
        return;
    }

    // first we nuke everything except SET, ACCUMA, and MACMA
    
    tape_metadata tmd_dup = tmd;
    for(node& child : root.children) {
        uint8_t opc = child.cmd.opc;
        int32_t aux = child.cmd.aux;
        if(opc == MOV) {
            tmd_dup.mp_pos += aux;
        } else if(opc == ADD) {
            cell_value cell = tmd_dup.get_val();
            if(cell.known) {
                tmd_dup.set_val(cell.val + aux);
            } else {
                tmd_dup.mark_unknown();
            }
        } else if(opc == IN) {
            tmd_dup.mark_unknown();
        } else if(opc == OUT || opc == PUTA || opc == RKILL || opc == OUTC || opc == NOP || opc == HLT) {
            // do nothing
        } else if(opc == SET) {
            tmd_dup.set_val(aux);
            // tmd_dup.mark_unknown();
        } else if(opc == ACCUMA || opc == MACMA) {
            // now, we _could_ simulate the registers, however, the registers allow a
            // cell to be modified by another cell, which would require possibly multiple
            // passes to resolve, so instead, we nuke it
            tmd_dup.mark_unknown();
        }
    }
    tmd = tmd && tmd_dup;
    tmd.set_val(0);
}

void nop_compaction(node& root) {
    // removes NOPs non-recursively from a node with basic children
    if(root.type == OP || !root.basic_children) {
        nuke("this isn't even valid\n");
    }


    // now a trivial pass to clear NOPs
    size_t src = 0;
    size_t dst = 0;
    // find first NOP
    for(node& child : root.children) {
        src++;
        if(child.cmd.opc == NOP) {
            break;
        }
        dst++;
    }
    if(src == dst) {return;}
    for(;src < root.children.size(); src++) {
        uint8_t opc = root.children[src].cmd.opc;
        if(opc == NOP) {continue;}

        root.children[dst].cmd = root.children[src].cmd;
        dst++;
    }
    root.children.resize(dst);

}

void dead_write_elimination(node& root) {
    // works on basic SEQ/LOOPs, and removes redundant operations

    if(root.type == OP) {
        nuke("you can't be serious\n");
    }

    if(!root.basic_children) {
        for(node& child : root.children) {
            dead_write_elimination(child);
        }
        return;
    }

    // now, working only on loops with basic children

    // does livenss-checking on registers, where PUTA, and RKILL end A's lif, and PUTB, NCRAB,
    // MULRAB, and RKILL end B's life, and ACCUMA, MACMA, NCRAB, and MULRAB revive A, and
    // ACCUMB, MACMB revive B
    // we do liveness checking on registers first to eliminate PUTA and PUTB, which revive cells
    
    // MOV, ADD, OUT, IN, SET, NOP, HLT
    bool regA_alive = true;
    for(int i = root.children.size() - 1; i >= 0; i--) {
        uint8_t opc = root.children[i].cmd.opc;
        int32_t aux = root.children[i].cmd.aux;
        if(opc == PUTA) {
            if(!regA_alive) {
                root.children[i].cmd.opc = NOP; // if it's dead, remove the operation
            }
            regA_alive = false;
        } else if(opc == RKILL) {
            regA_alive = false;
        } else if(opc == ACCUMA || opc == MACMA) {
            regA_alive = true;
        } else if(opc == MOV || opc == ADD || opc == OUT || opc == IN || opc == SET
            || opc == OUTC || opc == NOP || opc == HLT) {
            // do nothing
        }
    }
    
    // does liveness-checking on cells, where IN and SET end a cell's life,
    // and OUT, PUTA, PUTB, ACCUMB revive cells
    unordered_set<int32_t> dead;
    int32_t mp = 0;
    // reverse pass, kill cells on seeing IN and SET, unkill cells on seeing OUT, PUTA, PUTB,
    // and since cells may be used later, they all start out as alive (duh)
    for(int i = root.children.size() - 1; i >= 0; i--) {
        uint8_t opc = root.children[i].cmd.opc;
        int32_t aux = root.children[i].cmd.aux;
        if(opc == IN || opc == SET) {
            if(dead.find(mp) != dead.end()) {
                root.children[i].cmd.opc = NOP;
            } else {
                dead.insert(mp);
            }
        } else if(opc == OUT || opc == PUTA) {
            dead.erase(mp);
        } else if(opc == MOV) {
            mp -= aux;  // negative because we're doing a reverse iteration, although it
                        // doesn't actually matter at all since > and < are symmetric in BF
        } else if(opc == ADD || opc == ACCUMA || opc == MACMA) {
            if(dead.find(mp) != dead.end()) {
                root.children[i].cmd.opc = NOP; // remove these
            }
        } else if(opc == RKILL || opc == OUTC || opc == NOP
            || opc == HLT) {
            // do nothing
        }
    }

    nop_compaction(root);
}

void strength_reduction(node& root) {
    if(root.type == OP) {
        nuke("why must you do this to me\n");
    } // should never happen

    if(!root.basic_children) {
        for(node& child : root.children) {
            strength_reduction(child);
        }
        return;
    }

    // removes MOV 0, ADD 0, and MACMA 0
    for(node& child : root.children) {
        uint8_t opc = child.cmd.opc;
        uint32_t aux = child.cmd.aux;
        if(opc == MOV || opc == ADD || opc == MACMA) {
            if(aux == 0) {
                child.cmd.opc = NOP;
            }
        }
    }
    // concatenates consecutive ADDs and MOVs
    size_t prev_instr = 0; // everything between these two should always be a register-only
    size_t curr_instr = 1; // instruction or a NOP
    while(curr_instr < root.children.size()) {
        uint8_t& prev_opc = root.children[prev_instr].cmd.opc;
        uint8_t& curr_opc = root.children[curr_instr].cmd.opc;
        int32_t& prev_aux = root.children[prev_instr].cmd.aux;
        int32_t& curr_aux = root.children[curr_instr].cmd.aux;
        if(prev_opc == MOV && curr_opc == MOV) { // concatenate
            prev_aux = prev_aux + curr_aux;
            curr_opc = NOP;
            if(prev_aux == 0) {
                prev_opc = NOP;
                while(prev_instr > 0) {
                    prev_instr--;
                    if(root.children[prev_instr].cmd.opc != NOP) {
                        break;
                    }
                }
                if(root.children[prev_instr].cmd.opc == NOP) { // => prev_instr == 0
                    prev_instr = curr_instr;
                }
            }
            curr_instr++;
        } else if(prev_opc == ADD && curr_opc == ADD) {
            prev_aux = (uint8_t)(prev_aux + curr_aux);
            curr_opc = NOP;
            if(prev_aux == 0) {
                prev_opc = NOP;
                while(prev_instr > 0) {
                    prev_instr--;
                    if(root.children[prev_instr].cmd.opc != NOP) {
                        break;
                    }
                }
                if(root.children[prev_instr].cmd.opc == NOP) {
                    prev_instr = curr_instr;
                }
            }
            curr_instr++;
        } else if(curr_opc == RKILL || curr_opc == OUTC
            || curr_opc == NOP) {
            curr_instr++;
        } else {
            prev_instr = curr_instr;
            curr_instr++;
        }
    }

    nop_compaction(root);
}





int optimize(vector<command>& commands, bool tape_empty) {
    // first pass, it groups together (+/-) and (>/<)
    size_t add_chain_start = 0;
    size_t mov_chain_start = 0;
    bool mov_chain = false;
    bool add_chain = false;
    uint8_t opc;
    int32_t aux;
    for(size_t i = 0; i < commands.size(); i++) {
        opc = commands[i].opc;
        aux = commands[i].aux;
        if(opc == ADD) {
            if(mov_chain) {
                // if it's in the middle of a MOV chain, check for cases 
                // like +++>><<+++ where the MOV becomes a MOV 0 or NOP,
                // so it can be interrupted)
                mov_chain = false;
                if(commands[mov_chain_start].aux == 0) {
                    commands[mov_chain_start].opc = NOP;
                    add_chain = true;
                } else {
                    add_chain = false;
                }
            }
            if(!add_chain) {
                add_chain_start = i;
                add_chain = true;
            } else {
                commands[add_chain_start].aux += aux;
                commands[i].opc = NOP;
            }
        } else if(opc == MOV) {
            if(add_chain) {
                add_chain = false;
                if(commands[add_chain_start].aux == 0) {
                    commands[add_chain_start].opc = NOP;
                    mov_chain = true;
                } else {
                    mov_chain = false;
                }
            }
            if(!mov_chain) {
                mov_chain_start = i;
                mov_chain = true;
            } else {
                commands[mov_chain_start].aux += aux;
                commands[i].opc = NOP;
            }
        } else {
            add_chain = false;
            mov_chain = false;
        }
    }

    clear_nop(commands);
    commands.shrink_to_fit();
    
    node root = treeify(commands, 0, commands.size());

    translational_affine_loop_solving(root);

    flatten(root);

    tape_metadata tmd;
    tmd.out_of_range_zero = true || tape_empty;
    main_pass(root, tmd);
    
    flatten(root);
    
    dead_write_elimination(root);
    
    flatten(root);
    
    strength_reduction(root);

    commands.clear();
    linearize(root, commands);

    return 0;
}