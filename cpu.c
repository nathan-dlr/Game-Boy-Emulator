#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "gb.h"
#include "cpu.h"
#include "decode.h"
#define ZERO_FLAG(f) (f & 0x80)
#define SUBTRACTION_FLAG(f) (f & 0x40)
#define HALF_CARRY_FLAG(f) (f & 0x20)
#define CARRY_FLAG(f) (f & 0x10)
#define ZERO_BIT 0x80
#define NEGATIVE_BIT 0x40
#define HALF_CARRY_BIT 0x20
#define CARRY_BIT 0x10
#define SET_BIT(bit, value) (bit | value)
#define CLEAR_BIT(bit, value) (~bit & value)
#define BYTE 0
#define WORD 1
#define FIRST_NIBBLE(x) (x & 0x000F)

/* Interrupt Enable and Interrupt Flag */
#define IE 0xFFFF
#define IF 0xFF0F

/* REGISTERS */
static uint16_t REGS[6];

/* Interrupt Master Enable Flag */
static bool IME = false;
static uint8_t CPU_STATE_temp;

//TODO CReate a MEMORY DATA REGISTER and MEMORY ADDRESS REGISTER? not actually in gameboy but would be useful in transferring data between ticks
//TODO QUEUE FUNCTION X TIMES WHERE X = #CYLCLES, EACH FUNCTION WILL DO WORK BAsED ON A SWITCH STATEMENT CASE 0 - DO CYCLE 1 WORK, CASE 1 - DO CYCLE 2 WORK
//TODO some parameters take in a enum but the parameter type is uint8t, change to the actual enum type
//BUT HOW DO WE KEEP THE INFORMATION - static variables are ugly and bad - STORE ON THE BUS!!!!
//theres an address bus and a data bus
//COULD WE CREATE A OPERAND
//INSTRUCTION CYCLES COULD BE KEPT TRACK OF IN CPU STATE


void cpu_init() {
    REGS[AF] = 0x01B0;
    REGS[BC] = 0x0013;
    REGS[DE] = 0x00D8;
    REGS[HL] = 0x014D;
    REGS[SP] = 0xFFFE;
    REGS[PC] = 0x0100;
    CPU_STATE_temp = RUNNING;
}

//this will change to tick function
//if decode we queue up more
void execute_next_instruction() {
    uint8_t next_byte = fetch_byte();
    decode(next_byte);
}

///////////////////////////////////////// GENERAL HELPER FUNCTIONS /////////////////////////////////////////

static uint16_t get_reg_pair(uint8_t reg_pair) {
    return (CPU.REGS[reg_pair * 2] >> 8) | CPU.REGS[(reg_pair * 2) + 1];
}

static uint16_t write_reg_pair(uint8_t reg_pair, uint16_t value) {
    CPU.REGS[reg_pair * 2] = (uint8_t) value >> 8;
    CPU.REGS[(reg_pair * 2) + 1] = (uint8_t) value;
}

/*
 * Returns byte pointed to by PC
*/
uint8_t fetch_byte() {
    uint8_t byte = read_memory(REGS[PC]);
    REGS[PC] += 0x0001;
    return byte;
}

/*
 * Puts PC onto address bus and reads that byte onto the data bus
 */
uint8_t read_next_byte() {
    CPU.ADDRESS_BUS = CPU.REGS[PC];
    read_memory2();
    CPU.REGS[PC]++;
}

/*
 * Returns word pointed to by PC
*/
uint16_t fetch_word() {
    uint16_t word = (read_memory(REGS[PC] + 1) << 8) + read_memory(REGS[PC]);
    REGS[PC] += 0x0002;
    return word;
}

/*
 * Helper function for 8bit arithmetic instructions
 * Returns value held by OPERAND of OPERAND_TYPE
*/
static uint8_t get_8bit_operand(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = 0;
    switch (operand_type) {
        case REG_8BIT:
            source_val = read_8bit_reg(operand);
            break;
        case CONST_8BIT:
            source_val = operand;
            break;
        case REG_POINTER:
            source_val = read_memory(REGS[HL]);
            break;
        default:
            perror("Invalid operand in get_8bit_operand function");
            break;
    }
    return source_val;
}


