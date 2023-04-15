#include<stdint.h>
#include<string.h>
typedef int8_t  s8;   // 1 byte printed values
typedef int16_t s16;  //  2 byte printed values
typedef uint8_t u8;   // internal representation of 1 byte 
typedef uint16_t u16; // 2 byte printed address 
// data = immediate
// disp = memory

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

u8 zero_flag = 0;
u8 sign_flag = 0;
u8 parity_flag = 0;
s16 reg_value[reg_count] = {0};


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
    if (reg_value[Reg]) {
        printf("    %s: 0x%hx (%hu)\n", registers[Reg][1], reg_value[Reg], reg_value[Reg]);
    }
}

void gen_flags_str(char * flags_str) {
    if (parity_flag || zero_flag || sign_flag)  {
        snprintf(flags_str, 20,"%s%s%s", 
                parity_flag ? "P" : "",
                zero_flag ? "Z" : "", 
                sign_flag ? "S" : ""
                );
    }
}
void set_flags(s16 val) {
    int parity_count = 0;
    if (val & 0b00000001) parity_count++; 
    if (val & 0b00000010) parity_count++; 
    if (val & 0b00000100) parity_count++; 
    if (val & 0b00001000) parity_count++; 
    if (val & 0b00010000) parity_count++; 
    if (val & 0b00100000) parity_count++; 
    if (val & 0b01000000) parity_count++; 
    if (val & 0b10000000) parity_count++; 
    
    if (parity_count % 2 == 0) {
        parity_flag = 1;
    } else {
        parity_flag = 0;
    }
    if (val == 0) {
        zero_flag = 1;
    } else {
        zero_flag = 0;
    }
    if ((val & 0x8000)>> 15 == 1) {
        sign_flag = 1;
    } else {
        sign_flag = 0;
    }
}
const char op_str[][10] = {
    "op_none",
    "op_mov",
    "op_cmp",
    "op_sub",
    "op_add"
};
enum Op {
    op_none,
    op_mov,
    op_cmp,
    op_sub,
    op_add
};

enum Type {
    t_none,
    t_rep,
    t_mov_immed_to_reg,
    t_to_accumulator,
    t_immed_to_reg_mem,
    t_reg_to_reg_mem,
    t_asc_immed_to_reg_mem
};


enum Mode {
    mode_none,
    mode_16bit_mem,
    mode_effective_addr,
    mode_effective_addr_8bit_mem,
    mode_effective_addr_16bit_mem,
    mode_register
};

const char  mode_str[][40] = {
    "mode_none",
    "mode_16bit_mem",
    "mode_effective_addr",
    "mode_effective_addr_8bit_mem",
    "mode_effective_addr_16bit_mem",
    "mode_register"
};

struct Inst {
    Type type;
    bool w;
    bool d;
    bool s;
    u8 reg;
    u8 rm;
    Op op;
    s16 immed;
    s16 mem; // disp (check mode)
    Mode mode;
};

/** Mode
 ** mod 00 Memory Mode rm == 110 ? 16-bit disp : no disp    [effective_address] / [memory]
 ** mod 01 Memory Mode 8-bit disp                           [effective_address + disp]
 ** mod 10 Memory Mode 16-bit disp                          [effective_address + disp]
 ** mod 11 Register Mode no disp                            bx, dx
 **/

