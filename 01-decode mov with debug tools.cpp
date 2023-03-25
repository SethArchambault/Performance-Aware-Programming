#include <stdio.h>
#include <stdlib.h>
#define assert(expr) if(!(expr)) { printf("%s:%d %s() %s\n",__FILE__,__LINE__, __func__, #expr); *(volatile int *)0 = 0; }

int masks[] = {
    0b00000001,
    0b00000011,
    0b00000111,
    0b00001111,
    0b00011111,
    0b00111111,
    0b01111111,
    0b11111111
};

#define opp_to_binary(byte)  \
    (byte & 0x20 ? '1' : '0'), \
    (byte & 0x10 ? '1' : '0'), \
    (byte & 0x08 ? '1' : '0'), \
    (byte & 0x04 ? '1' : '0'), \
    (byte & 0x02 ? '1' : '0'), \
    (byte & 0x01 ? '1' : '0') 

#define three_to_binary(byte)  \
    (byte & 0x04 ? '1' : '0'), \
    (byte & 0x02 ? '1' : '0'), \
    (byte & 0x01 ? '1' : '0') 

#define two_to_binary(byte)  \
    (byte & 0x02 ? '1' : '0'), \
    (byte & 0x01 ? '1' : '0') 

int get_val(char * mem, int start, int end) {
    int shift = 7 - end;
    int mask_len = end - start;
    return ((*mem) >> shift) & masks[mask_len]; 
}
enum {
    mov = 0b100010,
    ax = 0b000,
    bx = 0b011
};

struct Inst {
    int opp;
    int d;
    int w;
    int mod;
    int reg;
    int rm;
};

struct Reg{
    int bit;
    char w[2][3];
};


void print_reg(int w, int reg) {

    Reg reg_arr[] = {
        {0b000, {"al", "ax"}},
        {0b001, {"cl", "cx"}},
        {0b010, {"dl", "dx"}},
        {0b011, {"bl", "bx"}},
        {0b100, {"ah", "sp"}},
        {0b101, {"ch", "bp"}},
        {0b110, {"dh", "si"}},
        {0b111, {"bh", "di"}}
    };
    for(int i = 0; i <sizeof(reg_arr)/ sizeof(Reg) ; ++i) {
        if (reg_arr[i].bit == reg) {
            printf("%s", reg_arr[i].w[w]);
        }
    }
}

Inst get_inst(char * mem) {
    
    Inst inst;
    inst.opp    = get_val(mem, 0, 5);
    inst.d      = get_val(mem, 6, 6);
    inst.w      = get_val(mem,  7, 7);
    inst.mod    = get_val(&mem[1], 0, 1);
    inst.reg    = get_val(&mem[1], 2, 4);
    inst.rm     = get_val(&mem[1], 5, 7);
    return inst;
}

void print_inst(Inst inst) {
    printf("Opp     %c%c%c%c%c%c\n", opp_to_binary(inst.opp));
    printf("D       %d\n", inst.d);
    printf("W       %d\n", inst.w);
    printf("MOD     %c%c\n",    two_to_binary(inst.mod));
    printf("Reg     %c%c%c\n",  three_to_binary(inst.reg));
    printf("RM      %c%c%c\n",  three_to_binary(inst.rm));
}

int main() {
    FILE * f = fopen ("mov", "r");
    assert(f);
    fseek (f, 0, SEEK_END);
    long int length = ftell (f); // get current position in stream
    fseek (f, 0, SEEK_SET);
    char * buf = (char *)malloc (length);
    fread (buf, length, 1, f);
    fclose (f);
    /*
    for(int i=0; i <= 7; i++) {
        printf("%d", get_val(buf,  i, i));
    }
    buf++;
    //printf("%d", buf[0] >> i & 1 );
    printf(" ");
    for(int i=0; i <= 7; i++) {
        printf("%d", get_val(buf, i, i));
    }

    printf("\n");
    printf("Opp***DW M*RegRm*\n");
    printf("         O\n");
    printf("         D\n");
    printf("\n");

    */


    printf("bits 16\n");
    for(int i = 0; i < length; i+=2) {
        Inst inst = get_inst(&buf[i]);
        //print_inst(inst);
        if (inst.opp == mov) {
            printf("mov ");
            if (inst.d == 0) {
                print_reg(inst.w, inst.rm);
                printf(", ");
                // reg is source, so print rm first
                print_reg(inst.w, inst.reg);
            } else if (inst.d == 1) {
                print_reg(inst.w, inst.reg);
                printf(", ");
                // reg is source, so print rm first
                print_reg(inst.w, inst.rm);
            }
        }
        printf("\n");
    }
    return 0;
}



