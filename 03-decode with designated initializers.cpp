#include <stdio.h>
#include <stdlib.h>
#define assert(expr) if(!(expr)) { printf("%s:%d %s() %s\n",__FILE__,__LINE__, __func__, #expr); *(volatile int *)0 = 0; }
struct Instruction {
    int opp;
    int d;
    int w;
    int mod;
    int regrm[2];
};
struct Op {
    int bits;
    char str[4];
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
    Op opps[] = {
        {0b100010,  "mov"}
    };
    char registers[][2][3]= {
        [0b000] = {"al", "ax"},
        [0b001] = {"cl", "cx"},
        [0b010] = {"dl", "dx"},
        [0b011] = {"bl", "bx"},
        [0b100] = {"ah", "sp"},
        [0b101] = {"ch", "bp"},
        [0b110] = {"dh", "si"},
        [0b111] = {"bh", "di"},
    };
    printf("bits 16\n\n");
    for(int i = 0; i < length; i+=2) {
        char b1 = buf[i];
        char b2 = buf[i+1];
        Instruction inst =  {
            .opp        = (b1 & 0b11111100) >> 2,
            .d          = (b1 & 0b00000010) >> 1,
            .w          = (b1 & 0b00000001) >> 0,
            .mod        = (b2 & 0b11000000) >> 6,
            .regrm[0]   = (b2 & 0b00111000) >> 3,
            .regrm[1]   = (b2 & 0b00000111) >> 0,
        };
        for(int i = 0; i < sizeof(opps) / sizeof(Op); ++i) {
            if (opps[i].bits == inst.opp) {
                printf("%s ", opps[i].str);
            }
        } 
        int dest    = inst.regrm[!inst.d];
        int src     = inst.regrm[inst.d];
        printf("%s, %s\n", registers[dest][inst.w], registers[src][inst.w]);
    }
    return 0;
}