int generate_instruction( u8 * mem, int mem_idx, Inst *inst) {
    u8 * b        = &mem[mem_idx];
    if (0b1111001 == (b[0] & 0b11111110)>>1) {  // t_rep
        inst->type = t_rep; 
        mem_idx += 1;
    } else if (0b1011 == (b[0] & 0b11110000)>>4) { // t_mov_immed_to_reg
        bool w         = (b[0] & 0b00001000) >> 3;
        u8 reg         = (b[0] & 0b00000111) >> 0;
        s16 immed      = (s8)b[1];
        if (w == 1) {
            immed       = (immed & 0x00ff) | (b[2] << 8);
        }
        inst->type      = t_mov_immed_to_reg;
        inst->w         = w;
        inst->reg       = reg;
        inst->immed     = immed;
        mem_idx          += 2 + w;
    } else if( 0b001011 == (b[0] & 0b11111100) >> 2 ||  // sub immediate to accumulator t_to_accumulator
             ( 0b000001 == (b[0] & 0b11111100) >> 2)||  // add immeidate to accumulator
             ( 0b101000 == (b[0] & 0b11111100) >> 2)||  // mov memory <-> accumulator
             ( 0b001111 == (b[0] & 0b11111100) >> 2)) { // cmp accumulator to register

        inst->type      = t_to_accumulator;
        bool inverse_d   = (b[0] & 0b00000010) >> 1;
        inst->d         = !inverse_d;
        bool w = b[0] & 0b00000001;
        inst->w         = w;
        s16 immed  = (s8) b[1];
        if (w == 1) {
            immed  &= 0x00ff;
            immed  |= b[2] << 8;
        }
        inst->immed     = immed;
        // could compress this to an array if this is a common pattern
        s8 binary = (b[0] & 0b11111100) >> 2;
        if(0b001111 == binary) { 
            inst->op    = op_cmp;
        } else if (0b001011 == binary) {
            inst->op    = op_sub;
        } else if (0b000001 == binary) {
            inst->op    = op_add;
        } else if (0b101000 == binary) {
            inst->op    = op_mov;
        }
        mem_idx += 2 + w;
    } else if ( 0b100010 == (b[0] & 0b11111100) >> 2   ||  // mov register/memory <-> register // to_reg
               (0b000000 == (b[0] & 0b11111100) >> 2 ) ||  // add
               (0b001010 == (b[0] & 0b11111100) >> 2 ) ||  // sub
               (0b001110 == (b[0] & 0b11111100) >> 2 ) ) { // cmp 
        bool d     = (b[0] & 0b00000010) >> 1;
        bool w     = (b[0] & 0b00000001) >> 0;
        u8 mod     = (b[1] & 0b11000000) >> 6;
        u8 reg     = (b[1] & 0b00111000) >> 3;
        inst->reg = reg;
        u8 rm      = (b[1] & 0b00000111) >> 0;
        inst->rm = rm;

        // needs conversion
        s8 opp_type = (b[0] & 0b00111000) >> 3;
        if (0b000 == opp_type) {
            inst->op = op_add;
        } else if (0b001 == opp_type) {
            inst->op = op_mov;
        } else if (0b101 == opp_type) {
            inst->op = op_sub;
        } else if (0b111 == opp_type) {
            inst->op = op_cmp;
        }
        if (mod == 0b00) {
            if (rm == 0b110) { // this does direct address
               inst->mode = mode_16bit_mem;
                s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
                inst->mem = disp;
                mem_idx += 2;
            } else {
               inst->mode = mode_effective_addr;
            }
        } else if (mod == 0b01) {
            inst->mode = mode_effective_addr_8bit_mem;
            s8 disp = b[2]; 
            inst->mem = disp;
            mem_idx += mod;
        } else if (mod == 0b10) {
            inst->mode = mode_effective_addr_16bit_mem;
            s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
            inst->mem = disp;
            mem_idx += mod;
        } else if (mod == 0b11) {
            inst->mode = mode_register;
        } else {
            assert(0 && "mod fail");
        }

        mem_idx += 2;
        inst->type         = t_reg_to_reg_mem;
        inst->w         = w;
        inst->d         = d;
        inst->rm        = rm;
    } else if (0b1100011 == (b[0] & 0b11111110)>>1) { // immediate to register/memory 
                                                      // mov, add, sub, cmp
        bool s      = (b[0] & 0b00000010) >> 1;
        bool w      = (b[0] & 0b00000001) >> 0;
        u8 mod      = (b[1] & 0b11000000) >> 6;
        u8 middle   = (b[1] & 0b00111000) >> 3;
        u8 rm       = (b[1] & 0b00000111) >> 0;

        s16 immed    = 0; // used for word / byte below
     
        // detect op
        if (0b1100011 == (b[0] & 0b11111110)>>1) {
            inst->op         = op_mov;
        } else {
            if (middle == 0b000) {
                inst->op         = op_add;
            } else if (middle == 0b101) {
                inst->op         = op_sub;
            } else if (middle == 0b111) {
                inst->op         = op_cmp;
            }
        }
        if (mod == 0b00) {
            if (rm == 0b110) { // 16 bit disp
                assert(0); // never used
                inst->mode = mode_16bit_mem;
                s16 disp = (b[2] & 0x00ff) | (b[3] << 8) ;
                inst->mem = disp;
                immed = b[4];
                inst->immed = immed;
                mem_idx += 5; // 2 bytes + 2 disp + 1 data
            } else  { // no disp
                inst->mode = mode_effective_addr;
                immed = (s8) b[2];
                if (w == 1) {
                    immed = (immed & 0x00ff) | b[3] << 8;
                }
                mem_idx += 3 + w;
            }
        } else if (mod == 0b01) { // this is never called
          assert(0);                        
            inst->mode = mode_effective_addr_8bit_mem;
            u8 disp = b[2];
            immed = b[3] & 0x00ff;
            if (w == 1) {
                immed |= b[4] << 8;
            }
            mem_idx += 3 + w + mod;
        } else if (mod == 0b10) {
            inst->mode = mode_effective_addr_16bit_mem;
            s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
            inst->mem = disp;
            immed  = (s8) b[4];
            if (w == 1) {
                immed = (immed & 0x00ff) | b[5] << 8;
            }
            mem_idx += 3 + w + mod;
        } else if (mod == 0b11) {
            inst->mode = mode_register;
            immed  = b[2] & 0x00ff;
            mem_idx += 3;
        } else {
            assert(0 && "mod invalid");
        }
        inst->type      = t_immed_to_reg_mem;
        inst->w         = w;
        inst->rm        = rm;
        inst->s         = s;
        inst->immed     = immed;
    } else if (0b100000 == (b[0] & 0b11111100)>>2) {// immediate -> register/memory add, sub, cmp
                                                    // compress
        inst->type = t_asc_immed_to_reg_mem;
        // this is slightly different from mov due to the placement of byte / word
        bool s      = (b[0] & 0b00000010) >> 1;
        inst->s = s;
        bool w      = (b[0] & 0b00000001) >> 0;
        inst->w = w;
        u8 mod      = (b[1] & 0b11000000) >> 6;
        u8 middle   = (b[1] & 0b00111000) >> 3;
        u8 rm       = (b[1] & 0b00000111) >> 0;
        inst->rm = rm;
        if (middle == 0b000) {
            inst->op = op_add;
        } else if (middle == 0b101) {
            inst->op = op_sub;
        } else if (middle == 0b111) {
            inst->op = op_cmp;
        } else {
            assert(0 && "middle invalid");
        }

        s16 immed    = 0; // used for word / byte below
        if (mod == 0b00) { // memory mode no disp
            if (rm == 0b110) { // 16 bit disp
               inst->mode = mode_16bit_mem;
                s16 mem = (b[2] & 0x00ff) | (b[3] << 8) ;
                inst->mem = mem;
                immed = b[4];
                mem_idx += 5; // 2 bytes + 2 disp + 1 immed
            } else  { // no disp
               inst->mode = mode_effective_addr;
                immed = (s8) b[2];
                mem_idx += 3; // 2 bytes + 1 immed
            }
        } else if (mod == 0b01) { // this is never called
            inst->mode = mode_effective_addr_8bit_mem;
            assert(0);
            u8 mem = b[2];
            inst->mem = mem;
            immed = b[3] & 0x00ff;
            if (w == 1) {
                immed |= b[4] << 8;
            }
            mem_idx += 3 + w + mod;
        } else if (mod == 0b10) {
            inst->mode = mode_effective_addr_16bit_mem;
            s16 mem = (b[3] << 8) | (b[2] & 0x00ff);
            inst->mem = mem;
            immed  = (s8) b[4];
            if (w == 1) {
                // for some reason, this has to be commented out?
                //immed = (immed & 0x00ff) | b[5] << 8;
            }
            mem_idx += 4 + 1;
        } else if (mod == 0b11) {
            inst->mode = mode_register;
            // listing 46 requires this:
            immed = b[2] & 0x00ff;
            // listing47 requires this:
            //immed = (s8) b[2];
            //if (w == 1 && s == 1) {
            if (w == 1 && s == 0) {
                immed |= b[3] << 8;
            }
            mem_idx += 3;
            if (w == 1 && s == 0) {
                mem_idx += 1;
            }
        } else {
            assert(0 && "mod invalid");
        }
        inst->immed = immed;

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
            assert(0 && "disabled twobytecodes for now");
            s8 ip_inc8 = b[1];
            //printf("%s %d\n", code.op, ip_inc8);
            mem_idx += 2;
        } else {
            printf("%c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c\n",
                BYTE_TO_BINARY(b[0]), BYTE_TO_BINARY(b[1]), BYTE_TO_BINARY(b[2]), BYTE_TO_BINARY(b[3]), 
                BYTE_TO_BINARY(b[4]));
            assert(0 && "no opp code found");
        }
    }
    return mem_idx;
}


