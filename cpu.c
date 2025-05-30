#include <stdint.h>
#include "gb.h"
#include "cpu.h"
#include "decode.h"
#define ZERO_FLAG(f) (f & 0x80)
#define SUBTRACTION_FLAG(f) ((f & 0x40) >> 6)
#define HALF_CARRY_FLAG(f) ((f & 0x20) >> 5)
#define CARRY_FLAG(f) ((f & 0x10) >> 4)
#define ZERO_BIT 0x80
#define NEGATIVE_BIT 0x40
#define HALF_CARRY_BIT 0x20
#define CARRY_BIT 0x10
#define SET_BIT(bit, value) (bit | value)
#define CLEAR_BIT(bit, value) (~bit | value)
#define BYTE 1
#define WORD 2
#define FIRST_NIBBLE(x) (x & 0x000F)
/* REGISTERS */
static uint16_t REGS[6];



void cpu_init() {
    REGS[AF] = 0x0100;
    REGS[BC] = 0xFF13;
    REGS[DE] = 0x00C1;
    REGS[HL] = 0x8403;
    REGS[SP] = 0xFFEE;
    REGS[PC] = 0x0100;
    decode_init();
}


static void write_8bit_reg(uint8_t reg, uint8_t val) {
    uint8_t index;
    if (reg % 2 == 0) {
        index = reg / 2;
        REGS[index] = REGS[index] | (val << 4);
    }
    else {
        index = (reg - 1) / 2;
        REGS[index] = REGS[index] | val;
    }
}

uint8_t read_8bit_reg(uint8_t reg) {
    if (reg % 2 == 0) {
        return (REGS[reg / 2] && 0xFF00) >> 4;
    }
    else {
        return REGS[(reg - 1) / 2] && 0x00FF;
    }
}

//TODO Change function name
void cpu() {
    uint8_t next_byte = fetch_byte();
    decode(next_byte);
}

/*
 * Returns byte pointed to by PC
*/
uint8_t fetch_byte() {
    uint8_t byte = MEMORY[PC];
    REGS[PC] += 0x0001;
    return byte;
}

/*
 * Returns word pointed to by PC
*/
uint16_t fetch_word() {
    uint16_t word = (MEMORY[REGS[PC]] << 4) + MEMORY[REGS[PC]];
    REGS[PC] += 0x0002;
    return word;
}

/*
 * Sets zero flag if RESULT is zero, otherwise clears zero flag
*/
static void set_zero_flag(uint32_t result) {
    if (result == 0) {
        write_8bit_reg(F, SET_BIT(ZERO_BIT, REGS[A]));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(ZERO_BIT, read_8bit_reg(F)));
    }
}

/*
 * Uses the RESULT of an addition operation and the LENGTH of the operands to set flags
*/
static void set_addition_flags(uint32_t result, bool length) {
    if ((length == BYTE && result > 0x000F) || (length == WORD && result > 0x00FF)) {
        write_8bit_reg(F, SET_BIT(HALF_CARRY_BIT, REGS[A]));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    }
    if ((length == BYTE && result > 0x00FF) || (length == WORD && result > 0xFFFF)) {
        write_8bit_reg(F, SET_BIT(CARRY_BIT, REGS[A]));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    }
    set_zero_flag(result);
}

/*
 * Uses the RESULT of a subtraction operation of
 */
static void set_subtraction_flags(uint16_t result, uint8_t source_val) {
    if (FIRST_NIBBLE(source_val) > (FIRST_NIBBLE(REGS[A]))) {
        //THIS WILL OVERWRITE THE REGISTER, NEED TO OR IT
        write_8bit_reg(F, SET_BIT(HALF_CARRY_BIT, REGS[A]));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    }
    if (source_val > REGS[A]) {
        write_8bit_reg(F, SET_BIT(CARRY_BIT, REGS[A]));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(CARRY_BIT, read_8bit_reg(F)));
    }
    write_8bit_reg(F, SET_BIT(NEGATIVE_BIT, REGS[A]));
    set_zero_flag((uint32_t) result);
}

/*
 * Loads SOURCE into DEST
 * Operand types are describes by DEST_TYPE and SOURCE_TYPE
*/
void ld(uint16_t dest, uint16_t source, uint8_t dest_type, uint8_t source_type) {
    uint8_t source_val;
    switch (source_type) {
        case REG_8BIT:
            source_val = read_8bit_reg(source_val);
            break;
        case REG_16BIT:
            source_val = REGS[source];
            break;
        case POINTER:
            source_val = MEMORY[source];
            break;
        case REG_POINTER:
            source_val = MEMORY[REGS[source]];
            break;
        case OFFSET:
            source_val = MEMORY[0xFF00 + source];
            break;
        case REG_OFFSET:
            source_val = MEMORY[0xFF00 + REGS[source]];
            break;
        //source is 16 or 8 bit constant
        default:
            source_val = source;
            break;
    }
    switch (dest_type) {
        case REG_8BIT:
            write_8bit_reg(dest, source_val);
        case REG_16BIT:
            REGS[dest] = source_val;
            break;
        case POINTER:
            MEMORY[dest] = source_val;
            break;
        case REG_POINTER:
            MEMORY[REGS[dest]] = source_val;
            break;
        case OFFSET:
            MEMORY[0xFF00 + dest] = source_val;
            break;
        case REG_OFFSET:
            MEMORY[0xFF00 + REGS[dest]] = source_val;
            break;
        //const
        default:
        //TODO ERROR HANDLING
    }
}

