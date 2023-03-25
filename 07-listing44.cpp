#include<stdint.h>
#include<string.h>
typedef int8_t  s8;   // 1 byte printed values
typedef int16_t s16;  //  2 byte printed values
typedef uint8_t u8;   // internal representation of 1 byte 
typedef uint16_t u16; // 2 byte printed address 
/** Mode
 ** mod 00 Memory Mode rm == 110 ? 16-bit disp : no disp    [effective_address] / [memory]
 ** mod 01 Memory Mode 8-bit disp                           [effective_address + disp]
 ** mod 10 Memory Mode 16-bit disp                          [effective_address + disp]
 ** mod 11 Register Mode no disp                            bx, dx
 **/
// data = immediate
// disp = memory
// 
//
// jmp      11101001    ip-inc-lo       ip-inc-hi               direct within segment
// jmp      11101011    ip-inc8                                 direct within segment short
// jmp      11101010    ip-lo           ip-hi                   direct intersegment
//
// jmp      11111111    mod 100 r/m     (disp-lo)   (disp-hi)   indirect within segment
// jmp      11111111    mod 101 r/m     (disp-lo)   (disp-hi)   indirect intersegment
// push     11111111    mod 110 r/m     (disp-lo)   (disp-hi)   Register/memory
// pop      10001111    mod 000 r/m     (disp-lo)   (disp-hi)   Register/memory
//
// mov      100010dw    mod reg r/m     (disp-lo)    (disp-hi)  register/memory to/from register 
// add      000000dw    mod reg r/m     (disp-lo)    (disp-hi)  reg/memory with register to either
// sub      001010dw    mod reg r/m     (disp-lo)    (disp-hi)  reg/memory and register to either
// cmp      001110dw    mod reg r/m     (disp-lo)    (disp-hi)  reg/memory and register to either
//                                 
// mov      1100011w    mod 000 r/m     (disp-lo)    (disp-hi)    data    data if w = 1         immediate to register/memory
// add      100000sw    mod 000 r/m     (disp-lo)    (disp-hi)    data    data if s: w = 01     immediate to register/memory
// sub      100000sw    mod 101 r/m     (disp-lo)    (disp-hi)    data    data if s: w = 01     immediate from register/memory
// cmp      100000sw    mod 111 r/m     (disp-lo)    (disp-hi)    data    data if s: w = 01     immediate with register/memory
//                                 
// mov      101000!dw   addr-lo         addr-hi                memory to accumulator    bit 7 is the inverse direction 0 = to accumulator, 1 = to memory
//                                 
// mov      101000dw    addr-hi         addr-lo                    memory to accumulator if d = 0
//                                 
// add      0000010w    data            data if w= 1                    
// sub      0010110w    data            data if w= 1                immediate from accumulator    
// cmp      0011110w    data            data if w= 1                immediate with accumulator    
//                                 
// mov      1011w reg   data            data if w = 1                immediate to register    mov bx, 34
//
// push     01010 reg                                           Register 
// pop      01011 reg 
//
// push     000 reg 110                                         Segment Register 
// pop      000 reg 111
//
// xchg     1000011w    mod reg r/m     (disp-lo)   (disp-hi)   register/memory with register
// xchg     10010reg    register with accumulator
//
// in       1110010w    data-8      Fixed port
// in       1110110w                Variable port
//
// out      1110011w    data-8      Fixed port
// out      1110111w                Variable Port
//
// xlat     11010111                Translate byte to al
// lea      10001101    mod reg r/m     (disp-lo)   (disp-hi)  load effective address to register 
// les      11000100    mod reg r/m     (disp-lo)   (disp-hi)  load pointer to es


char registers[][2][3] = { // registers[reg][w]                     
    [0b000] = {"al", "ax"},
    [0b001] = {"cl", "cx"},
    [0b010] = {"dl", "dx"},
    [0b011] = {"bl", "bx"},
    [0b100] = {"ah", "sp"},
    [0b101] = {"ch", "bp"},
    [0b110] = {"dh", "si"},
    [0b111] = {"bh", "di"},
};
char effective_address[][8] = { // effective_address[rm]
    [0b000] = "bx + si",
    [0b001] = "bx + di",
    [0b010] = "bp + si",
    [0b011] = "bp + di",
    [0b100] = "si",
    [0b101] = "di",
    [0b110] = "bp",
    [0b111] = "bx",
};