void print_instruction(Inst inst, char * debug_str) {
    bool w         = inst.w;
    bool d         = inst.d;
    bool s         = inst.s;
    Mode mode       = inst.mode;
    u8 reg         = inst.reg;
    u8 rm         = inst.rm;
    s16 immed      = inst.immed;
    s16 mem         = inst.mem;
    Op op          = inst.op;
    if(op == op_cmp) { 
        printf("cmp ");
    } else if (op == op_sub) {
        printf("sub ");
    } else if (op == op_add) {
        printf("add ");
    } else if (op == op_mov) {
        printf("mov ");
    }
    switch(inst.type) {
        case t_none: return;
        case t_rep:
            printf("rep ");
            return;
        case t_mov_immed_to_reg: 
        {
            printf("mov %s, %hu ; t_immed_to_reg %s\n", registers[reg][w], immed, debug_str);
            return;
        } 
        case t_to_accumulator:
        {
            char arg[2][21] = {{0}};
            snprintf(arg[0], 20, "%s", registers[0b000][w]);
            // can be compressed
            if(op == op_cmp) { 
                snprintf(arg[1], 20, "%d", immed);
            } else if (op == op_sub) {
                snprintf(arg[1], 20, "%d", immed);
            } else if (op == op_add) {
                snprintf(arg[1], 20, "%d", immed);
            } else if (op == op_mov) {
                snprintf(arg[1], 20, "[%d]", immed);
            }
            printf("%s, %s; t_to_accumulator %s %s\n", arg[!d], arg[d], op_str[op], debug_str);
            return;
        }
        case t_reg_to_reg_mem:
        {
            char arg[2][21]; // used for d conditional at the end
            snprintf(arg[0], 20, "%s", registers[reg][w]);
            if (mode == mode_16bit_mem) {
                snprintf(arg[1], 20, "[%d]", mem);
            }
            if (mode == mode_effective_addr) {
                snprintf(arg[1], 20, "[%s]", effective_address[rm]);
            }
            if (mode == mode_effective_addr_8bit_mem) {
                snprintf(arg[1], 20, "[%s + %d]", effective_address[rm], mem);
            }
            if (mode == mode_effective_addr_16bit_mem) {
                snprintf(arg[1], 20, "[%s + %d]", effective_address[rm], mem);
            }
            if (mode == mode_register) {
                snprintf(arg[1], 20, "%s", registers[rm][w]);
            }
            printf("%s, %s; t_reg_to_reg_mem %s %s\n", arg[!d], arg[d], mode_str[mode], debug_str);
            return;
        }
        case t_immed_to_reg_mem:
        {
            //const char * w_str[] = { "byte", "word" };
            if (w == 0) {
                printf("byte ");
            }
            //printf("%s ", w_str[w]);

            if (mode == mode_16bit_mem) {
                assert(0); // never used
                printf("[%d], ", mem);
            }
            if (mode == mode_effective_addr) {
                printf("[%s], ", effective_address[rm]);
            }
            if (mode == mode_effective_addr_8bit_mem) {
                assert(0); // never used
                printf("[%s + %d], ", effective_address[rm], mem);
            }
            if (mode == mode_effective_addr_16bit_mem) {
                printf("[%s + %d], ", effective_address[rm], mem);
            }
            if (mode == mode_register) {
                printf("%s, ", registers[rm][w]);
            }
            if (s) {
                printf("%d; t_immed_to_reg_mem s", immed);
            } else {
                printf("%u; t_immed_to_reg_mem !s", immed);
            }
            printf(" %s\n", debug_str);
            return;
        } // case
        case t_asc_immed_to_reg_mem:
        {
            //const char * w_str[] = { "byte", "word" };
            //printf("%s ", w_str[w]);

            if (w == 0) {
                printf("byte ");
            }
            if (mode == mode_16bit_mem) {
                printf("[%d], ", mem); // change
            }
            if (mode == mode_effective_addr) {
                printf("[%s], ", effective_address[rm]); // change
            }
            if (mode == mode_effective_addr_8bit_mem
                || mode == mode_effective_addr_16bit_mem
            ) {
                printf("[%s + %d], ", effective_address[rm], mem);
            }
            if (mode == mode_register) {
                printf("%s, ", registers[rm][w]); 
            }

            // this is probably a little weird.. I don't really get S & W doing
            // weird stuff together.
            if (s) {
                if (w) {
                    printf("%hd", (s8) immed);
                } else {
                    printf("%hd", immed);
                }
            } else {
                printf("%hu", immed);
            }
            printf(" ; t_asc_immed_to_reg_mem w%d s%d %s %s\n", 
                    w, s, mode_str[mode], debug_str);
            return;
        }
    } // switch
}
void simulate_instruction(Inst inst, char * debug_str) {
    bool w         = inst.w;
    bool d         = inst.d;
    bool s         = inst.s;
    Mode mode       = inst.mode;
    u8 reg         = inst.reg;
    u8 rm         = inst.rm;
    s16 immed      = inst.immed;
    s16 mem         = inst.mem;
    Op op          = inst.op;
    switch(inst.type) {
        case t_none: return;
        case t_rep:
            return;
        case t_mov_immed_to_reg: 
        {
            bool w         = inst.w;
            u8 reg         = inst.reg;
            s16 immed      = inst.immed;
            s16 before = reg_value[reg];
            if (w) {
                reg_value[reg] = immed;
            } else {
                reg_value[reg] = immed & 0x00ff;
            }
            s16 after = reg_value[reg];
            if (before != after) {
                snprintf(debug_str, 30, "%s:0x%hx->0x%hx", registers[reg][w], before, after);
            }
            break; 
        }
        case t_asc_immed_to_reg_mem:
        {
            s16 before = reg_value[rm];
            char old_flags[30] = {0};
            gen_flags_str(old_flags);
            int val;
            if (op == op_add) {
                val = reg_value[rm] + immed;
                reg_value[rm] = val;
            } else if (op == op_sub) {
                val = reg_value[rm] - immed;
                reg_value[rm] = val;
            } else if (op == op_cmp) {
                val = reg_value[rm] - immed;
            }
            set_flags(val);

            s16 after = reg_value[rm];
            char new_flags[30] = {0};
            gen_flags_str(new_flags);
            if (before != after) {
                snprintf(debug_str, 30, "%s:0x%hx->0x%hx", 
                        registers[rm][w], before, after);
            }
            if (old_flags[0] || new_flags[0]) {
                snprintf(debug_str, 30, "%s flags:%s->%s", 
                        debug_str, old_flags, new_flags);
            }
        
            return;
        }
        case t_to_accumulator:
            return;
        case t_immed_to_reg_mem:
        case t_reg_to_reg_mem:
        {
            if (mode == mode_register) {
                s16 before = reg_value[rm];
                char old_flags[30] = {0};
                gen_flags_str(old_flags);
                if (op == op_add) {
                    int val = reg_value[rm] + reg_value[reg];
                    reg_value[rm] = val;
                    set_flags(val);
                } else if (op == op_mov) {
                    reg_value[rm] = reg_value[reg];
                } else if (op == op_sub) {
                    int val = reg_value[rm] - reg_value[reg];
                    reg_value[rm] = val;
                    set_flags(val);
                } else if (op == op_cmp) {
                    int val = reg_value[rm] - reg_value[reg];
                    set_flags(val);
                }
                s16 after = reg_value[rm];
                char new_flags[30] = {0};
                gen_flags_str(new_flags);
                if (before != after) {
                    snprintf(debug_str, 30, "%s:0x%hx->0x%hx", 
                            registers[rm][w], before, after);
                }
                if (old_flags[0] || new_flags[0]) {
                    snprintf(debug_str, 30, "%s flags:%s->%s", 
                            debug_str, old_flags, new_flags);
                }
            }
            return;
        }
    }
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
    if (!f) {
        printf("issue with file, file_pos '%d' argv[file_pos] '%s'\n", argc, argv[file_pos]);
        return 0;
    }
    assert(f);
    fseek (f, 0, SEEK_END);
    long int length = ftell (f); // get current position in stream
    fseek (f, 0, SEEK_SET);
    u8 * mem = (u8 *)malloc (length);
    fread (mem, length, 1, f);
    fclose (f);
    printf("bits 16\n\n");
    for(int mem_idx = 0; mem_idx < length;) {
        Inst inst = {
            t_none
        };
        mem_idx = generate_instruction(mem, mem_idx, &inst);
        char debug_str[200] = {0};
        if (exec) {
            simulate_instruction(inst, debug_str);
        }
        print_instruction(inst, debug_str);
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
