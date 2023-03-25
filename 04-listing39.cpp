#include <stdio.h>
#include <stdlib.h>
#define assert(expr) if(!(expr)) { printf("%s:%d %s() %s\n",__FILE__,__LINE__, __func__, #expr); *(volatile int *)0 = 0; }
int main() {
    FILE * f = fopen ("listing39", "r");
    assert(f);
    fseek (f, 0, SEEK_END);
    long int length = ftell (f); // get current position in stream
    fseek (f, 0, SEEK_SET);
    char * mem = (char *)malloc (length);
    fread (mem, length, 1, f);
    fclose (f);
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
    char effective_address[][8]= {
        [0b000] = "bx + si",
        [0b001] = "bx + di",
        [0b010] = "bp + si",
        [0b011] = "bp + di",
        [0b100] = "si",
        [0b101] = "di",
        [0b110] = "bp",
        [0b111] = "bx",
    };
    printf("bits 16\n\n");
    for(int mem_idx = 0; mem_idx < length;) {
        char b1 = mem[mem_idx];
        if((b1 & 0b11111100) >> 2 == 0b100010) {
            // register/memory to/from register
            char b2     = mem[mem_idx+1];
            int d       = (b1 & 0b00000010) >> 1;
            int w       = (b1 & 0b00000001) >> 0;
            int mod     = (b2 & 0b11000000) >> 6;
            int reg     = (b2 & 0b00111000) >> 3;
            int rm      = (b2 & 0b00000111) >> 0;
            if (mod == 0b11) {
                // reg to reg
                if (d) {
                    printf("mov %s, %s\n", registers[reg][w], registers[rm][w]);
                } else {
                    printf("mov %s, %s\n", registers[rm][w], registers[reg][w]);
                }
                mem_idx += 2;
            } else {
                int data = 0;
                if (mod == 0b00) {
                    if (rm == 0b110) {
                        // this does direct address
                        assert(0 && "direct address");
                    }
                    // this will end up with no data, just an effective address
                } else if (mod == 0b01) {
                    data = mem[mem_idx + 2] & 0b11111111;
                } else if (mod == 0b10){
                    int b3   = mem[mem_idx+2] & 0b11111111;
                    int b4   = mem[mem_idx+3] & 0b11111111;
                    data     = b3;
                    data    |= b4 << 8;
                } else {
                    assert(0 && "mod fail");
                }

                if (data) {
                    if (d) {
                        printf("mov %s, [%s + %d]\n", registers[reg][w], effective_address[rm], data);
                    } else {
                        printf("mov [%s + %d], %s\n", effective_address[rm], data, registers[reg][w]);
                    }
                } else {
                    if (d) {
                        printf("mov %s, [%s]\n", registers[reg][w], effective_address[rm]);
                    } else {
                        printf("mov [%s], %s\n",  effective_address[rm], registers[reg][w]);
                    }
                }
                mem_idx += 2 + mod;
            }
        } else if(((b1 & 0b11110000) >> 4) == 0b1011) {
            // immediate to register
            int w           = (b1 & 0b00001000) >> 3;
            int reg         = (b1 & 0b00000111) >> 0;
            int b2           = mem[mem_idx+1] & 0b11111111;
            int data        = b2;
            if (w == 1) {
                int b3      = mem[mem_idx+2] & 0b11111111;
                data       |= b3 << 8;
            }
            printf("mov %s, %d\n", registers[reg][w], data);
            mem_idx        += 2 + w;
        } else {
            assert(0 && "no opp code found");
        }
    } // for
    return 0;
}