///////////////////////////////////////// FLAG SETTERS /////////////////////////////////////////

/*
 * Sets zero flag if RESULT is zero, otherwise clears zero flag
*/
static uint8_t get_zero_flag(uint16_t result) {
    if (result == 0) {
        return ZERO_BIT;
    }
    return 0x00;
}

/*
 * Uses the SUM of a byte addition operation and the augend of the
 * operation before it was performed to set flags
*/
static uint8_t get_add_flags_byte(uint8_t sum, uint8_t augend) {
    uint8_t flags = 0x00;
    if ((FIRST_NIBBLE(sum)) < (FIRST_NIBBLE(augend))) {
        flags |= HALF_CARRY_BIT;
    }
    if (sum < augend) {
        flags |= CARRY_BIT;
    }
    return flags;
}

/*
 * Uses the SUM of a word addition operation and the augend of the
 * operation before it was performed to set flags
*/
static uint8_t get_add_flags_word(uint16_t sum, uint16_t augend) {
    uint8_t flags = 0x00;
    if ((sum & 0x0FFF) < (augend & 0x0FFF)) {
        flags |= HALF_CARRY_BIT;
    }
    if (sum < augend) {
        flags |= CARRY_BIT;
    }
    return flags;
}

/*
 * Uses the SUBTRAHEND and MINUEND of a subtraction operation to set zero, half carry, and carry bit
 */
static uint8_t get_subtraction_flags(uint8_t minuend, uint8_t subtrahend) {
    uint8_t flags = NEGATIVE_BIT;
    if (FIRST_NIBBLE(subtrahend) > (FIRST_NIBBLE(minuend))) {
        flags |= HALF_CARRY_BIT;
    }
    if (subtrahend > minuend) {
        flags |= CARRY_BIT;
    }
    return flags;
}

/*
 * Uses the RESULT of a rotation operation to set flags
 * SET_ZERO signals weather to test for zero or to clear zero flag
 * CARRY_FLAG_NEW will indicate weather to set or clear the carry flag
 */
static uint8_t get_rot_flags(uint8_t result, bool set_zero, bool carry_flag_new) {
    uint8_t flags = 0x00;
    if (set_zero) {
        flags |= get_zero_flag((uint32_t) result);
    }
    if (carry_flag_new) {
        flags |= CARRY_BIT;
    }
    return flags;
}

//LD r8,r8 - 1 cycle: decode/ld_r8_bus
//LD r8,n8 - 2 cycles: decode -> ld_r8_imm
//LD r16,n16 - 3 cycles: decode -> ld_r8_imm -> ld_r8_imm
//LD [HL],r8 - 2 cycles: decode -> write_memory
//LD [HL],n8 - 3 cycles: decode -> read_next_byte -> write_bus
//LD r8,[HL] - 2 cycles: decode -> ld_r8_addr_bus
// LD [r16],A - 2 cycles: decode -> write_memory
//LD [n16],A - 4 cycles: decode -> read_next_byte -> read_next_byte -> write_memory
//LDH [n16],A - 3 cycles: decode -> read_next_byte -> write_memory
//LDH [C],A - 2 cycles: decode -> write_memory
//LD A,[r16] - 2 cycles: decode -> read_memory/ld_r8_bus
//LD A,[n16] - 4 cycles: decode -> read_next_byte -> read_next_byte -> read_memory/ld_r8_bus
//LDH A,[n16] - 3 cycles: decode -> read_next_byte -> read_memory/ld_r8_bus
//LDH A,[C] - 2 cycles: decode -> read_memory/ld_r8_bus
//LD [HLI],A - 2 cycles: decode -> write_memory
//LD [HLD],A - 2 cycles: decode -> write_memory
//LD A,[HLI] - 2 cycles: decode -> read_memory/ld_r8_bus
//LD A,[HLD] - 2 cycles: decode -> read_memory/ld_r8_bus
//LD SP,n16 - 3 cycles: decode -> read_next_byte/ld_r8_bus -> read_next_byte/ld_r8_bus
//LD [n16],SP - 5 cycles: decode -> read_next_byte -> read_next_byte -> write_memory -> write_memory
//LD HL,SP+e8 - 3 cycles: decode -> read_next_byte -> add to low byte of sp and store in L -> add any carry from low byte add and store in H
//LD SP,HL - 2 cycles : decode -> ld_sp_hl

