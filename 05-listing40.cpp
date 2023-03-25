#include <stdio.h>
#include <stdlib.h>
#define assert(expr) if(!(expr)) { printf("%s:%d %s() %s\n",__FILE__,__LINE__, __func__, #expr); *(volatile int *)0 = 0; }

#include<stdint.h>
typedef int8_t  s8;   // 1 byte printed values
typedef int16_t s16;  //  2 byte printed values
typedef uint8_t u8;   // internal representation of 1 byte 
typedef uint16_t u16; // 2 byte printed address 

int main() {
    FILE * f = fopen ("listing40", "r");
    assert(f);
    fseek (f, 0, SEEK_END);
    long int length = ftell (f); // get current position in stream
    fseek (f, 0, SEEK_SET);
    u8 * mem = (u8 *)malloc (length);
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
        u8 * b        = &mem[mem_idx];
        if (0b1111001 == (b[0] & 0b11111110)>>1) { 
            printf("rep ");
            mem_idx += 1;
        } else if (0b1011 == (b[0] & 0b11110000)>>4) {
            bool w           = (b[0] & 0b00001000) >> 3;
            u8 reg           = (b[0] & 0b00000111) >> 0;
            s16 data         = b[1] & 0x00ff;
            if (w == 1) {
                data        |= (b[2] << 8);
            }
            printf("mov %s, %d; immediate to register\n", registers[reg][w], data);
            mem_idx          += 2 + w;
        } else if(0b101000 == (b[0] & 0b11111100)>>2) {
            bool inverse_d   = (b[0] & 0b00000010) >> 1;
            bool w           = (b[0] & 0b00000001) >> 0;
            u16 addr = b[1] & 0x00ff;
            if (w == 1) {
                addr |= b[2] << 8;
            }
            char arg[2][21] = {"ax", {0}};
            snprintf(arg[1], 20, "[%d]", addr);
            printf("mov %s, %s; accumulator to from memory\n", arg[inverse_d], arg[!inverse_d]);
            mem_idx   += 2 + w;
        } else if(0b11110100 == (b[0] & 0b11111111)) {
            printf("hlt\n");
            mem_idx   += 1;
        } else if(0b100010 == (b[0] & 0b11111100)>>2) {
            bool d     = (b[0] & 0b00000010) >> 1;
            bool w     = (b[0] & 0b00000001) >> 0;
            u8 mod     = (b[1] & 0b11000000) >> 6;
            u8 reg     = (b[1] & 0b00111000) >> 3;
            u8 rm      = (b[1] & 0b00000111) >> 0;
            char arg[2][21]; // used for d conditional at the end
            if (mod == 0b00) {
                if (rm == 0b110) { // this does direct address
                    printf("; direct address 16bit displacement\n");
                    s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
                    snprintf(arg[0], 20, "%s", registers[reg][w]);
                    snprintf(arg[1], 20, "[%d]", disp);
                    mem_idx += 2;
                } else {
                    snprintf(arg[0], 20, "%s", registers[reg][w]);
                    snprintf(arg[1], 20, "[%s]", effective_address[rm]);
                }
            } else if (mod == 0b01) {
                s8 disp = b[2]; 
                snprintf(arg[0], 20, "%s", registers[reg][w]);
                snprintf(arg[1], 20, "[%s + %d]", effective_address[rm], disp);
                mem_idx += mod;
            } else if (mod == 0b10) {
                s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
                snprintf(arg[0], 20, "%s", registers[reg][w]);
                snprintf(arg[1], 20, "[%s + %d]", effective_address[rm], disp);
                mem_idx += mod;
            } else if (mod == 0b11) {
                snprintf(arg[0], 20, "%s", registers[reg][w]);
                snprintf(arg[1], 20, "%s", registers[rm][w]);
            } else {
                assert(0 && "mod fail");
            }
            printf("mov %s, %s; register/memory to/from register\n", arg[!d], arg[d]);
            mem_idx += 2;
        } else if (0b1100011 == (b[0] & 0b11111110)>>1) {
            bool w      = (b[0] & 0b00000001) >> 0;
            u8 mod      = (b[1] & 0b11000000) >> 6;
            u8 rm       = (b[1] & 0b00000111) >> 0;
            s16 data    = 0; // used for word / byte below
            if (mod == 0b00) {
                data = b[2] & 0x00ff;
                printf("mov [%s], ", effective_address[rm]);
                if (w == 1) {
                    data |= b[3] << 8;
                }
            } else if (mod == 0b01) { // this is never called
                assert(0);
                u8 disp = b[2];
                printf("mov [%s + %d], ", effective_address[rm], disp);
                data = b[3] & 0x00ff;
                if (w == 1) {
                    data |= b[4] << 8;
                }
            } else if (mod == 0b10) {
                s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
                printf("mov [%s + %d], ", effective_address[rm], disp);
                data  = b[4] & 0x00ff;
                if (w == 1) {
                    data |= b[5] << 8;
                }
            } else {
                assert(0 && "mod invalid");
            }
            const char * w_str[] = { "byte", "word" };
            printf("%s %d; immediate to register/memory\n", w_str[w], data);
            mem_idx += 3 + w + mod;
        } else {
            assert(0 && "no opp code found");
        }
    } // for
    return 0;
}
