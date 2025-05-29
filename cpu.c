#include <stdint.h>
#include "gb.h"
#include "cpu.h"
#include "decode.h"
#define ZERO_FLAG(f) (f & 0x80)
#define SUBTRACTION_FLAG(f) (f & 0x40)
#define HALF_CARRY_FLAG(f) (f & 0x20)
#define CARRY_FLAG(f) (f & 0x10)


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
 */
void add(uint16_t operand, uint8_t operand_type) {
    uint16_t result;
    switch (operand_type) {
        case REG_8BIT:
            result = read_8bit_reg(A) + read_8bit_reg(operand);
            write_8bit_reg(A, result);
            break;
        case REG_16BIT:
            result = REGS[HL] + REGS[operand];
            REGS[HL] = result;
            break;
        case CONST_8BIT:
            result = read_8bit_reg(A) + operand;
            write_8bit_reg(A, result);
            break;
        case CONST_16BIT:
            result = REGS[HL] + operand;
            REGS[HL] = result;
            break;
        case REG_POINTER:
            result = read_8bit_reg(A) + MEMORY[REGS[HL]];
            write_8bit_reg(A, result);
            break;
        case OFFSET:
            result = REGS[SP] + (int8_t)operand;
            REGS[SP] = result;
            break;
    }
}