//ADD A,r8 - 1 cycle: decode/add
//ADD A,[HL] - 2 cycles: decode/set address bus -> read byte -> add
//ADD A,n8 - 2 cycles: decode -> read_next_byte/add
//ADD HL,r16 - 2 cycles: decode -> add first bit -> add second bit
//ADD HL,SP - 2 cycles: decode -> add first bit -> add second bit
//ADD SP,e8 -

//TODO PUT PC ON ADDR BUS BEFORE OR DURING FETCH NEXT BYTE?
/*
 * Loads byte at [PC] into 8bit register indicated by DEST
 */
void ld_r8_imm8(uint8_t dest) {
    read_next_byte();
    CPU.REGS[dest] = CPU.DATA_BUS;
}

/*
 * Loads [PC] into temporary register W
 * Puts 8bit register A onto data but if indicated by LOAD_A
 */
void ld_rW_imm8(uint8_t load_a) {
    read_next_byte();
    write_8bit_reg(W, CPU.DATA_BUS);
    CPU.ADDRESS_BUS = get_reg_pair(WZ);
    if (load_a) {
        CPU.DATA_BUS = CPU.REGS[A];
    }
}

/*
 * Loads [ADDR_BUS] into 8bit register indicated by DEST
 */
void ld_r8_addr_bus(uint8_t dest) {
    read_memory2();
    CPU.REGS[dest] = CPU.DATA_BUS;
}

/*
 * Used for LDH with an immediate value
 * Adds the immediate value to 0xFF00 and puts it on the ADDR_BUS
 */
void ldh_imm8() {
    read_next_byte();
    CPU.ADDRESS_BUS = 0xFF00 + CPU.DATA_BUS;
}

/*
 * Loads
 */
void ld_imm16_sp(uint8_t byte_num) {
    CPU.ADDRESS_BUS = get_reg_pair(WZ);
    CPU.DATA_BUS = byte_num == 0 ? CPU.REGS[SP0] : CPU.REGS[SP1];
    write_memory2();
    write_reg_pair(WZ, get_reg_pair(WZ) + 1);
}

void ld_hl_sp8() {
    uint16_t sp = get_reg_pair(SP);
    write_reg_pair(HL, sp + CPU.DATA_BUS);
    CPU.REGS[F] = get_add_flags_byte(CPU.REGS[HL], sp);
}

void ld_sp_hl() {
    write_reg_pair(SP, get_reg_pair(HL));
}

///////////////////////////////////////// ARITHMETIC INSTRUCTIONS  /////////////////////////////////////////

/*
 * Add Instruction
 * Takes in either 8bit/16bit reg, 8bit/16bit const, register pointer (HL), or signed offset
 * Depending on operand, result will be stored in either accumulator, HL, or SP
 * Zero, carry, and half-carry flags are set
 */
//void add(uint16_t operand, uint8_t operand_type) {
//    uint16_t result;
//    uint8_t accumulator = read_8bit_reg(A);
//    uint8_t new_flags = 0x00;
//    switch (operand_type) {
//        case REG_8BIT:
//            result = accumulator + read_8bit_reg(operand);
//            new_flags = get_add_flags_byte((uint8_t) result, accumulator);
//            new_flags |= get_zero_flag((uint8_t) result);
//            write_8bit_reg(A, (uint8_t) result);
//            break;
//        case REG_16BIT:
//            result = REGS[HL] + REGS[operand];
//            new_flags = get_add_flags_word(result, REGS[HL]);
//            new_flags |= ZERO_FLAG(read_8bit_reg(F));
//            REGS[HL] = result;
//            break;
//        case CONST_8BIT:
//            result = accumulator + operand;
//            new_flags = get_add_flags_byte((uint8_t) result, accumulator);
//            new_flags |= get_zero_flag((uint8_t) result);
//            write_8bit_reg(A, (uint8_t) result);
//            break;
//        case REG_POINTER:
//            result = accumulator + read_memory(REGS[HL]);
//            new_flags = get_add_flags_byte((uint8_t) result, accumulator);
//            new_flags |= get_zero_flag((uint8_t) result);
//            write_8bit_reg(A, (uint8_t) result);
//            break;
//        case OFFSET:
//            result = REGS[SP] + (int8_t) operand;
//            new_flags = get_add_flags_byte((uint8_t) result, REGS[SP]);
//            REGS[SP] = result;
//            break;
//        default:
//            perror("Invalid operand in add");
//    }
//    write_8bit_reg(F, new_flags);
//}

