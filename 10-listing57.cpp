// next up, fix the whole thing about effective address being strings..

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define assert(expr) if(!(expr)) { printf("%s:%d %s() %s\n",__FILE__,__LINE__, __func__, #expr); *(volatile int *)0 = 0; }
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

enum Ea {
    ea_bx_si,
    ea_bx_di,
    ea_bp_si,
    ea_bp_di,
    ea_si,
    ea_di,
    ea_bp,
    ea_bx,
    ea_end
};

Ea bin_to_ea(int bin){
    switch(bin) {
        case 0b000: return ea_bx_si;
        case 0b001: return ea_bx_di;
        case 0b010: return ea_bp_si;
        case 0b011: return ea_bp_di;
        case 0b100: return ea_si;
        case 0b101: return ea_di;
        case 0b110: return ea_bp;
        case 0b111: return ea_bx;
    }
    assert(!"invalid bin to ea");
    return ea_end;
};

char ea_str[][8] = { 
    [ea_bx_si]    = "bx + si",
    [ea_bx_di]    = "bx + di",
    [ea_bp_si]    = "bp + si",
    [ea_bp_di]    = "bp + di",
    [ea_si]       = "si",
    [ea_di]       = "di",
    [ea_bp]       = "bp",
    [ea_bx]       = "bx"
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
    ip,
    reg_count
};

reg ea_to_reg(Ea ea) {
    if (ea == ea_bx) return bx;
    if (ea == ea_bp) return bp;
    if (ea == ea_di) return di;
    if (ea == ea_si) return si;
    printf("rm not found: %s\n", ea_str[ea]);
    assert(0);
    return ax;
}

u8 zero_flag = 0;
u8 sign_flag = 0;
u8 parity_flag = 0;
s16 reg_value[reg_count] = {0};
#define memory_len 64*4 * 65 // one row for the instructions
u8 memory[memory_len];

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
        printf("    %s: 0x%04hx (%hu)\n", registers[Reg][1], reg_value[Reg], reg_value[Reg]);
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
    op_add,
    op_jnz,
    op_je,
    op_jl,
    op_jle,
    op_jb,
    op_jbe,
    op_jp,
    op_jo,
    op_js,
    op_jne,
    op_jnl,
    op_jg,
    op_jnb,
    op_ja,
    op_jnp,
    op_jno,
    op_jns,
    op_loop,
    op_loopz,
    op_loopnz,
    op_jcxz
};

struct TwoByteCode {
    s16 binary;
    Op op;
};

TwoByteCode jump_codes[] = {
    { 0b01110101, op_jnz},
    { 0b01110100, op_je},
    { 0b01111100, op_jl},
    { 0b01111110, op_jle},
    { 0b01110010, op_jb},
    { 0b01110110, op_jbe},
    { 0b01111010, op_jp},
    { 0b01110000, op_jo},
    { 0b01111000, op_js},
    { 0b01110101, op_jne},
    { 0b01111101, op_jnl},
    { 0b01111111, op_jg},
    { 0b01110011, op_jnb},
    { 0b01110111, op_ja},
    { 0b01111011, op_jnp},
    { 0b01110001, op_jno},
    { 0b01111001, op_jns},
    { 0b11100010, op_loop},
    { 0b11100001, op_loopz},
    { 0b11100000, op_loopnz},
    { 0b11100011, op_jcxz},
};

enum Type {
    t_none,
    t_rep,
    t_mov_immed_to_reg,
    t_to_accumulator,
    t_immed_to_reg_mem,
    t_reg_to_mem,
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
    int size;
    Ea ea;
};

/** Mode
 ** mod 00 Memory Mode rm == 110 ? 16-bit disp : no disp    [effective_address] / [memory]
 ** mod 01 Memory Mode 8-bit disp                           [effective_address + disp]
 ** mod 10 Memory Mode 16-bit disp                          [effective_address + disp]
 ** mod 11 Register Mode no disp                            bx, dx
 **/

void generate_instruction( u8 * mem, Inst *inst) {
    u8 * b        = &mem[reg_value[ip]];
    inst->size = 0;
    if (0b1111001 == (b[0] & 0b11111110)>>1) {  // t_rep
        inst->type = t_rep; 
        inst->size = 1; 
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
        inst->size      = 2 + w;
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
        inst->size      = 2 + w;
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
                               // moving memory into a register
                inst->mode = mode_16bit_mem;
                s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
                inst->mem = disp;
                inst->size = 2;
            } else {
               inst->mode = mode_effective_addr;
               inst->ea   = bin_to_ea(rm);
            }
        } else if (mod == 0b01) {

            inst->mode = mode_effective_addr_8bit_mem;
            s8 disp = b[2]; 
            inst->mem = disp;
            inst->size = mod;
            inst->ea   = bin_to_ea(rm);
        } else if (mod == 0b10) {

            inst->mode = mode_effective_addr_16bit_mem;
            s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
            inst->mem = disp;
            inst->size = mod;
            inst->ea   = bin_to_ea(rm);
        } else if (mod == 0b11) {
            inst->mode = mode_register;
        } else {
            assert(0 && "mod fail");
        }

        inst->size += 2;
        inst->type      = t_reg_to_mem;
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
                inst->mode = mode_16bit_mem;
                s16 disp = (b[2] & 0x00ff) | (b[3] << 8) ;
                inst->mem = disp;
                immed = (s8) b[4];
                if (w == 1) {
                    immed = (immed & 0x00ff) | b[5] << 8;
                }
                inst->size += 5 + w; // 2 bytes + 2 disp + 1 data
            } else  { // no disp
                inst->mode = mode_effective_addr;
                inst->ea   = bin_to_ea(rm);
                immed = (s8) b[2];
                if (w == 1) {
                    immed = (immed & 0x00ff) | b[3] << 8;
                }
                inst->size  += 3 + w;
            }
        } else if (mod == 0b01) { 
            //mov word [bx + 4], 10; t_immed_to_reg_mem s  ip:0x1b->0x20
            inst->mode = mode_effective_addr_8bit_mem;
            inst->ea   = bin_to_ea(rm);
            u8 disp = b[2];
            inst->mem = disp;
            immed = b[3] & 0x00ff;
            if (w == 1) {
                immed |= b[4] << 8;
            }
            inst->size  += 4 + w;
        } else if (mod == 0b10) {
            inst->mode = mode_effective_addr_16bit_mem;
            inst->ea   = bin_to_ea(rm);
            s16 disp = (b[3] << 8) | (b[2] & 0x00ff);
            inst->mem = disp;
            immed  = (s8) b[4];
            if (w == 1) {
                immed = (immed & 0x00ff) | b[5] << 8;
            }
            inst->size  += 3 + w + mod;
        } else if (mod == 0b11) {
            inst->mode = mode_register;
            immed  = b[2] & 0x00ff;
            inst->size  += 3;
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
                inst->size  += 5; // 2 bytes + 2 disp + 1 immed
            } else  { // no disp
                inst->mode = mode_effective_addr;
                inst->ea   = bin_to_ea(rm);
                immed = (s8) b[2];
                inst->size  += 3; // 2 bytes + 1 immed
            }
        } else if (mod == 0b01) { // this is never called
          assert(0);
            inst->mode = mode_effective_addr_8bit_mem;
            inst->ea   = bin_to_ea(rm);
            assert(0);
            u8 mem = b[2];
            inst->mem = mem;
            immed = b[3] & 0x00ff;
            if (w == 1) {
                immed |= b[4] << 8;
            }
            inst->size  += 3 + w + mod;
        } else if (mod == 0b10) {
          assert(0);
            inst->mode = mode_effective_addr_16bit_mem;
            inst->ea   = bin_to_ea(rm);
            s16 mem = (b[3] << 8) | (b[2] & 0x00ff);
            inst->mem = mem;
            immed  = (s8) b[4];
            if (w == 1) {
                // for some reason, this has to be commented out?
                //immed = (immed & 0x00ff) | b[5] << 8;
            }
            inst->size  += 4 + 1;
        } else if (mod == 0b11) {
            inst->mode = mode_register;
            // listing 46 requires this:
            immed = b[2] & 0x00ff;
            // listing47 requires this:
            //immed = (s8) b[2];
            //if (w == 1 && s == 1) {
            if (s == 0 && w == 1) {
                immed |= b[3] << 8;
                inst->size  += 1;
            }
            // w is for the register, not necessarily for the immediate
            // Data SW=01
            inst->size  += 3;
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
            s8 ip_inc8 = b[1] & 0x00ff;
            inst->op = code.op;
            inst->immed = (s8)ip_inc8;
            inst->size  += 2;
        } else {
            printf("%c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c\n",
                BYTE_TO_BINARY(b[0]), BYTE_TO_BINARY(b[1]), BYTE_TO_BINARY(b[2]), BYTE_TO_BINARY(b[3]), 
                BYTE_TO_BINARY(b[4]));
            assert(0 && "no opp code found");
        }
    }
}


