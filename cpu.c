#include <stdint.h>
#include <stdbool.h>
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
        return (REGS[reg / 2] & 0xFF00) >> 4;
    }
    else {
        return REGS[(reg - 1) / 2] & 0x00FF;
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


/////////////////////////////////////////* FLAG SETTERS */////////////////////////////////////////
//TODO can we reduce from uint32?
/*
 * Sets zero flag if RESULT is zero, otherwise clears zero flag
*/
static void set_zero_flag(uint32_t result) {
    if (result == 0) {
        write_8bit_reg(F, SET_BIT(ZERO_BIT, read_8bit_reg(F)));
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
        write_8bit_reg(F, SET_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    }
    if ((length == BYTE && result > 0x00FF) || (length == WORD && result > 0xFFFF)) {
        write_8bit_reg(F, SET_BIT(CARRY_BIT, read_8bit_reg(F)));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    }
    set_zero_flag(result);
    write_8bit_reg(F, CLEAR_BIT(NEGATIVE_BIT, read_8bit_reg(F)));
}

/*
 * Uses the RESULT of a subtraction operation to set zero, half carry, and carry bit
 */
static void set_subtraction_flags(uint16_t result, uint8_t source_val) {
    if (FIRST_NIBBLE(source_val) > (FIRST_NIBBLE(read_8bit_reg(A)))) {
        write_8bit_reg(F, SET_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    }
    if (source_val > read_8bit_reg(A)) {
        write_8bit_reg(F, SET_BIT(CARRY_BIT, read_8bit_reg(F)));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(CARRY_BIT, read_8bit_reg(F)));
    }
    write_8bit_reg(F, SET_BIT(NEGATIVE_BIT, read_8bit_reg(F)));
    set_zero_flag((uint32_t) result);
}

/*
 * Uses the RESULT of a rotation operation to set flags
 * SET_ZERO signals weather to test for zero or to clear zero flag
 * CARRY_FLAG_NEW will indicate weather to set or clear the carry flag
 */
static void set_rot_flags(uint8_t result, bool set_zero, uint8_t carry_flag_new) {
    if (set_zero) {
        set_zero_flag((uint32_t) result);
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(ZERO_BIT, read_8bit_reg(F)));
    }
    if (carry_flag_new) {
        write_8bit_reg(F, SET_BIT(CARRY_BIT, read_8bit_reg(F)));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(CARRY_BIT, read_8bit_reg(F)));
    }
    write_8bit_reg(F, CLEAR_BIT(NEGATIVE_BIT, read_8bit_reg(F)));
    write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
}

/*
 * Returns value held by OPERAND of OPERAND_TYPE
*/
uint8_t get_8bit_operand(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = 0;

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
    return source_val;
}

/*
 * Loads SOURCE into DEST
 * Operand types are describes by DEST_TYPE and SOURCE_TYPE
*/
//TODO LD HL SP+e8 SETS FLAGS MIGHT BE ABLE TO USE ADDITION FLAGS FUNC
void ld(uint16_t dest, uint16_t source, uint8_t dest_type, uint8_t source_type) {
    uint8_t source_val = 0;
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
            break;
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

/////////////////////////////////////////* ARITHMETIC INSTRUCTIONS  */////////////////////////////////////////

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
    uint8_t carry_bit = HALF_CARRY_FLAG(read_8bit_reg(F));
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint32_t result = source_val + carry_bit;
    write_8bit_reg(A, (uint8_t) result);
    set_addition_flags(result, BYTE);
}

/*
 * Compare Instruction - Compares the value in A with the value in operand
 * Results are stored in Flag Register - zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void cp(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint16_t result = read_8bit_reg(A) - source_val;
    set_subtraction_flags(result, source_val);
}

/*
 * Subtract Instruction - Subtracts the value in A with the value in operand
 * Results are stored in Accumulator Register
 * Zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void sub(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint16_t result = read_8bit_reg(A) - source_val;
    set_subtraction_flags(result, source_val);
    write_8bit_reg(A, (uint8_t) result);
}

/*
 * Subtract Carry Instruction - Subtracts the value in A with the value in operand and the carry bit
 * Results are stored in Accumulator Register
 * Zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void sbc(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint8_t carry_bit = HALF_CARRY_FLAG(read_8bit_reg(F));
    uint16_t result = read_8bit_reg(A) - source_val - carry_bit;
    set_subtraction_flags(result, source_val);
    write_8bit_reg(A, (uint8_t) result);
}

/*
 * Increment instruction
 * Increments byte held by OPERAND
 */