void add_imm() {
    read_next_byte();
    add(A);
}
void add(uint8_t dest) {
    uint8_t result = CPU.REGS[dest] + CPU.DATA_BUS;
    uint8_t flags = get_add_flags_byte(result, CPU.REGS[dest]);
    flags |= get_zero_flag(result);
    CPU.REGS[dest] = result;
    CPU.REGS[F] = flags;
}

/*
 * Add-Carry Instruction - adds operand, carry bit, and accumulator
 * Result stored in Accumulator Register
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 * Zero, carry, and half-carry flags are set
 */
void adc(uint8_t imm) {
    if (imm) {
        read_next_byte();
    }
    uint8_t carry_bit = CARRY_FLAG(CPU.REGS[F]) ? 0x01 : 0x00;
    uint8_t accumulator = CPU.REGS[A];
    uint8_t source_val = CPU.DATA_BUS;

    uint8_t intermediate = accumulator + carry_bit;
    uint8_t result = source_val + intermediate;
    uint8_t flags = get_add_flags_byte(intermediate, accumulator);
    flags |= (get_add_flags_byte(result, intermediate) | get_zero_flag(result));

    CPU.REGS[F] = flags;
    CPU.REGS[A] = result;
}

/*
 * Compare Instruction - Compares the value in A with the value in operand
 * Results are stored in Flag Register - zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void cp(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint8_t accumulator = read_8bit_reg(A);
    uint16_t result = accumulator - source_val;
    uint8_t flags = get_subtraction_flags(accumulator, source_val) | get_zero_flag(result);
    write_8bit_reg(F, flags);
}

/*
 * Subtract Instruction - Subtracts the value in A with the value in operand
 * Results are stored in Accumulator Register
 * Zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void sub(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint8_t accumulator = read_8bit_reg(A);
    uint16_t result = accumulator - source_val;
    uint8_t flags = get_subtraction_flags(accumulator, source_val) | get_zero_flag(result);
    write_8bit_reg(F, flags);
    write_8bit_reg(A, (uint8_t) result);
}

/*
 * Subtract Carry Instruction - Subtracts the value in A with the value in operand and the carry bit
 * Results are stored in Accumulator Register
 * Zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void sbc(uint8_t operand, uint8_t operand_type) {
    uint8_t accumulator = read_8bit_reg(A);
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint8_t carry_bit = CARRY_FLAG(read_8bit_reg(F)) ? 0x01 : 0x00;

    uint8_t intermediate = accumulator - source_val;
    uint8_t result = intermediate - carry_bit;
    uint8_t flags = get_subtraction_flags(accumulator, source_val);
    flags |= get_subtraction_flags(intermediate, carry_bit) | get_zero_flag(result);

    write_8bit_reg(F, flags);
    write_8bit_reg(A, result);
}

/*
 * Increment instruction
 * Increments byte held by OPERAND
 */
void inc(uint8_t operand, uint8_t operand_type) {
    uint8_t result = 0;
    uint8_t source = 0;
    switch (operand_type) {
        case REG_8BIT:
            source = read_8bit_reg(operand);
            result = source + 1;
            write_8bit_reg(operand, result);
            break;
        case REG_16BIT:
            REGS[operand] = REGS[operand] + 1;
            return;
        case REG_POINTER:
            source = read_memory(REGS[operand]);
            result = source + 1;
            write_memory(REGS[operand], result);
            break;
        default:
            perror("Invalid Operand in INC");
    }

    uint8_t flags = CARRY_FLAG(read_8bit_reg(F));
    if ((source & 0x000F) == 0x000F) {
        flags |= HALF_CARRY_BIT;
    }
    flags |= get_zero_flag(result);
    write_8bit_reg(F, flags);


}