enum reg {
    ax,
    cx,
    dx,
    bx,
    sp,
    bp,
    si,
    di,
    reg_count
};

s16 reg_var[reg_count] = {0};

struct TwoByteCode {
    s16 binary;
    char op[7];
};

TwoByteCode jump_codes[] = {
    { 0b01110101, "jnz"},
    { 0b01110100, "je"},
    { 0b01111100, "jl"},
    { 0b01111110, "jle"},
    { 0b01110010, "jb"},
    { 0b01110110, "jbe"},
    { 0b01111010, "jp"},
    { 0b01110000, "jo"},
    { 0b01111000, "js"},
    { 0b01110101, "jne"},
    { 0b01111101, "jnl"},
    { 0b01111111, "jg"},
    { 0b01110011, "jnb"},
    { 0b01110111, "ja"},
    { 0b01111011, "jnp"},
    { 0b01110001, "jno"},
    { 0b01111001, "jns"},
    { 0b11100010, "loop"},
    { 0b11100001, "loopz"},
    { 0b11100000, "loopnz"},
    { 0b11100011, "jcxz"},
};
#include <stdio.h>
#include <stdlib.h>
#define assert(expr) if(!(expr)) { printf("%s:%d %s() %s\n",__FILE__,__LINE__, __func__, #expr); *(volatile int *)0 = 0; }

#define BYTE_TO_BINARY(byte)  \
    (byte & 0x80 ? '1' : '0'), \
    (byte & 0x40 ? '1' : '0'), \
    (byte & 0x20 ? '1' : '0'), \
    (byte & 0x10 ? '1' : '0'), \
    (byte & 0x08 ? '1' : '0'), \
    (byte & 0x04 ? '1' : '0'), \
    (byte & 0x02 ? '1' : '0'), \
    (byte & 0x01 ? '1' : '0') 

void print_reg(reg Reg) {
    printf("    %s: 0x%.4x (%d)\n", registers[Reg][1], reg_var[Reg], reg_var[Reg]);
}