void inc(uint8_t operand, uint8_t operand_type) {
    uint8_t result = 0;
    switch (operand_type) {
        case REG_8BIT:
            result = read_8bit_reg(operand) + 1;
            write_8bit_reg(operand, result);
            break;
        case REG_16BIT:
            REGS[operand] = REGS[operand] + 1;
            return;
        case REG_POINTER:
            result = MEMORY[REGS[operand]] + 1;
            MEMORY[REGS[operand]] = result;
            break;
    }
    if (result > 0x000F) {
        write_8bit_reg(F, SET_BIT(HALF_CARRY_BIT, read_8bit_reg(A)));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(A)));
    }
    set_zero_flag(result);
    write_8bit_reg(F, CLEAR_BIT(NEGATIVE_BIT, read_8bit_reg(A)));


}

/*
 * Decrement instruction
 * Decrements byte held by OPERAND
 */
void dec(uint8_t operand, uint8_t operand_type) {
    uint8_t result;
    uint16_t source_val;
    switch (operand_type) {
        case REG_8BIT:
            source_val = read_8bit_reg(operand);
            result = source_val - 1;
            write_8bit_reg(operand, result);
            break;
        case REG_16BIT:
            REGS[operand] = REGS[operand] - 1;
            return;
        case REG_POINTER:
            source_val = MEMORY[REGS[operand]];
            result = source_val - 1;
            MEMORY[REGS[operand]] = result;
            break;
    }
    //Borrow from bit four
    if (FIRST_NIBBLE(source_val) == 0) {
        write_8bit_reg(F, SET_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    }
    else {
        write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    }
    set_zero_flag(result);
    write_8bit_reg(F, SET_BIT(NEGATIVE_BIT, read_8bit_reg(A)));
}

/////////////////////////////////////////* LOGIC INSTRUCTIONS */////////////////////////////////////////

/*
 * Bitwise AND instruction
 * Results are stored in Accumulator Register
 * Zero and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
*/
void and(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint8_t result = read_8bit_reg(A) & source_val;
    write_8bit_reg(A, source_val);

    set_zero_flag((uint32_t) result);
    write_8bit_reg(F, SET_BIT(HALF_CARRY_BIT, read_8bit_reg(A)));
    write_8bit_reg(F, CLEAR_BIT(NEGATIVE_BIT, read_8bit_reg(A)));
    write_8bit_reg(F, CLEAR_BIT(CARRY_BIT, read_8bit_reg(A)));
}

/*
 * Bitwise OR instruction
 * Results are stored in Accumulator Register
 * Zero flag is set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
*/
void or(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint8_t result = read_8bit_reg(A) | source_val;
    write_8bit_reg(A, source_val);

    set_zero_flag((uint32_t) result);
    write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(A)));
    write_8bit_reg(F, CLEAR_BIT(NEGATIVE_BIT, read_8bit_reg(A)));
    write_8bit_reg(F, CLEAR_BIT(CARRY_BIT, read_8bit_reg(A)));
}

/*
 * Bitwise XOR instruction
 * Results are stored in Accumulator Register
 * Zero flag is set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
*/
void xor(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint8_t result = read_8bit_reg(A) ^ source_val;
    write_8bit_reg(A, source_val);

    set_zero_flag((uint32_t) result);
    write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(A)));
    write_8bit_reg(F, CLEAR_BIT(NEGATIVE_BIT, read_8bit_reg(A)));
    write_8bit_reg(F, CLEAR_BIT(CARRY_BIT, read_8bit_reg(A)));
}


/*
 * Complement accumulator instruction
 * Replaces value in accumulator with its complement
 */
void cpl() {
    write_8bit_reg(A, ~read_8bit_reg(A));
    write_8bit_reg(F, SET_BIT(NEGATIVE_BIT | HALF_CARRY_BIT, read_8bit_reg(F)));
}

/////////////////////////////////////////* BIT FLAG INSTRUCTIONS */////////////////////////////////////////