/*
 * Decrement instruction
 * Decrements byte held by OPERAND
 */
void dec(uint8_t operand, uint8_t operand_type) {
    uint8_t result;
    uint16_t source_val ;
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
            source_val = read_memory(REGS[operand]);
            result = source_val - 1;
            write_memory(REGS[operand], result);
            break;
        default:
            result = 0;
            source_val = 0;
            perror("Invalid operand in DEC");
    }
    //Borrow from bit four
    uint8_t flags = CARRY_FLAG(read_8bit_reg(F)) | NEGATIVE_BIT;
    if (FIRST_NIBBLE(source_val) == 0) {
        flags |= HALF_CARRY_BIT;
    }
    flags |= get_zero_flag(result);
    write_8bit_reg(F, flags);
}

///////////////////////////////////////// LOGIC INSTRUCTIONS /////////////////////////////////////////

/*
 * Bitwise AND instruction
 * Results are stored in Accumulator Register
 * Zero and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
*/
void and(uint8_t operand, uint8_t operand_type) {
    uint8_t source_val = get_8bit_operand(operand, operand_type);
    uint8_t result = read_8bit_reg(A) & source_val;

    write_8bit_reg(A, result);
    write_8bit_reg(F, get_zero_flag(result) | HALF_CARRY_BIT);
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
    write_8bit_reg(A, result);

    write_8bit_reg(F, get_zero_flag((uint32_t) result));
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
    write_8bit_reg(A, result);

    write_8bit_reg(F, get_zero_flag((uint32_t) result));
}


/*
 * Complement accumulator instruction
 * Replaces value in accumulator with its complement
 */
void cpl() {
    write_8bit_reg(A, ~read_8bit_reg(A));
    write_8bit_reg(F, SET_BIT(NEGATIVE_BIT | HALF_CARRY_BIT, read_8bit_reg(F)));
}

///////////////////////////////////////// BIT FLAG INSTRUCTIONS /////////////////////////////////////////

/*
 * Bit instruction
 * Tests if BIT in SOURCE_REG is set, sets the zero flag if not set
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void bit(uint8_t bit_num, uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : read_memory(REGS[HL]);
    bool set = source_val & (0x0001 << bit_num);

    uint8_t flags = CARRY_FLAG(read_8bit_reg(F)) | HALF_CARRY_BIT;
    if (!set) {
        flags |= ZERO_BIT;
    }
    write_8bit_reg(F, flags);
}

/*
 * Reset (clear) bit_num instruction
 * Clears BIT in SOURCE_REG
 * Source reg is either 8-bit_num reg or byte pointed to by HL, indicated by REG_8BIT
 */
void res(uint8_t bit_num, uint8_t source_reg, bool reg_8bit) {
    uint8_t bit = 0x01 << bit_num;
    if (reg_8bit) {
        write_8bit_reg(source_reg, CLEAR_BIT(bit, read_8bit_reg(source_reg)));
    }
    else {
        write_memory(REGS[HL], CLEAR_BIT(bit, read_memory(REGS[HL])));
    }

}