int main(int argc, char ** argv) {
    assert(argc >= 2);
    assert(argv[1]);
    s8 file_pos = 1;
    u8 exec = 0;
    if (argc == 3) {
        assert(argv[2]);
        if (strcmp(argv[1], "-exec") == 0) {
            file_pos = 2;
            exec = 1;
        }
    }
    FILE * f = fopen (argv[file_pos], "r");
    assert(f);
    fseek (f, 0, SEEK_END);
    long int length = ftell (f); // get current position in stream
    fseek (f, 0, SEEK_SET);
    u8 * mem = (u8 *)malloc (length);
    fread (mem, length, 1, f);
    fclose (f);
    printf("bits 16\n\n");
    for(int mem_idx = 0; mem_idx < length;) {
        u8 * b        = &mem[mem_idx];
        /*
        printf("%c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c\n",
            BYTE_TO_BINARY(b[0]), BYTE_TO_BINARY(b[1]), BYTE_TO_BINARY(b[2]), BYTE_TO_BINARY(b[3]), 
            BYTE_TO_BINARY(b[4]));
            */
        if (0b1111001 == (b[0] & 0b11111110)>>1) {  // rep
            printf("rep ");
            mem_idx += 1;
        } else if (0b1011 == (b[0] & 0b11110000)>>4) { // mov immediate to register
            bool w           = (b[0] & 0b00001000) >> 3;
            u8 reg           = (b[0] & 0b00000111) >> 0;
            s16 immed        = (s8) b[1];
            if (w == 1) {
                immed       = (immed & 0x00ff) | (b[2] << 8);
            }
            printf("mov %s, %d ; ", registers[reg][w], immed);
            s16 before = reg_var[reg];
            reg_var[reg] = immed;
            s16 after = reg_var[reg];
            if (exec) {
                printf("%s:0x%x->0x%x", registers[reg][w], before, after);
            } else { 
                printf("immediate to register");
            }
            printf("\n");
            mem_idx          += 2 + w;
        } else if( 0b001011 == (b[0] & 0b11111100) >> 2 ||  // sub immediate to accumulator 
                 ( 0b000001 == (b[0] & 0b11111100) >> 2)||  // add immeidate to accumulator
                 ( 0b101000 == (b[0] & 0b11111100) >> 2)||  // mov memory <-> accumulator
                 ( 0b001111 == (b[0] & 0b11111100) >> 2)) { // cmp accumulator to register
            bool inverse_d   = (b[0] & 0b00000010) >> 1;
            bool w = b[0] & 0b00000001;
            s16 data = (s8) b[1];
            if (w == 1) {
                data &= 0x00ff;
                data |= b[2] << 8;
            }
            char arg[2][21] = {{0}};
            snprintf(arg[0], 20, "%s", registers[0b000][w]);
            s8 binary = (b[0] & 0b11111100) >> 2;
            if(0b001111 == binary) { 
                printf("cmp ");
                snprintf(arg[1], 20, "%d", data);
            } else if (0b001011 == binary) {
                printf("sub ");
                snprintf(arg[1], 20, "%d", data);
            } else if (0b000001 == binary) {
                printf("add ");
                snprintf(arg[1], 20, "%d", data);
            } else if (0b101000 == binary) {
                printf("mov ");
                snprintf(arg[1], 20, "[%d]", data);
            }
            printf("%s, %s; accumulator to from memory\n", arg[inverse_d], arg[!inverse_d]);
            mem_idx += 2 + w;
        } else if ( 0b100010 == (b[0] & 0b11111100) >> 2   ||  // mov register/memory <-> register
                   (0b000000 == (b[0] & 0b11111100) >> 2 ) ||  // add
                   (0b001010 == (b[0] & 0b11111100) >> 2 ) ||  // sub
                   (0b001110 == (b[0] & 0b11111100) >> 2 ) ) { // cmp 
            bool d     = (b[0] & 0b00000010) >> 1;
            bool w     = (b[0] & 0b00000001) >> 0;
            u8 mod     = (b[1] & 0b11000000) >> 6;
            u8 reg     = (b[1] & 0b00111000) >> 3;
            u8 rm      = (b[1] & 0b00000111) >> 0;

            s8 opp_type = (b[0] & 0b00111000) >> 3;
            if (0b000 == opp_type) {
                printf("add ");
            } else if (0b001 == opp_type) {
                printf("mov ");
            } else if (0b101 == opp_type) {
                printf("sub ");
            } else if (0b111 == opp_type) {
                printf("cmp ");
            }

            char arg[2][21]; // used for d conditional at the end

            char comment[40] = {0};
            if (!exec) {
                strcpy(comment, "register/memory to/from register");
            }
            if (mod == 0b00) {
                if (rm == 0b110) { // this does direct address
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
                s16 before = reg_var[rm];
                reg_var[rm] = reg_var[reg];
                s16 after = reg_var[rm];
                if (exec) {
                    snprintf(comment, 19, "%s:0x%x->0x%x", registers[rm][w], before, after);
                }
                snprintf(arg[0], 20, "%s", registers[reg][w]);
                snprintf(arg[1], 20, "%s", registers[rm][w]);
            } else {
                assert(0 && "mod fail");
            }

            printf("%s, %s ; %s\n", arg[!d], arg[d], comment);
            mem_idx += 2;
        } else if (0b1100011 == (b[0] & 0b11111110)>>1) { // immediate to register/memory 
                                                          // mov, add, sub, cmp
            bool s      = (b[0] & 0b00000010) >> 1;
            bool w      = (b[0] & 0b00000001) >> 0;
            u8 mod      = (b[1] & 0b11000000) >> 6;
            u8 middle   = (b[1] & 0b00111000) >> 3;
            u8 rm       = (b[1] & 0b00000111) >> 0;
            if (0b1100011 == (b[0] & 0b11111110)>>1) {
                printf("mov ");
            } else {
                if (middle == 0b000) {
                    printf("add ");
                } else if (middle == 0b101) {
                    printf("sub ");
                } else if (middle == 0b111) {
                    printf("cmp ");
                }
            }
            const char * w_str[] = { "byte", "word" };
            printf("%s ", w_str[w]);

            s16 data    = 0; // used for word / byte below
            if (mod == 0b00) {
                if (rm == 0b110) { // 16 bit disp
                    s16 disp = (b[2] & 0x00ff) | (b[3] << 8) ;
                    printf("[%d], ", disp); // change
                    data = b[4];
                    mem_idx += 5; // 2 bytes + 2 disp + 1 data
                } else  { // no disp
                    printf("[%s], ", effective_address[rm]);
                    data = (s8) b[2];
                    if (w == 1) {
                        data = (data & 0x00ff) | b[3] << 8;
                    }
                    mem_idx += 3 + w;
                }
            } else if (mod == 0b01) { // this is never called
                assert(0);
                u8 disp = b[2];
                printf("[%s + %d], ", effective_address[rm], disp);
                data = b[3] & 0x00ff;
                if (w == 1) {
                    data |= b[4] << 8;
                }
                mem_idx += 3 + w + mod;
            } else if (mod == 0b10) {
                s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
                printf("[%s + %d], ", effective_address[rm], disp);
                data  = (s8) b[4];
                if (w == 1) {
                    data = (data & 0x00ff) | b[5] << 8;
                }
                mem_idx += 3 + w + mod;
            } else if (mod == 0b11) {
                printf("%s, ", registers[rm][w]);
                data  = b[2] & 0x00ff;
                mem_idx += 3;
            } else {
                assert(0 && "mod invalid");
            }
            if (s) {
                printf("%d; immediate to register/memory\n", data);
            } else {
                printf("%u; immediate to register/memory\n", data);
            }
        } else if (0b100000 == (b[0] & 0b11111100)>>2) {// immediate -> register/memory add, sub, cmp
                                                        // compress
            // this is slightly different from mov due to the placement of byte / word
            bool s      = (b[0] & 0b00000010) >> 1;
            bool w      = (b[0] & 0b00000001) >> 0;
            u8 mod      = (b[1] & 0b11000000) >> 6;
            u8 middle   = (b[1] & 0b00111000) >> 3;
            u8 rm       = (b[1] & 0b00000111) >> 0;

            if (middle == 0b000) {
                printf("add ");
            } else if (middle == 0b101) {
                printf("sub ");
            } else if (middle == 0b111) {
                printf("cmp ");
            } else {
                printf("middle %d\n", middle);
                assert(0 && "middle invalid");
            }

            const char * w_str[] = { "byte", "word" };
            printf("%s ", w_str[w]);

            s16 data    = 0; // used for word / byte below
            if (mod == 0b00) { // memory mode no disp
                if (rm == 0b110) { // 16 bit disp
                    s16 disp = (b[2] & 0x00ff) | (b[3] << 8) ;
                    printf("[%d], ", disp); // change
                    data = b[4];
                    mem_idx += 5; // 2 bytes + 2 disp + 1 data
                } else  { // no disp
                    printf("[%s], ", effective_address[rm]); // change
                    data = (s8) b[2];
                    mem_idx += 3; // 2 bytes + 1 data
                }
            } else if (mod == 0b01) { // this is never called
                assert(0);
                u8 disp = b[2];
                printf("[%s + %d], ", effective_address[rm], disp); // change
                data = b[3] & 0x00ff;
                if (w == 1) {
                    data |= b[4] << 8;
                }
                mem_idx += 3 + w + mod;
            } else if (mod == 0b10) {
                s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
                printf("[%s + %d], ", effective_address[rm], disp);
                data  = (s8) b[4];
                if (w == 1) {
                    // w is always 1?
                    // for some reason, this has to be commented out?
                    //data = (data & 0x00ff) | b[5] << 8;
                }
                mem_idx += 4 + 1;
            } else if (mod == 0b11) {
                printf("%s, ", registers[rm][w]); 
                data  = b[2] & 0x00ff;
                mem_idx += 3;
            } else {
                assert(0 && "mod invalid");
            }
            if (s && w) {
                printf("%d; immediate to register/memory\n", data);
            } else {
                printf("%u; immediate to register/memory\n", data);
            }
        } else {
            int code_idx = 0;
            int len = sizeof(jump_codes)/sizeof(TwoByteCode);
            TwoByteCode code;
            for (; code_idx < len; ++code_idx) {
                code = jump_codes[code_idx];
                if (code.binary == b[0]) {
                    break;
                }
            }
            if( code_idx < len) {
                s8 ip_inc8 = b[1];
                printf("%s %d\n", code.op, ip_inc8);
                mem_idx += 2;
            } else {
                printf("%c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c\n",
                    BYTE_TO_BINARY(b[0]), BYTE_TO_BINARY(b[1]), BYTE_TO_BINARY(b[2]), BYTE_TO_BINARY(b[3]), 
                    BYTE_TO_BINARY(b[4]));
                assert(0 && "no opp code found");
            }
        }
    } // for
    if (exec) {
        printf("\nFinal registers:\n");
        print_reg(ax);
        print_reg(bx);
        print_reg(cx);
        print_reg(dx);
        print_reg(sp);
        print_reg(bp);
        print_reg(si);
        print_reg(di);
    }
    return 0;
}