/*
 * Bit instruction
 * Tests if BIT in SOURCE_REG is set, sets the zero flag if not set
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void bit(uint8_t bit, uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : MEMORY[REGS[HL]];
    bool set = source_val & (0x0001 << bit);

    if (set) {
        write_8bit_reg(F, SET_BIT(ZERO_BIT, read_8bit_reg(F)));
    }
    write_8bit_reg(F, CLEAR_BIT(NEGATIVE_BIT, read_8bit_reg(F)));
    write_8bit_reg(F, SET_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
}

/*
 * Reset (clear) bit instruction
 * Clears BIT in SOURCE_REG
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void res(uint8_t bit, uint8_t source_reg, bool reg_8bit) {
    if (reg_8bit) {
        write_8bit_reg(source_reg, CLEAR_BIT(bit, source_reg));
    }
    else {
        MEMORY[REGS[HL]] = CLEAR_BIT(bit, MEMORY[REGS[HL]]);
    }
}


/*
 * Set bit instruction
 * Sets BIT in SOURCE_REG
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void set(uint8_t bit, uint8_t source_reg, bool reg_8bit) {
    if (reg_8bit) {
        write_8bit_reg(source_reg, SET_BIT(bit, source_reg));
    }
    else {
        MEMORY[REGS[HL]] = SET_BIT(bit, MEMORY[REGS[HL]]);
    }
}

/////////////////////////////////////////* BIT SHIFT INSTRUCTIONS */////////////////////////////////////////

/*
 * Rotate left instruction
 * Rotates bits in SOURCE_REG left, through the carry flag
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rl(uint8_t source_reg, bool reg_8bit) {
    uint8_t carry_flag_old = CARRY_FLAG(read_8bit_reg(F));
    uint8_t source_val = read_8bit_reg(source_reg);
    uint8_t carry_flag_new = source_val & 0x80 ? 1 : 0;
    uint8_t new_val = (source_val << 1) | carry_flag_old;
    reg_8bit ? write_8bit_reg(source_reg, new_val) : (MEMORY[REGS[HL]] = new_val);

    set_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Rotate left accumulator instruction
 * Rotates bits in accumulator left, through the carry flag
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rla() {
    uint8_t carry_flag_old = CARRY_FLAG(read_8bit_reg(F));
    uint8_t source_val = read_8bit_reg(A);
    uint8_t carry_flag_new = source_val & 0x80 ? 1 : 0;
    uint8_t new_val = (source_val << 1) | carry_flag_old;
    write_8bit_reg(A, new_val);

    set_rot_flags(new_val, false, carry_flag_new);
}

/*
 * Rotate left circular instruction
 * Rotate bits in SOURCE_REG left circularly, from MSB to LSB
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rlc(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : MEMORY[REGS[HL]];
    uint8_t carry_flag_new = source_val & 0x80 ? 1 : 0;
    uint8_t new_val = (source_val << 1) | carry_flag_new;
    reg_8bit ? write_8bit_reg(source_reg, new_val) : (MEMORY[REGS[HL]] = new_val);

    set_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Rotate left circular accumulator instruction
 * Rotate bits in accumulator left circularly, from MSB to LSB
 */
void rlca() {
    uint8_t source_val = read_8bit_reg(A);
    uint8_t carry_flag_new = source_val & 0x80 ? 1 : 0;
    uint8_t new_val = (source_val << 1) | carry_flag_new;
    write_8bit_reg(A, new_val);

    set_rot_flags(new_val, false, carry_flag_new);
}

