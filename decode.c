#include <stdint.h>
#include "cpu.h"

#define PREFIX 0xCB
#define GET_FIRST_OCTAL_DIGIT(byte) (byte & 0xB0 >> 6)
#define GET_SECOND_OCTAL_DIGIT(byte) (byte & 0x38 >> 3)
#define GET_THIRD_OCTAL_DIGIT(byte) (byte & 0x07)
#define GET_BIT_THREE(byte) ((byte & 0x08) >> 3)
#define GET_BITS_FOUR_FIVE(byte) ((byte & 0x30 >> 4))

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
static void push_call_nop(uint8_t opcode);
static void cb_prefixed_ops(uint8_t opcode);

/*
 * DISASSEMBLY TABLES
 * These tables aid in decoding instructions as outlined in "DECODING Gameboy Z80 OPCODES" by Scott Mansell
 * These tables DO NOT directly access any registers
 */
static uint8_t REGISTERS_DT[] = {B, C, D, E, H, L, HL, A};
static uint8_t REGISTER_PAIRS_DT[] = {BC, DE, HL, SP};
static uint8_t REGISTER_PAIRS2_DT[] = {BC, DE, HL, AF};

typedef void (*opcode_func)(uint8_t);
static opcode_func decode_lookup[5][8] = {
        {relative_jumps, load_immediate_add_16bit, indirect_loading, inc_dec_16bit, inc,  dec,           ld_8bit, ops_on_accumulator},
        {ld_or_halt, nop, nop, nop, nop,                                                  nop,           nop,     nop},
        {alu, nop, nop, nop, nop,                                                         nop,           nop,     nop},
        {mem_mapped_ops, pop_various, conditional_jumps, assorted_ops, conditional_calls, push_call_nop, alu,     rst},
        {cb_prefixed_ops, nop, nop, nop, nop,                                             nop,           nop,     nop}
};

/*
 * Gets indexes for decode lookup table using OPCODE and jumps to that function
 * Algorithm described in "DECODING Gameboy Z80 OPCODES" by Scott Mansell
*/
void decode(uint8_t opcode) {
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
    decode_lookup[first_index][second_index](opcode);
}

/*
 * All below functions take in OPCODE and choose which instruction to
 * execute based off the algorithm described in "DECODING Gameboy Z80 OPCODES" by Scott Mansell
*/

static void relative_jumps(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        case 0:
            nop(opcode);
            break;
        case 1:
            stop();
            break;
        case 2:
            ld(fetch_word(), SP, POINTER, REG_16BIT);
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
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    //TODO figure out which add this is
    bit_three ? add(HL, REGISTER_PAIRS_DT[bits_four_five]) : ld(REGISTER_PAIRS_DT[bits_four_five], fetch_word(), REG_16BIT, CONST_16BIT);
}

//TODO adjust parameters once ld is written
static void indirect_loading(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    switch (bits_four_five) {
        case 0:
            bit_three ? ld(A, BC, REG_8BIT, REG_16BIT) : ld(BC, A, REG_16BIT, REG_8BIT);
            break;
        case 1:
            bit_three ? ld(A, DE, REG_16BIT, REG_16BIT) : ld(DE, A, REG_16BIT, REG_8BIT);
            break;
        /* TODO COME BACK TO DECIDE HOW TO IMPLEMENT INCREMENT/DECREMENT FOR CASE 2 and 3
         * IF IN LD WE NEED TO KNOW WHICH OPERAND TO BE INCREMENTED AND IF ITS INCREMENTED OR DECREMENTED
         * IF WE JUST CALL INC, WE NEED TO MAKE SURE THE CYCLES ARE CORRECT
        */
        case 2:
            bit_three ? ld(A, HL, REG_8BIT, REG_16BIT) : ld(HL, A, REG_16BIT, REG_8BIT);
            inc(HL);
            break;
        case 3:
            bit_three ? ld(A, HL, REG_8BIT, REG_16BIT) : ld(HL, A, REG_16BIT, REG_8BIT);
            dec(HL);
            break;
    }
}

static void inc_dec_16bit(uint8_t opcode) {
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    bits_four_five ? inc(opcode) : dec(opcode);
}

static void ld_8bit(uint8_t opcode) {
    uint8_t reg_dt_index = GET_SECOND_OCTAL_DIGIT(opcode);
    uint8_t dest_type = reg_dt_index == 6 ? REG_POINTER : REG_8BIT;
    ld(REGISTERS_DT[reg_dt_index], fetch_byte(), dest_type, CONST_8BIT);
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
    uint8_t dest_reg = GET_SECOND_OCTAL_DIGIT(opcode);
    uint8_t source_reg = GET_THIRD_OCTAL_DIGIT(opcode);
    dest_reg == 6 ? halt() : ld(REGISTERS_DT[dest_reg], REGISTERS_DT[source_reg], REG_8BIT, REG_8BIT);
}

static void alu(uint8_t opcode) {
    uint8_t first_octal_dig = GET_FIRST_OCTAL_DIGIT(opcode);
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    uint8_t third_octal_dig = GET_THIRD_OCTAL_DIGIT(opcode);
    //operand is either 8 bit registers, 8 bit imm, or [HL] depending on if first_octal_dig is 2 or 3
    uint8_t operand;
    uint8_t operand_type;
    if (first_octal_dig == 3) {
        operand = fetch_byte();
        operand_type = CONST_8BIT;
    }
    //first_octal_dig is 2
    else {
        operand = REGISTERS_DT[third_octal_dig];
        operand_type = third_octal_dig == 6 ? REG_POINTER : REG_8BIT;
    }

    switch (second_octal_dig) {
        case 0:
            add(operand, operand_type);
            break;
        case 1:
            adc(operand, operand_type);
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
            ld(fetch_byte(), A, OFFSET, REG_8BIT);
            break;
        case 5:
            add(fetch_byte(), OFFSET);
            break;
        case 6:
            ld(A, fetch_byte(), REG_8BIT, OFFSET);
            break;
        case 7:
            //TODO set H and C flags
            ld(HL, SP + (int8_t)fetch_byte(), REG_8BIT, CONST_16BIT);
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
            ld(SP, HL, REG_16BIT, REG_16BIT);
    }
}

static void conditional_jumps(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        default:
            jp();
            break;
        case 4:
            ld(C, A, REG_OFFSET, REG_8BIT);
            break;
        case 5:
            ld(fetch_word(), A, POINTER, REG_8BIT);
            break;
        case 6:
            ld(A, C, REG_8BIT, REG_OFFSET);
            break;
        case 7:
            ld(A, fetch_word(), REG_8BIT, POINTER);
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
            nop(opcode);
    }
}

static void conditional_calls(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    //instructions whose opcode's second octal digit are 4-7 are usually implemented in the Z80 but not on the gbz80
    second_octal_dig < 3 ? call() : nop(opcode);
}

static void push_call_nop(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    if (!bit_three) {
        push();
    }
    bits_four_five == 0 ? call() : nop(opcode);
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
