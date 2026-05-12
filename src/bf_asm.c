#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

void run(uint8_t*, uint64_t);


int main(int argc, char** argv) {
    uint64_t mem_size_m_1 = ((uint64_t)1 << 15) - 1;
    uint8_t* tape;
    tape = calloc(mem_size_m_1 + 1, sizeof(uint8_t));
    if(!tape) {
        printf("Could not allocate tape\n");
        return 1;
    }

    run(tape, mem_size_m_1);
    
    free(tape);
    return 0;
}
