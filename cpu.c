#include <stdint.h>
#include "gb.h"
#include "cpu.h"
#define ZERO_FLAG(f) (f & 0x80)
#define SUBTRACTION_FLAG(f) (f & 0x40)
#define HALF_CARRY_FLAG(f) (f & 0x20)
#define CARRY_FLAG(f) (f & 0x10)
#define PREFIX 0xCB
#define GET_FIRST_OCTAL_DIGIT(byte) (byte & 0xB0 >> 6)
#define GET_SECOND_OCTAL_DIGIT(byte) (byte & 0x38 >> 3)
#define GET_THIRD_OCTAL_DIGIT(byte) (byte & 0x07);
#define GET_BIT_THREE(byte) ((byte & 0x08) >> 3)
#define GET_BITS_FOUR_FIVE(byte) ((byte & 0x30 >> 4))

typedef void (*opcode_func)(uint8_t);
static void relative_jumps(uint8_t opcode);
static void load_immediate_add_16bit(uint8_t opcode);
static void indirect_loading(uint8_t opcode);
static void inc_dec_16bit(uint8_t opcode);
static void ld_8bit(uint8_t opcode);
static void ops_on_accumulator(uint8_t opcode);
static void ld_or_halt(uint8_t opcode);
static void alu(uint8_t opcode);
static void mem_mapped_ops(uint8_t opcode);
static void pop_various(uint8_t opcode);
static void conditional_jumps(uint8_t opcode);
static void assorted_ops(uint8_t opcode);
static void conditional_calls(uint8_t opcode);
static void push_or_nop(uint8_t opcode);
static void cb_prefixed_ops(uint8_t opcode);



void cpu_init() {
    a = b = c = d = f = h = l = 0;
    opcode_func* opcode_func_arr = {
            {relative_jumps, load_immediate_add_16bit, indirect_loading, inc_dec_16bit, inc,  dec, ld_8bit, ops_on_accumulator},
            {ld_or_halt},
            {alu},
            {mem_mapped_ops, pop_various, conditional_jumps, assorted_ops, conditional_calls, push_or_nop, alu, rst},
            {cb_prefixed_ops}
    };
}

void decode(unsigned char opcode) {
    unsigned char first_nibble = opcode & 0xF0;
    bool CB_prefix = first_nibble == PREFIX;
    uint8_t first_octal_dig = GET_FIRST_OCTAL_DIGIT(opcode);
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    uint8_t third_octal_dig = GET_THIRD_OCTAL_DIGIT(opcode);
    uint8_t first_index = CB_prefix ? 4 : first_octal_dig;
    uint8_t second_index;
    switch (first_index) {
        case 0:
            second_index = third_octal_dig;
            break;
        case 1:
            second_index = third_octal_dig == 6;
            break;
        case 2:
            second_index = second_octal_dig;
            break;
        case 3:
            second_index = third_octal_dig;
            break;
        case 4:
            second_index = first_octal_dig;
            break;
    }
}

//TODO shift bits
static void relative_jumps(uint8_t opcode) {
    uint8_t second_octal_dig = opcode && 0x38;
    switch (second_octal_dig) {
        case 0:
            nop();
            break;
        case 1:
            stop();
            break;
        case 2:
            ld();
            break;
        case 3:
            jr();
            break;
        defualt:
            jr();
            break;
    }
}

static void load_immediate_add_16bit(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    bit_three ? add() : ld();
}

//TODO adjust parameters once ld is written
static void indirect_loading(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    switch (bits_four_five) {
        case 0:
            bit_three ? ld() : ld();
            break;
        case 1:
            ld();
            break;
        case 2:
            ld();
            break;
        case 3:
            bit_three ? ld(): ld();
            break;
        //invalid:
        default:
            nop();
            break;
    }
}

static void inc_dec_16bit(uint8_t opcode) {
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    bits_four_five ? inc() : dec();
}

static void ld_8bit(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    ld();
}

static void ops_on_accumulator(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        case 0:
            rlca();
            break;
        case 1:
            rrca();
            break;
        case 2:
            rla();
            break;
        case 3:
            rra();
            break;
        case 4:
            daa();
            break;
        case 5:
            cpl();
            break;
        case 6:
            scf();
            break;
        case 7:
            ccf();
            break;
    }
}

static void ld_or_halt(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    second_octal_dig == 6 ? halt() : ld();
}

static void alu(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        case 0:
            add();
            break;
        case 1:
            adc();
            break;
        case 2:
            sub();
            break;
        case 3:
            sbc();
            break;
        case 4:
            and();
            break;
        case 5:
            xor();
            break;
        case 6:
            or();
            break;
        case 7:
            cp();
            break;
    }
}

static void mem_mapped_ops(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        default:
            ret();
            break;
        case 4:
            ld();
            break;
        case 5:
            add();
            break;
        case 6:
            ld();
            break;
        case 7:
            ld();
            break;
    }
}

static void pop_various(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    if (!bit_three) {
        pop();
    }
    switch (bits_four_five) {
        case 0:
            ret();
            break;
        case 1:
            reti();
            break;
        case 2:
            jp();
        case 3:
            ld();
    }
}

static void conditional_jumps(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        default:
            jp();
            break;
        case 4:
            ld();
            break;
        case 5:
            ld();
            break;
        case 6:
            ld();
            break;
        case 7:
            ld();
            break;
    }
}

static void assorted_ops(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        case 0:
            jp();
            break;
        case 6:
            di();
            break;
        case 7:
            ei();
            break;
        //instructions whose opcode's second octal digit are 2-5 are usually implemented in the Z80 but not on the gbz80
        default:
            nop();
    }
}

static void conditional_calls(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    //instructions whose opcode's second octal digit are 4-7 are usually implemented in the Z80 but not on the gbz80
    second_octal_dig < 3 ? call() : nop();
}

static void push_or_nop(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    if (!bit_three) {
        push();
    }
    bits_four_five == 0 ? call() : nop();
}

static void cb_prefixed_ops(uint8_t opcode) {
    uint8_t first_octal_dig = GET_FIRST_OCTAL_DIGIT(opcode);
    switch (first_octal_dig) {
        case 0:
            rot();
            break;
        case 1:
            bit();
            break;
        case 2:
            res();
            break;
        case 3:
            set();
            break;
    }
}