/*
 * Add Instruction
 * Takes in either 8bit/16bit reg, 8bit/16bit const, register pointer (HL), or signed offset
 * Depending on operand, result will be stored in either accumulator, HL, or SP
 * Zero, carry, and half-carry flags are set
 */
void add(uint16_t operand, uint8_t operand_type) {
    uint32_t result;
    switch (operand_type) {
        case REG_8BIT:
            result = read_8bit_reg(A) + read_8bit_reg(operand);
            write_8bit_reg(A, (uint8_t) result);
            set_addition_flags(result, BYTE);
            break;
        case REG_16BIT:
            result = REGS[HL] + REGS[operand];
            REGS[HL] = (uint16_t) result;
            set_addition_flags(result, WORD);
            break;
        case CONST_8BIT:
            result = read_8bit_reg(A) + operand;
            write_8bit_reg(A, (uint8_t) result);
            set_addition_flags(result, BYTE);
            break;
        case CONST_16BIT:
            result = REGS[HL] + operand;
            REGS[HL] = (uint16_t) result;
            set_addition_flags(result, WORD);
            break;
        case REG_POINTER:
            result = read_8bit_reg(A) + MEMORY[REGS[HL]];
            write_8bit_reg(A, (uint8_t) result);
            set_addition_flags(result, BYTE);
            break;
        case OFFSET:
            result = REGS[SP] + (int8_t) operand;
            REGS[SP] = result;
            set_addition_flags(result, BYTE);
            break;
    }
}

/*
 * Add-Carry Instruction - adds operand, carry bit, and accumulator
 * Result stored in Accumulator Register
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 * Zero, carry, and half-carry flags are set
 */
void adc(uint8_t operand, uint8_t operand_type) {
    uint16_t result;
    uint8_t carry_bit = HALF_CARRY_FLAG(read_8bit_reg(F));
    switch (operand_type) {
        case REG_8BIT:
            result = REGS[A] + REGS[operand] + carry_bit;
            break;
        case CONST_8BIT:
            result = REGS[A] + operand + carry_bit;
            break;
        case REG_POINTER:
            result = REGS[A] + MEMORY[REGS[HL]] + carry_bit;
            break;
    }
    REGS[A] = result;
    set_addition_flags((uint32_t) result, BYTE);
}

//TODO CONSIDER MAKING THIS ONE FUNCTION
/*
 * Compare Instruction - Compares the value in A with the value in operand
 * Results are stored in Flag Register - zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void cp(uint8_t operand, uint8_t operand_type) {
    uint16_t result;
    uint8_t source_val;
    switch (operand_type) {
        case REG_8BIT:
            source_val = read_8bit_reg(operand);
            break;
        case CONST_8BIT:
            source_val = operand;
            break;
        case REG_POINTER:
            source_val = MEMORY[REGS[HL]];
            break;
    }
    result = REGS[A] - operand;
    set_subtraction_flags(result, source_val);
}

//TODO make subtract and sbc two different instructions?
/*
 * Subtract Instruction - Subtracts the value in A with the value in operand
 * Results are stored in Accumulator Register
 * Zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void sub(uint8_t operand, uint8_t operand_type) {
    uint16_t result;
    uint8_t source_val;
    switch (operand_type) {
        case REG_8BIT:
            source_val = read_8bit_reg(operand);
            break;
        case CONST_8BIT:
            source_val = operand;
            break;
        case REG_POINTER:
            source_val = MEMORY[REGS[HL]];
            break;
    }
    result = REGS[A] - operand;
    REGS[A] = result;
    set_subtraction_flags(result, source_val);
}

/*
 * Subtract Carry Instruction - Subtracts the value in A with the value in operand and the carry bit
 * Results are stored in Accumulator Register
 * Zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void sbc(uint8_t operand, uint8_t operand_type) {
    uint16_t result;
    uint8_t source_val;
    uint8_t carry_bit = HALF_CARRY_FLAG(read_8bit_reg(F));
    switch (operand_type) {
        case REG_8BIT:
            source_val = read_8bit_reg(operand);
            break;
        case CONST_8BIT:
            source_val = operand;
            break;
        case REG_POINTER:
            source_val = MEMORY[REGS[HL]];
            break;
    }
    result = REGS[A] - operand - carry_bit;
    REGS[A] = result;
    set_subtraction_flags(result, source_val);
}