/*
 * Rotate right instruction
 * Rotate bits in SOURCE_REG right, through the carry flag
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rr(uint8_t source_reg, bool reg_8bit) {
    uint8_t carry_flag_old = CARRY_FLAG(read_8bit_reg(F));
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : MEMORY[REGS[HL]];
    uint8_t carry_flag_new = source_val & 0x01 ? 1 : 0;
    uint8_t new_val = (source_val >> 1);
    new_val |= carry_flag_old << 8;
    reg_8bit ? write_8bit_reg(source_reg, new_val) : (MEMORY[REGS[HL]] = new_val);

    set_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Rotate right accumulator instruction
 * Rotate bits in SOURCE_REG right, through the carry flag
 * SOURCE reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rra() {
    uint8_t carry_flag_old = CARRY_FLAG(read_8bit_reg(F));
    uint8_t source_val = read_8bit_reg(A);
    uint8_t carry_flag_new = source_val & 0x01 ? 1 : 0;
    uint8_t new_val = (source_val >> 1);
    new_val |= carry_flag_old << 8;
    write_8bit_reg(A, new_val);

    set_rot_flags(new_val, false, carry_flag_new);
}

/*
 * Rotate right circular instruction
 * Rotate bits in SOURCE_REG right, from LSB to MSB
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rrc(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : MEMORY[REGS[HL]];
    uint8_t carry_flag_new = source_val & 0x01 ? 1 : 0;
    uint8_t new_msb = source_val & 0x0001;
    uint8_t new_val = (source_val >> 1);
    new_val |= new_msb << 8;
    reg_8bit ? write_8bit_reg(source_reg, new_val) : (MEMORY[REGS[HL]] = new_val);

    set_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Rotate right circular accumulator instruction
 * Rotate bits in accumulator right, from LSB to MSB
 */
void rrca() {
    uint8_t source_val = read_8bit_reg(A);
    uint8_t carry_flag_new = source_val & 0x01 ? 1 : 0;
    uint8_t new_msb = source_val & 0x01;
    uint8_t new_val = (source_val >> 1);
    new_val |= new_msb << 8;
    write_8bit_reg(A, new_val);

    set_rot_flags(new_val, false, carry_flag_new);
}

/*
 * Shift Left Arithmetically
 * Shift bits in SOURCE_REG left arithmetically
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void sla(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : MEMORY[REGS[HL]];
    uint8_t carry_flag_new = source_val & 0x80 ? 1 : 0;
    uint8_t new_val = source_val << 1;
    reg_8bit ? write_8bit_reg(source_reg, new_val) : (MEMORY[REGS[HL]] = new_val);

    set_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Source Right Arithmetically
 * Shift bits in SOURCE_REG right arithmetically
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void sra(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : MEMORY[REGS[HL]];
    uint8_t carry_flag_new = source_val & 0x01 ? 1 : 0;
    uint8_t new_msb = source_val & 0x80;
    uint8_t new_val = source_val >> 1;
    new_val |= new_msb;
    reg_8bit ? write_8bit_reg(source_reg, new_val) : (MEMORY[REGS[HL]] = new_val);

    set_rot_flags(new_val, false, carry_flag_new);
}

/*
 * Source Right Logically
 * Shift bits in SOURCE_REG right logically
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void srl(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : MEMORY[REGS[HL]];
    uint8_t carry_flag_new = source_val & 0x01 ? 1 : 0;
    uint8_t new_val = source_val >> 1;
    new_val &= 0xEF;
    reg_8bit ? write_8bit_reg(source_reg, new_val) : (MEMORY[REGS[HL]] = new_val);

    set_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Swap Instruction
 * Swap the upper 4 bits and the lower 4 bits in SOURCE_REG
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
*/
void swap(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : MEMORY[REGS[HL]];
    uint8_t new_val = ((source_val & 0x0F) << 4) | ((source_val & 0xF0) >> 4);
    reg_8bit ? write_8bit_reg(source_reg, new_val) : (MEMORY[REGS[HL]] = new_val);

    set_zero_flag((uint32_t) new_val);
    write_8bit_reg(F, CLEAR_BIT(NEGATIVE_BIT, read_8bit_reg(F)));
    write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
    write_8bit_reg(F, CLEAR_BIT(ZERO_BIT, read_8bit_reg(F)));
}

/////////////////////////////////////////* JUMPS AND SUBROUTINE INSTRUCTIONS */////////////////////////////////////////

void call(uint8_t cc, uint16_t const_16) {
    bool condition;
    uint8_t flags = read_8bit_reg(F);
    switch (cc) {
        case NZ:
            condition = (flags & (NEGATIVE_BIT | ZERO_BIT)) == (NEGATIVE_BIT | ZERO_BIT);
            break;
        case Z:
            condition = (flags & ZERO_BIT) == ZERO_BIT;
            break;
        case NC:
            condition = (flags & (NEGATIVE_BIT | CARRY_BIT)) == (NEGATIVE_BIT | CARRY_BIT);
            break;
        case CARRY:
            condition = (flags & CARRY) == CARRY_BIT;
            break;
        default:
            condition = true;
            break;
    }
    if (!condition) {
        return;
    }
    MEMORY[REGS[SP]] = PC + 2;
    REGS[SP] += 2;
    REGS[PC] = const_16;
}