/*
 * Set bit instruction
 * Sets BIT in SOURCE_REG
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void set(uint8_t bit_num, uint8_t source_reg, bool reg_8bit) {
    uint8_t bit = 0x01 << bit_num;
    if (reg_8bit) {
        write_8bit_reg(source_reg, SET_BIT(bit, read_8bit_reg(source_reg)));
    }
    else {
        write_memory(REGS[HL], SET_BIT(bit, read_memory(REGS[HL])));
    }
}

///////////////////////////////////////// BIT SHIFT INSTRUCTIONS /////////////////////////////////////////

/*
 * Rotate left instruction
 * Rotates bits in SOURCE_REG left, through the carry flag
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rl(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : read_memory(REGS[HL]);
    uint8_t carry_flag_old = CARRY_FLAG(read_8bit_reg(F)) ? 0x01 : 0x00;
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = (source_val << 1) | carry_flag_old;

    reg_8bit ? write_8bit_reg(source_reg, new_val) : write_memory(REGS[HL], new_val);
    write_8bit_reg(F, get_rot_flags(new_val, true, carry_flag_new));
}

/*
 * Rotate left accumulator instruction
 * Rotates bits in accumulator left, through the carry flag
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rla() {
    uint8_t carry_flag_old = CARRY_FLAG(read_8bit_reg(F)) ? 0x01 : 0x00;
    uint8_t source_val = read_8bit_reg(A);
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = (source_val << 1) | carry_flag_old;

    write_8bit_reg(A, new_val);
    write_8bit_reg(F, get_rot_flags(new_val, false, carry_flag_new));
}

/*
 * Rotate left circular instruction
 * Rotate bits in SOURCE_REG left circularly, from MSB to LSB
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rlc(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : read_memory(REGS[HL]);
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = source_val << 1;
    new_val |= carry_flag_new ? 0x01 : 0x00;

    reg_8bit ? write_8bit_reg(source_reg, new_val) : write_memory(REGS[HL], new_val);
    write_8bit_reg(F, get_rot_flags(new_val, true, carry_flag_new));
}

/*
 * Rotate left circular accumulator instruction
 * Rotate bits in accumulator left circularly, from MSB to LSB
 */
void rlca() {
    uint8_t source_val = read_8bit_reg(A);
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = source_val << 1;
    new_val |= carry_flag_new ? 0x01 : 0x00;

    write_8bit_reg(A, new_val);
    write_8bit_reg(F, get_rot_flags(new_val, false, carry_flag_new));
}

/*
 * Rotate right instruction
 * Rotate bits in SOURCE_REG right, through the carry flag
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rr(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : read_memory(REGS[HL]);
    uint8_t carry_flag_old = CARRY_FLAG(read_8bit_reg(F)) ? 0x01 : 0x00;
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_val = (source_val >> 1);
    new_val |= carry_flag_old << 7;

    reg_8bit ? write_8bit_reg(source_reg, new_val) : write_memory(REGS[HL], new_val);
    write_8bit_reg(F, get_rot_flags(new_val, true, carry_flag_new));
}

/*
 * Rotate right accumulator instruction
 * Rotate bits in SOURCE_REG right, through the carry flag
 * SOURCE reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rra() {
    uint8_t carry_flag_old = CARRY_FLAG(read_8bit_reg(F)) ? 0x01 : 0x00;
    uint8_t source_val = read_8bit_reg(A);
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_val = (source_val >> 1);
    new_val |= carry_flag_old << 7;

    write_8bit_reg(A, new_val);
    write_8bit_reg(F, get_rot_flags(new_val, false, carry_flag_new));
}

/*
 * Rotate right circular instruction
 * Rotate bits in SOURCE_REG right, from LSB to MSB
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rrc(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : read_memory(REGS[HL]);
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_msb = source_val & 0x0001;
    uint8_t new_val = (source_val >> 1);
    new_val |= new_msb << 7;

    reg_8bit ? write_8bit_reg(source_reg, new_val) : write_memory(REGS[HL], new_val);
    write_8bit_reg(F, get_rot_flags(new_val, true, carry_flag_new));
}

/*
 * Rotate right circular accumulator instruction
 * Rotate bits in accumulator right, from LSB to MSB
 */
void rrca() {
    uint8_t source_val = read_8bit_reg(A);
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_msb = source_val & 0x01;
    uint8_t new_val = (source_val >> 1);
    new_val |= new_msb << 7;

    write_8bit_reg(A, new_val);
    write_8bit_reg(F, get_rot_flags(new_val, false, carry_flag_new));
}