void print_instruction(Inst inst, char * debug_str) {
    bool w          = inst.w;
    bool d          = inst.d;
    bool s          = inst.s;
    Mode mode       = inst.mode;
    u8 reg          = inst.reg;
    u8 rm           = inst.rm;
    s16 immed       = inst.immed;
    s16 mem         = inst.mem;
    Op op           = inst.op;
    Ea ea           = inst.ea;
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
        case t_none: 
            if (op == op_jnz) {
                printf("jnz $%d ; %s\n", immed + inst.size, debug_str);
                return;
            } 
            return;
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
        case t_reg_to_mem:
        {
            char arg[2][21]; // used for d conditional at the end
            snprintf(arg[0], 20, "%s", registers[reg][w]);
            if (mode == mode_16bit_mem) {
                snprintf(arg[1], 20, "[%d]", mem);
            }
            if (mode == mode_effective_addr) {
                snprintf(arg[1], 20, "[%s]", ea_str[ea]);
            }
            if (mode == mode_effective_addr_8bit_mem) {
                snprintf(arg[1], 20, "[%s + %d]", ea_str[ea], mem);
            }
            if (mode == mode_effective_addr_16bit_mem) {
                snprintf(arg[1], 20, "[%s + %d]", ea_str[ea], mem);
            }
            if (mode == mode_register) {
                snprintf(arg[1], 20, "%s", registers[rm][w]);
            }
            {
                printf("%s, %s; t_reg_to_mem %s %s\n", arg[!d], arg[d], mode_str[mode], debug_str);
            }
            return;
        }
        case t_immed_to_reg_mem:
        {
            if (w == 0) {
                printf("byte ");
            } else {
                printf("word ");
            }

            if (mode == mode_16bit_mem) {
                printf("[%d], ", mem);
            }
            if (mode == mode_effective_addr) {
                printf("[%s], ", ea_str[ea]);
            }
            if (mode == mode_effective_addr_8bit_mem) {
                printf("[%s + %d], ", ea_str[ea], mem);
            }
            if (mode == mode_effective_addr_16bit_mem) {
                printf("[%s + %d], ", ea_str[ea], mem);
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
                printf("[%s], ", ea_str[ea]); // change
            }
            if (mode == mode_effective_addr_8bit_mem
                || mode == mode_effective_addr_16bit_mem
            ) {
                printf("[%s + %d], ", ea_str[ea], mem);
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

int clock_total = 0;
void clocks(char *str, int clocks, int has_displacement) {
    if (has_displacement) {
        int ea_clock = 6; // disp only
        int clocks_w_ea_int = clocks + ea_clock;
        clock_total += clocks_w_ea_int;
        snprintf(str, 100, "| Clocks: +%d = %d (%d + %dea) |", clocks_w_ea_int, clock_total, clocks, ea_clock);
    } else  {
        clock_total += clocks;
        snprintf(str, 100, "| Clocks: +%d = %d |", clocks, clock_total);
    }
}

void clocks_w_ea(char *str, int clocks, Ea ea, int has_displacement) {
    int ea_clock = 0;
    // no displacement only right now.
    if (ea == ea_bx)    ea_clock = 5;
    else if (ea == ea_bp)    ea_clock = 5;
    else if (ea == ea_si)    ea_clock = 5;
    else if (ea == ea_di)    ea_clock = 5;
    else if (ea == ea_bp_di) ea_clock = 7;
    else if (ea == ea_bx_si) ea_clock = 7;
    else if (ea == ea_bp_si) ea_clock = 8;
    else if (ea == ea_bx_di) ea_clock = 8;
    else {
        printf("clocks_w_ea missing ea: %s\n", ea_str[ea]);
        assert(0);
    }
    if (has_displacement) {
        ea_clock += 4;
    }
    int clocks_w_ea_int = clocks + ea_clock;
    clock_total += clocks_w_ea_int;
    snprintf(str, 100, "| Clocks: +%d = %d (%d + %dea) |", clocks_w_ea_int, clock_total, clocks, ea_clock);
}

void simulate_instruction(Inst inst, char * debug_str) {
    bool w          = inst.w;
    bool d          = inst.d;
    bool s          = inst.s;
    Mode mode       = inst.mode;
    u8 reg          = inst.reg;
    u8 rm           = inst.rm;
    s16 immed       = inst.immed;
    s16 mem         = inst.mem;
    Op op           = inst.op;
    Ea ea           = inst.ea;

    int ip_before = reg_value[ip];
    reg_value[ip] += inst.size;
    switch(inst.type) {
        case t_none: 
            if (op == op_jnz) {
                if (!zero_flag)  {
                    reg_value[ip] += immed;
                }
                break;
            } 
            break;
        case t_rep:
            break;
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
                char clock_str[100];
                clocks(clock_str, 4, false);
                snprintf(debug_str, 100, "%s %s %s:0x%hx->0x%hx", 
                    debug_str, clock_str, registers[reg][w], before, after);
            }
            break; 
        }
        case t_asc_immed_to_reg_mem:
        {
            s16 before = reg_value[rm];
            char old_flags[30] = {0};
            gen_flags_str(old_flags);
            int val;
            // rm could be memory so this will need a conditional
            if (op == op_add) {
                if (mode == mode_effective_addr) {
                    if (ea == ea_bx_si) {assert(0);
                    }else if (ea == ea_bx_di) {assert(0);
                    }else if (ea == ea_bp_di) { assert(0);
                    }else if (ea == ea_bp_si) {
                        if (!d) {
                            val = immed;
                            memory[reg_value[bp] + reg_value[si]] = val;
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 17, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        }
                    } else {
                        assert(0);
                    }
                } else {
                    {
                        char clock_str[100];
                        clocks(clock_str, 4, false);
                        snprintf(debug_str, 100, "%s", clock_str);
                    }
                    val = reg_value[rm] + immed;
                    reg_value[rm] = val;
                }
            } else if (op == op_sub) {
                val = reg_value[rm] - immed;
                reg_value[rm] = val;
            } else if (op == op_cmp) {
                val = reg_value[rm] - immed;
            } else {
                assert(0);
            }
            set_flags(val);

            s16 after = reg_value[rm];
            char new_flags[30] = {0};
            gen_flags_str(new_flags);
            if (before != after) {
                snprintf(debug_str, 100, "%s %s:0x%hx->0x%hx", 
                        debug_str, registers[rm][w], before, after);
            }
            if (old_flags[0] || new_flags[0]) {
                snprintf(debug_str, 100, "%s flags:%s->%s", 
                         debug_str, old_flags, new_flags);
            }

        
            break;
        }
        case t_to_accumulator:
            break;
        case t_immed_to_reg_mem:
        {
            if (mode == mode_effective_addr_8bit_mem) {
                // works!
                if (ea == ea_bx_si) assert(0);
                if (ea == ea_bx_di) assert(0);
                if (ea == ea_bp_si) assert(0);
                if (ea == ea_bp_di) assert(0);
                u8 effective_reg = ea_to_reg(ea);
                memory[reg_value[effective_reg] + mem] = immed & 0x00ff;
            } else if (mode == mode_16bit_mem) {
                memory[mem] = immed;
            } else if (mode == mode_effective_addr) {
                if (ea == ea_bx) {
                    memory[reg_value[bx]] = immed;
                }
            } else {
                printf("mode %d\n", mode);
                assert(0);
            }

            break;
        }
        case t_reg_to_mem:
        {
            if(mode == mode_effective_addr_8bit_mem) {
                if (ea == ea_bx_si) assert(0);
                if (ea == ea_bx_di) assert(0);
                if (ea == ea_bp_si) assert(0);
                if (ea == ea_bp_di) assert(0);
                u8 effective_reg = ea_to_reg(ea);
                memory[reg_value[effective_reg] + mem] = reg_value[reg];
                {
                    char clock_str[100];
                    clocks_w_ea(clock_str, 8, ea, mem > 0);
                    snprintf(debug_str, 100, "%s", clock_str);
                }
            } else if(mode == mode_effective_addr_16bit_mem) {
                // this needs to work for different modes
                if (op == op_add) {
                    if (ea == ea_bx_si) assert(0);
                    if (ea == ea_bx_di) assert(0);
                    if (ea == ea_bp_di) assert(0);
                    if (ea == ea_bp_si) {
                        if (d) {
                            reg_value[reg] += memory[reg_value[bp] + reg_value[si] + mem];
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 9, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        } else  {
                            memory[reg_value[bp] + reg_value[si] + mem] += reg_value[reg];
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 16, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        }
                    } else {

                        u16 effective_reg = ea_to_reg(ea);
                        if (d) {
                            reg_value[reg] += memory[reg_value[effective_reg] + mem];
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 9, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        } else  {
                            memory[reg_value[effective_reg] + mem] += reg_value[reg];
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 16, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        }
                    }

                } else if (op == op_sub) {
                    assert(0);
                } else if (op == op_cmp) {
                    assert(0);
                } else if (op == op_mov) {
                    if (ea == ea_bp_di) {
                        if (d) {
                            reg_value[reg] = memory[reg_value[bp] + reg_value[di]];
                            {
                                //fix
                                char clock_str[100];
                                clocks_w_ea(clock_str, 8, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        } else  {
                            memory[reg_value[bp] + reg_value[di]] = reg_value[reg];
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 9, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        }
                    } else if (ea == ea_bx_si) {
                        if (d) {
                            reg_value[reg] = memory[reg_value[bx] + reg_value[si]];
                            {
                                //fix
                                char clock_str[100];
                                clocks_w_ea(clock_str, 8, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        } else  {
                            memory[reg_value[bx] + reg_value[si]] = reg_value[reg];
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 9, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        }
                    } else if (ea == ea_bx_di) {
                        if (d) {
                            reg_value[reg] = memory[reg_value[bx] + reg_value[di]];
                            {
                                //fix
                                char clock_str[100];
                                clocks_w_ea(clock_str, 8, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        } else  {
                            memory[reg_value[bx] + reg_value[di]] = reg_value[reg];
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 9, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        }
                    } else if (ea == ea_bp_si) {
                        if (d) {
                            reg_value[reg] = memory[reg_value[bp] + reg_value[si]];
                            {
                                //fix
                                char clock_str[100];
                                clocks_w_ea(clock_str, 8, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        } else  {
                            memory[reg_value[bp] + reg_value[si]] = reg_value[reg];
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 9, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        }
                    } else { 
                        u16 effective_reg = ea_to_reg(ea);
                        if (d) {
                            reg_value[reg] = memory[reg_value[effective_reg] + mem];
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 8, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        } else  {
                            memory[reg_value[effective_reg] + mem] = reg_value[reg];
                            {
                                char clock_str[100];
                                clocks_w_ea(clock_str, 9, ea, mem > 0);
                                snprintf(debug_str, 100, "%s", clock_str);
                            }
                        }
                    }
                } else {
                    assert(0);
                }
            } else if (mode == mode_effective_addr) {
                if (ea == ea_bx) {
                    if (!d) {
                        memory[reg_value[bx]] = reg_value[reg] & 0x00ff;
                    } else {
                        reg_value[reg] = memory[reg_value[bx]];
                    }
                } else if (ea == ea_si) {
                    if (!d) {
                        memory[reg_value[si]] = reg_value[reg] & 0x00ff;
                    } else {
                        reg_value[reg] = memory[reg_value[si]];
                    }
                } else if (ea == ea_di) {
                    if (!d) {
                        memory[reg_value[di]] = reg_value[reg] & 0x00ff;
                    } else {
                        reg_value[reg] = memory[reg_value[di]];
                    }
                } else if (ea == ea_bp_di) {
                    if (!d) {
                        memory[reg_value[bp]+reg_value[di]] = reg_value[reg] & 0x00ff;
                    } else {
                        reg_value[reg] = memory[reg_value[bp]+reg_value[di]];
                    }
                } else if (ea == ea_bp_si) {
                    if (!d) {
                        memory[reg_value[bp] + reg_value[si]] = reg_value[reg] & 0x00ff;
                    } else {
                        reg_value[reg] = memory[reg_value[bp] + reg_value[si]];
                    }
                } else if (ea == ea_bx_si) {
                    if (!d) {
                        memory[reg_value[bx]+reg_value[si]] = reg_value[reg] & 0x00ff;
                    } else {
                        reg_value[reg] = memory[reg_value[bx]+reg_value[si]];
                    }
                } else if (ea == ea_bx_di) {
                    if (!d) {
                        memory[reg_value[bx]+reg_value[di]] = reg_value[reg] & 0x00ff;
                    } else {
                        reg_value[reg] = memory[reg_value[bx]+reg_value[di]];
                    }
                } else {
                    printf("\nea to str %s\n", ea_str[ea]);
                    assert(0);
                }
                {
                    char clock_str[100];
                    // 1 cycle more to write to memory
                    clocks_w_ea(clock_str, d ? 8 : 9, ea, false);
                    snprintf(debug_str, 100, "%s", clock_str);
                }
            } else if (mode == mode_16bit_mem) {
                char clock_str[100];
                clocks(clock_str, 8, true);
                snprintf(debug_str, 100, "%s", clock_str);
                reg_value[reg] = memory[mem];
            } else if (mode == mode_register) {
                s16 before = reg_value[rm];
                char old_flags[30] = {0};
                gen_flags_str(old_flags);
                if (op == op_add) {
                    int val = reg_value[rm] + reg_value[reg];
                    reg_value[rm] = val;
                    set_flags(val);
                    {
                        char clock_str[100];
                        clocks(clock_str, 3, false);
                        snprintf(debug_str, 100, "%s", clock_str);
                    }
                } else if (op == op_mov) {
                    reg_value[rm] = reg_value[reg];
                    {
                        char clock_str[100];
                        clocks(clock_str, 2, false);
                        snprintf(debug_str, 100, "%s", clock_str);
                    }
                } else if (op == op_sub) {
                    int val = reg_value[rm] - reg_value[reg];
                    reg_value[rm] = val;
                    set_flags(val);
                    {
                        char clock_str[100];
                        clocks(clock_str, 3, false);
                        snprintf(debug_str, 100, "%s", clock_str);
                    }
                } else if (op == op_cmp) {
                    int val = reg_value[rm] - reg_value[reg];
                    set_flags(val);
                    {
                        char clock_str[100];
                        clocks(clock_str, 3, false);
                        snprintf(debug_str, 100, "%s", clock_str);
                    }
                } else {
                    assert(0);
                }
                s16 after = reg_value[rm];
                char new_flags[30] = {0};
                gen_flags_str(new_flags);
                if (before != after) {
                    snprintf(debug_str, 100, "%s %s:0x%hx->0x%hx", 
                            debug_str, registers[rm][w], before, after);
                }
                if (old_flags[0] || new_flags[0]) {
                    snprintf(debug_str, 100, "%s flags:%s->%s", 
                            debug_str, old_flags, new_flags);
                }
            } else {
                printf("mode %d\n", mode);
                assert(0);
            }
            break;
        }
        default:
         assert(0 && "implement simulation for op");

    }// switch
    snprintf(debug_str , 100, "%s ip:0x%x->0x%x", debug_str, ip_before, reg_value[ip]);
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
    fread (memory, length, 1, f);
    fclose (f);
    printf("bits 16\n\n");
    for(; reg_value[ip] < length;) {
        Inst inst = {
            t_none
        };
        generate_instruction(memory, &inst);
        char debug_str[200] = {0};
        if (exec) {
            simulate_instruction(inst, debug_str);
        } else {
            // if we're not executing we still want to increment
            reg_value[ip] += inst.size;
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
        printf("    ip: 0x%04hx (%hu)\n", reg_value[ip], reg_value[ip]);

        char final_flags[30] = {0};
        gen_flags_str(final_flags);
        printf("flags:%s\n", final_flags);

        FILE * f = fopen ("memory.dat", "w");
        if (f != NULL)
        {
            fwrite(memory, memory_len, 1, f);
            fclose (f);
        }
    }
    return 0;
}