void jp(uint8_t cc, bool is_hl) {
    bool condition;
    uint8_t flags = read_8bit_reg(F);
    switch (cc) {
        case NZ:
            condition = (flags & (NEGATIVE_BIT | ZERO_BIT)) == (NEGATIVE_BIT | ZERO_BIT);
            break;
        case Z:
            condition = (flags & ZERO_BIT) == ZERO_BIT;
            break;
        case NC:
            condition = (flags & (NEGATIVE_BIT | CARRY_BIT)) == (NEGATIVE_BIT | CARRY_BIT);
            break;
        case CARRY:
            condition = (flags & CARRY) == CARRY_BIT;
            break;
        default:
            condition = true;
            break;
    }
    if (!condition) {
        return;
    }
    uint16_t address = is_hl ? REGS[HL] : fetch_word();
    REGS[PC] = address;
}

void jr(uint8_t cc) {
    bool condition;
    uint8_t flags = read_8bit_reg(F);
    switch (cc) {
        case NZ:
            condition = (flags & (NEGATIVE_BIT | ZERO_BIT)) == (NEGATIVE_BIT | ZERO_BIT);
            break;
        case Z:
            condition = (flags & ZERO_BIT) == ZERO_BIT;
            break;
        case NC:
            condition = (flags & (NEGATIVE_BIT | CARRY_BIT)) == (NEGATIVE_BIT | CARRY_BIT);
            break;
        case CARRY:
            condition = (flags & CARRY) == CARRY_BIT;
            break;
        default:
            condition = true;
            break;
    }
    if (!condition) {
        return;
    }
    int8_t offset = (int8_t) fetch_byte();
    REGS[PC] = REGS[PC] + 2 + offset;
}

void ret(uint8_t cc) {
    bool condition;
    uint8_t flags = read_8bit_reg(F);
    switch (cc) {
        case NZ:
            condition = (flags & (NEGATIVE_BIT | ZERO_BIT)) == (NEGATIVE_BIT | ZERO_BIT);
            break;
        case Z:
            condition = (flags & ZERO_BIT) == ZERO_BIT;
            break;
        case NC:
            condition = (flags & (NEGATIVE_BIT | CARRY_BIT)) == (NEGATIVE_BIT | CARRY_BIT);
            break;
        case CARRY:
            condition = (flags & CARRY) == CARRY_BIT;
            break;
        default:
            condition = true;
            break;
    }
    if (!condition) {
        return;
    }
    REGS[PC] = REGS[PC] & MEMORY[REGS[SP]++];
    REGS[PC] = REGS[PC] & (MEMORY[REGS[SP]++] << 4);

}

//TODO RETI
//TODO RST

/////////////////////////////////////////* CARRY FLAG INSTRUCTIONS */////////////////////////////////////////

void ccf() {
    write_8bit_reg(F, read_8bit_reg(F) ^ CARRY_BIT);
    write_8bit_reg(F, CLEAR_BIT(NEGATIVE_BIT, read_8bit_reg(F)));
    write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
}

void scf() {
    write_8bit_reg(F, SET_BIT(CARRY_BIT, read_8bit_reg(F)));
    write_8bit_reg(F, CLEAR_BIT(NEGATIVE_BIT, read_8bit_reg(F)));
    write_8bit_reg(F, CLEAR_BIT(HALF_CARRY_BIT, read_8bit_reg(F)));
}

/////////////////////////////////////////* STACK MANIPULATION */////////////////////////////////////////

void pop(uint8_t reg_16) {
    REGS[reg_16] = REGS[reg_16] & MEMORY[REGS[SP]++];
    REGS[reg_16] = REGS[reg_16] & (MEMORY[REGS[SP]++] << 4);
}

void push(uint8_t reg_16) {
    MEMORY[--REGS[SP]] = (uint8_t) ((REGS[reg_16] & 0xF0) >> 4);
    MEMORY[--REGS[SP]] = (uint8_t) (REGS[reg_16] & 0x0F);4
}