/*
 * Shift Left Arithmetically
 * Shift bits in SOURCE_REG left arithmetically
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void sla(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : read_memory(REGS[HL]);
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = source_val << 1;

    reg_8bit ? write_8bit_reg(source_reg, new_val) : write_memory(REGS[HL], new_val);
    write_8bit_reg(F, get_rot_flags(new_val, true, carry_flag_new));
}

/*
 * Shift Right Arithmetically
 * Shift bits in SOURCE_REG right arithmetically
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void sra(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : read_memory(REGS[HL]);
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_msb = source_val & 0x80;
    uint8_t new_val = source_val >> 1;
    new_val |= new_msb;

    reg_8bit ? write_8bit_reg(source_reg, new_val) : write_memory(REGS[HL], new_val);
    write_8bit_reg(F, get_rot_flags(new_val, true, carry_flag_new));
}

/*
 * Shit Right Logically
 * Shift bits in SOURCE_REG right logically
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void srl(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : read_memory(REGS[HL]);
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_val = source_val >> 1;

    reg_8bit ? write_8bit_reg(source_reg, new_val) : write_memory(REGS[HL], new_val);
    write_8bit_reg(F, get_rot_flags(new_val, true, carry_flag_new));
}

/*
 * Swap Instruction
 * Swap the upper 4 bits and the lower 4 bits in SOURCE_REG
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
*/
void swap(uint8_t source_reg, bool reg_8bit) {
    uint8_t source_val = reg_8bit ? read_8bit_reg(source_reg) : read_memory(REGS[HL]);
    uint8_t new_val = ((source_val & 0x0F) << 4) | ((source_val & 0xF0) >> 4);

    reg_8bit ? write_8bit_reg(source_reg, new_val) : write_memory(REGS[HL], new_val);
    write_8bit_reg(F, get_zero_flag((uint32_t) new_val));
}

///////////////////////////////////////// JUMPS AND SUBROUTINE INSTRUCTIONS /////////////////////////////////////////

/*
 * Helper function for jumps and subroutines that evaluates if given condition codes match with those in flag register
 * Returns true if condition codes match, false otherwise
 */
static bool evaluate_condition_codes(uint8_t cc) {
    uint8_t flags = read_8bit_reg(F);
    switch (cc) {
        case NZ:
            return (flags | ~ZERO_BIT) == ~ZERO_BIT;
        case Z:
            return (flags & ZERO_BIT) == ZERO_BIT;
        case NC:
            return (flags | ~CARRY_BIT) == ~CARRY_BIT;
        case CARRY:
            return (flags & CARRY_BIT) == CARRY_BIT;
        default:
            return true;
    }
}

/*
 * Call Instruction
 * Pushes the address of the instruction after the call on the stack, such that RET can pop it later;
 * then it executes an implicit JP using the address pointed to by ADDRESS
 * Only executes if CC is met
 */
void call(uint8_t cc, uint16_t address) {
    if (!evaluate_condition_codes(cc)) {
        return;
    }
    REGS[SP]--;
    write_memory(REGS[SP], (uint8_t) ((REGS[PC] & 0xFF00) >> 8));
    REGS[SP]--;
    write_memory(REGS[SP], (uint8_t) (REGS[PC] & 0x00FF));
    REGS[PC] = address;
}

/*
 * Jump Instruction
 * Jumps to address pointed to by either the next word or by the contents of HL, indicated by IS_HL, if CC is met
 */
void jp(uint8_t cc, bool is_hl) {
    uint16_t address;
    if (is_hl) {
        address = REGS[HL];
    }
    else {
        address = fetch_word();
        if (!evaluate_condition_codes(cc)) {
            return;
        }
    }
    REGS[PC] = address;
}

/*
 * Relative Jump Instruction
 * Jumps to some relative offset if CC is met
 */
void jr(uint8_t cc) {
    int8_t offset = (int8_t) fetch_byte();
    if (!evaluate_condition_codes(cc)) {
        return;
    }
    REGS[PC] = REGS[PC] + offset;
}

/*
 * Return from subroutine
 * Pops the PC from the stack if cc is met
 */
void ret(uint8_t cc) {
    if (!evaluate_condition_codes(cc)) {
        return;
    }
    REGS[PC] = 0x0000;
    REGS[PC] = read_memory(REGS[SP]);
    REGS[SP]++;
    REGS[PC] = REGS[PC] | (read_memory(REGS[SP]) << 8);
    REGS[SP]++;
}

