#include <stdio.h>
#include <stdlib.h>
#define assert(expr) if(!(expr)) { printf("%s:%d %s() %s\n",__FILE__,__LINE__, __func__, #expr); *(volatile int *)0 = 0; }

#include<stdint.h>

typedef uint8_t u8;

struct Instruction {
    u8 w   : 1;
    u8 d   : 1;
    u8 opp : 6;
    u8 rm  : 3;
    u8 reg : 3;
    u8 mod : 2;
};

struct Op {
    u8 bits;
    char str[4];
};

struct Register{
    u8 bits;
    char w_table[2][3];
};

Op opps[] = {
    {0b100010,  "mov"}
};
Register registers[] = {
    {0b000, {"al", "ax"}},
    {0b001, {"cl", "cx"}},
    {0b010, {"dl", "dx"}},
    {0b011, {"bl", "bx"}},
    {0b100, {"ah", "sp"}},
    {0b101, {"ch", "bp"}},
    {0b110, {"dh", "si"}},
    {0b111, {"bh", "di"}}
};

int main() {
    FILE * f = fopen ("mov", "r");
    assert(f);
    fseek (f, 0, SEEK_END);
    long int length = ftell (f); // get current position in stream
    fseek (f, 0, SEEK_SET);
    char * buf = (char *)malloc (length);
    fread (buf, length, 1, f);
    fclose (f);
    printf("bits 16\n\n");
    for(int i = 0; i < length; i+=2) {
        Instruction *inst = (Instruction *) &buf[i];
        for(int i = 0; i < sizeof(opps) / sizeof(Op); ++i) {
            if (opps[i].bits == inst->opp) {
                printf("%s ", opps[i].str);
            }
        } 

        u8 rm_idx = inst->rm;
        assert(registers[rm_idx].bits == rm_idx);
        u8 reg_idx = inst->reg;
        assert(registers[reg_idx].bits == reg_idx);
        
        u8 dest_idx;
        u8 src_idx;
        if (inst->d == 0) {
            dest_idx =  rm_idx;
            src_idx  =  reg_idx;
        }
        if (inst->d == 1) {
            dest_idx =  reg_idx;
            src_idx  =  rm_idx;
        }
        printf("%s, %s\n", 
            registers[dest_idx].w_table[inst->w],
            registers[src_idx].w_table[inst->w]);
    }
    return 0;
}