/*
 * Return from subroutine and enable interrupts
 */
void reti() {
    IME = true;
    REGS[PC] = 0x0000;
    REGS[PC] = read_memory(REGS[SP]);
    REGS[SP]++;
    REGS[PC] = REGS[PC] | (read_memory(REGS[SP]) << 8);
    REGS[SP]++;
}

/*
 * Call address VEC
 */
void rst(uint8_t opcode) {
    uint8_t vec = (opcode & 0x38 >> 3) * 8;

    REGS[SP]--;
    write_memory(REGS[SP], (uint8_t) ((REGS[PC] & 0xFF00) >> 8));
    REGS[SP]--;
    write_memory(REGS[SP], (uint8_t) (REGS[PC] & 0x00FF));
    REGS[PC] = vec;

}

///////////////////////////////////////// CARRY FLAG INSTRUCTIONS /////////////////////////////////////////

/*
 * Complement Carry Flag
 */
void ccf() {
    uint8_t flags = read_8bit_reg(F);
    flags = (CARRY_FLAG(flags) ^ CARRY_BIT) | ZERO_FLAG(flags);
    write_8bit_reg(F, flags);
}

/*
 * Set Carry Flag
 */
void scf() {
    uint8_t flags = CARRY_BIT | ZERO_FLAG(read_8bit_reg(F));
    write_8bit_reg(F, flags);
}

///////////////////////////////////////// STACK MANIPULATION /////////////////////////////////////////

/*
 * Pop Instruction
 * Pop register whose index is indicated by REG_16 from the stack
 */
void pop(uint8_t reg_16) {
    REGS[reg_16] = 0x0000;
    REGS[reg_16] = read_memory(REGS[SP]);
    REGS[SP]++;
    REGS[reg_16] = REGS[reg_16] | (read_memory(REGS[SP]) << 8);
    REGS[SP]++;
    if (reg_16 == AF) {
        REGS[AF] &= 0xFFF0;
    }
}

/*
 * Push Instruction
 * Push register whose index is indicated by REG_16 from the stack
 */
void push(uint8_t reg_16) {
    REGS[SP]--;
    write_memory(REGS[SP], (uint8_t) ((REGS[reg_16] & 0xFF00) >> 8));
    REGS[SP]--;
    write_memory(REGS[SP], (uint8_t) (REGS[reg_16] & 0x00FF));
}

///////////////////////////////////////// INTERRUPT-RELATED INSTRUCTIONS /////////////////////////////////////////

/*
 * Disable Interrupts
*/
void di() {
    IME = false;
}

/*
 * Enable Interrupts
*/
void ei() {
    IME = true;
}

/*
 * Halt
 */
//TODO needs to correctly implement halt
void halt() {
    if (IME) {
        CPU_STATE_temp = HALTED;
    }
}

///////////////////////////////////////// MISC. INSTRUCTIONS /////////////////////////////////////////

void daa() {
    uint8_t accumulator = read_8bit_reg(A);
    uint8_t flags = read_8bit_reg(F);
    uint8_t new_flags = SUBTRACTION_FLAG(flags) | CARRY_FLAG(flags);
    uint8_t adjustment = 0;

    if (SUBTRACTION_FLAG(flags)) {
        if (HALF_CARRY_FLAG(flags)) {
            adjustment -= 0x06;
        }
        if (CARRY_FLAG(flags)) {
            adjustment -= 0x60;
        }
    }
    else {
        if (HALF_CARRY_FLAG(flags) || ((accumulator & 0x0F) > 0x09)) {
            adjustment += 0x06;
        }
        if (CARRY_FLAG(flags) || (accumulator > 0x99)) {
            adjustment += 0x60;
            new_flags |= CARRY_BIT;
        }
    }
    accumulator += adjustment;
    new_flags |= get_zero_flag(accumulator);
    write_8bit_reg(A, accumulator);
    write_8bit_reg(F, new_flags);
}

/*
 * No Operation
 * Do nothing
 */
void nop([[maybe_unused]] uint8_t opcode) {
    REGS[A] += 0;
}

void stop() {
    CPU_STATE_temp = HALTED;
}