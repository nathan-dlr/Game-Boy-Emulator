#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "gb.h"
#include "cpu.h"
#include "decode.h"
#include "queue.h"
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
#define FIRST_NIBBLE(x) (x & 0x000F)

/* Interrupt Enable and Interrupt Flag */
#define IE 0xFFFF
#define IF 0xFF0F

static FILE* log;

void cpu_init() {
    CPU = malloc(sizeof(CPU_STRUCT));
    INSTR_QUEUE = malloc(sizeof(func_queue));
    queue_init(INSTR_QUEUE);
    write_16bit_reg(AF, 0x01B0);
    write_16bit_reg(BC, 0x0013);
    write_16bit_reg(DE, 0x00D8);
    write_16bit_reg(HL, 0x014D);
    write_16bit_reg(SP, 0xFFFE);
    write_16bit_reg(PC, 0x0100);
    CPU->STATE = RUNNING;
    CPU->IME = false;
    log =  fopen("logfile", "w");
}

/*
 * Executes the next instruction by checking to see
 * if the INSTR_QUEUE is empty, if so it decodes another
 * instruction, if not it executes the next instruction
 * in the queue
 */
void execute_next_cycle() {
    if (is_empty(INSTR_QUEUE)) {
        fprintf(log, "A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X PC:%04X PCMEM:%02X,%02X,%02X,%02X\n",
                CPU->REGS[A],
                CPU->REGS[F],
                CPU->REGS[B],
                CPU->REGS[C],
                CPU->REGS[D],
                CPU->REGS[E],
                CPU->REGS[H],
                CPU->REGS[L],
                read_16bit_reg(SP),
                read_16bit_reg(PC),
                MEMORY[read_16bit_reg(PC)],
                MEMORY[read_16bit_reg(PC) + 1],
                MEMORY[read_16bit_reg(PC) + 2],
                MEMORY[read_16bit_reg(PC) + 3]);
        fseek(log, 0, SEEK_END);

        if (ftell(log) >= 0x10000000) {
            fclose(log);
            free_resources();
            exit(0);
        }
        if (CPU->REGS[A] == 0x31 && CPU->REGS[F] == 0x60 && read_16bit_reg(PC) == 0xc061 && read_16bit_reg(SP) == 0xdfe9) {
            CPU->REGS[A] += 0;
        }
        read_next_byte();
        decode();
    }
    else {
        const func_and_param_wrapper* next_func = queue_pop(INSTR_QUEUE);
        next_func->func(next_func->parameter);
    }
}

///////////////////////////////////////// GENERAL HELPER FUNCTIONS /////////////////////////////////////////

/*
 * Reads and returns the 16bit register indicated by REG_PAIR
 */
uint16_t read_16bit_reg(uint8_t reg_pair) {
    return (CPU->REGS[reg_pair * 2] << 8) | CPU->REGS[(reg_pair * 2) + 1];
}

/*
 * Writes VALUE to 16bit register indicated by REG_PAIR
 */
void write_16bit_reg(uint8_t reg_pair, uint16_t value) {
    CPU->REGS[reg_pair * 2] = (uint8_t) (value >> 8);
    CPU->REGS[(reg_pair * 2) + 1] = (uint8_t) value;
}

/*
 * Puts PC onto address bus and reads that byte onto the data bus
 */
void read_next_byte() {
    CPU->ADDRESS_BUS = read_16bit_reg(PC);
    read_memory(UNUSED_VAL);
    write_16bit_reg(PC, read_16bit_reg(PC) + 1);
}

///////////////////////////////////////// FLAG SETTERS /////////////////////////////////////////

/*
 * Sets zero flag if RESULT is zero, otherwise clears zero flag
*/
static uint8_t get_zero_flag(uint8_t result) {
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
        flags |= get_zero_flag(result);
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
//LD HL,SP+e8 - 3 cycles: decode -> read_next_byte -> add to low byte of sp and store in L -> add any carry from low byte add_A_8bit and store in H
//LD SP,HL - 2 cycles : decode -> ld_sp_hl

//ADD A,r8 - 1 cycle: decode/add_A_8bit
//ADD A,[HL] - 2 cycles: decode/set address bus -> read byte -> add_A_8bit
//ADD A,n8 - 2 cycles: decode -> read_next_byte/add_A_8bit
//ADD HL,r16 - 2 cycles: decode -> add first bit -> add_A_8bit second bit
//ADD HL,SP - 2 cycles: decode -> add first bit -> add_A_8bit second bit
//ADD SP,e8 -

/*
 * Loads byte at [PC] into 8bit register indicated by DEST
 */
void ld_r8_imm8(uint8_t dest) {
    read_next_byte();
    CPU->REGS[dest] = CPU->DATA_BUS;
}

/*
 * Loads [PC] into temporary register W
 * Puts 8bit register A onto data but if indicated by LOAD_A
 */
void ld_rW_imm8(uint8_t load_a) {
    read_next_byte();
    CPU->REGS[W] = CPU->DATA_BUS;
    CPU->ADDRESS_BUS = read_16bit_reg(WZ);
    if (load_a) {
        CPU->DATA_BUS = CPU->REGS[A];
    }
}

/*
 * Loads [ADDR_BUS] into 8bit register indicated by DEST
 */
void ld_r8_addr_bus(uint8_t dest) {
    read_memory(UNUSED_VAL);
    CPU->REGS[dest] = CPU->DATA_BUS;
}

/*
 * Loads data from CPU->DATA_BUS into 8bit reg indicated by DEST
 */
void ld_r8_data_bus(uint8_t dest) {
    CPU->REGS[dest] = CPU->DATA_BUS;
}

/*
 * Used for LDH with an immediate value
 * Adds the immediate value to 0xFF00 and puts it on the ADDR_BUS
 */
void ldh_imm8(uint8_t UNUSED) {
    (void)UNUSED;
    read_next_byte();
    CPU->ADDRESS_BUS = 0xFF00 + CPU->DATA_BUS;
    CPU->DATA_BUS = CPU->REGS[A];
}

/*
 * Loads a byte indicated by the address at WZ into either SP0 or SP1
 * Called twice
 */
void ld_imm16_sp(uint8_t byte_num) {
    CPU->ADDRESS_BUS = read_16bit_reg(WZ);
    CPU->DATA_BUS = byte_num == 0 ? CPU->REGS[SP0] : CPU->REGS[SP1];
    write_memory(UNUSED_VAL);
    write_16bit_reg(WZ, read_16bit_reg(WZ) + 1);
}

/*
 * Loads the value of sp plus an offset into HL and set flags
 */
void ld_hl_sp8(uint8_t cycle) {
    uint16_t sp;
    switch (cycle) {
        case 2:
            read_next_byte();
            return;
        case 3:
            sp = read_16bit_reg(SP);
            write_16bit_reg(HL, sp + CPU->DATA_BUS);
            CPU->REGS[F] = get_add_flags_byte(CPU->REGS[HL], sp);
            return;
        default:
            perror("Invalid cycle number in ld_hl_sp8");
    }
}

/*
 * Loads HL into SP
 */
void ld_sp_hl(uint8_t UNUSED) {
    (void)UNUSED;
    write_16bit_reg(SP, read_16bit_reg(HL));
}

/*
 * Load an immediate value to memory location pointed to by hl
 */
void ld_hl_imm8(uint8_t cycle) {
    switch (cycle) {
        case 2:
            read_next_byte();
            CPU->ADDRESS_BUS = HL;
            break;
        case 3:
            write_memory(UNUSED_VAL);
            break;
        default:
            perror("Invalid cycle number for ld_hl_imm8");
    }
}

///////////////////////////////////////// ARITHMETIC INSTRUCTIONS  /////////////////////////////////////////

/*
 * Adds value in DATA_BUS to accumulator and sets flags
 * If operand type is an immediate value, it reads the next byte from PC before adding
 */
void add_A_8bit(uint8_t is_imm) {
    if (is_imm) {
        read_next_byte();
        return;
    }
    uint8_t source_val = CPU->DATA_BUS;
    uint8_t accumulator = CPU->REGS[A];
    uint8_t result = source_val + accumulator;
    uint8_t flags = get_add_flags_byte(result, accumulator) | get_zero_flag(result);

    CPU->REGS[A] = result;
    CPU->REGS[F] = flags;
}

/*
 * Adds
 */
void add_HL_16bit(uint8_t source) {
    uint8_t dest;
    uint8_t source_val;
    if (source % 2 == 0) {
        dest = CPU->REGS[H];
        source_val = CPU->REGS[source] + (CARRY_FLAG(CPU->REGS[F]) >> 4);
    }
    else {
        dest = CPU->REGS[L];
        source_val = CPU->REGS[source];
    }
    uint8_t result = source_val + dest;
    uint8_t flags = ZERO_FLAG(CPU->REGS[F]) | get_add_flags_byte(result, dest);

    CPU->REGS[F] = flags;
    if (source % 2 == 0) {
        CPU->REGS[H] = result;
    }
    else {
        CPU->REGS[L] = result;
    }
}

void add_sp_e8(uint8_t cycle) {
    uint8_t result;
    switch (cycle) {
        case 2:
            read_memory(UNUSED_VAL);
            CPU->REGS[Z] = CPU->DATA_BUS;
            return;
        case 3:
            result = Z + CPU->REGS[SP0];
            CPU->REGS[L] = result;
            CPU->REGS[F] = get_add_flags_byte(result, CPU->REGS[SP0]);
            return;
        case 4:
            result = CPU->REGS[SP1] + (CARRY_FLAG(CPU->REGS[F]) >> 4);
            result += (CPU->REGS[Z] | 0x80) ? 0xFF : 0x00;
            CPU->REGS[H] = result;
            return;
        default:
            perror("Invalid input in ADD HL SP+e8");
            return;
    }
}
/*
 * Add-Carry Instruction - adds operand, carry bit, and accumulator
 * Result stored in Accumulator Register
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 * Zero, carry, and half-carry flags are set
 */
void adc(uint8_t is_imm) {
    if (is_imm) {
        read_next_byte();
        return;
    }
    uint8_t carry_bit = CARRY_FLAG(CPU->REGS[F]) ? 0x01 : 0x00;
    uint8_t source_val = CPU->DATA_BUS;
    uint8_t accumulator = CPU->REGS[A];

    uint8_t intermediate = accumulator + carry_bit;
    uint8_t result = source_val + intermediate;
    uint8_t flags = get_add_flags_byte(intermediate, accumulator);
    flags |= (get_add_flags_byte(result, intermediate) | get_zero_flag(result));

    CPU->REGS[F] = flags;
    CPU->REGS[A] = result;
}

/*
 * Compare Instruction - Compares the value in A with the value in operand
 * Results are stored in Flag Register - zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void cp(uint8_t is_imm) {
    if (is_imm) {
        read_next_byte();
        return;
    }
    uint8_t source_val = CPU->DATA_BUS;
    uint8_t accumulator = CPU->REGS[A];
    uint16_t result = accumulator - source_val;
    uint8_t flags = get_subtraction_flags(accumulator, source_val) | get_zero_flag(result);

    CPU->REGS[F] = flags;
}

/*
 * Subtract Instruction - Subtracts the value in A with the value in operand
 * Results are stored in Accumulator Register
 * Zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void sub(uint8_t is_imm) {
    if (is_imm) {
        read_next_byte();
        return;
    }
    uint8_t source_val = CPU->DATA_BUS;
    uint8_t accumulator = CPU->REGS[A];
    uint8_t result = accumulator - source_val;
    uint8_t flags = get_subtraction_flags(accumulator, source_val) | get_zero_flag(result);

    CPU->REGS[F] = flags;
    CPU->REGS[A] = result;
}

/*
 * Subtract Carry Instruction - Subtracts the value in A with the value in operand and the carry bit
 * Results are stored in Accumulator Register
 * Zero, negative, carry, and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
 */
void sbc(uint8_t is_imm) {
    if (is_imm) {
        read_next_byte();
        return;
    }
    uint8_t source_val = CPU->DATA_BUS;
    uint8_t accumulator = CPU->REGS[A];
    uint8_t carry_bit = CARRY_FLAG(CPU->REGS[F]) ? 0x01 : 0x00;

    uint8_t intermediate = accumulator - source_val;
    uint8_t result = intermediate - carry_bit;
    uint8_t flags = get_subtraction_flags(accumulator, source_val);
    flags |= get_subtraction_flags(intermediate, carry_bit) | get_zero_flag(result);

    CPU->REGS[F] = flags;
    CPU->REGS[A] = result;
}

void inc_8bit(uint8_t reg_index) {
    uint8_t reg = get_reg_dt(reg_index);
    uint8_t source = reg_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    uint8_t result = source + 1;
    uint8_t flags = CARRY_FLAG(CPU->REGS[F]);

    flags |= get_zero_flag(result);
    if (FIRST_NIBBLE(source) == 0x0F) {
        flags |= HALF_CARRY_BIT;
    }
    if (reg_index != 6) {
        CPU->REGS[reg] = result;
    }
    else {
        CPU->DATA_BUS = result;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = flags;
}

void inc_16bit(uint8_t dest) {
    write_16bit_reg(dest, read_16bit_reg(dest) + 1);
}

void dec_16bit(uint8_t dest) {
    write_16bit_reg(dest, read_16bit_reg(dest) - 1);
}

void dec_8bit(uint8_t reg_index) {
    uint8_t reg = get_reg_dt(reg_index);
    uint8_t source = reg_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    uint8_t result = source - 1;
    uint8_t flags = CARRY_FLAG(CPU->REGS[F]) | NEGATIVE_BIT;

    if ((source & 0x000F) == 0x00) {
        flags |= HALF_CARRY_BIT;
    }
    flags |= get_zero_flag(result);
    if (reg_index != 6) {
        CPU->REGS[reg] = result;
    }
    else {
        CPU->DATA_BUS = result;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = flags;
}

///////////////////////////////////////// LOGIC INSTRUCTIONS /////////////////////////////////////////

/*
 * Bitwise AND instruction
 * Results are stored in Accumulator Register
 * Zero and half-carry flags are set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
*/
void and(uint8_t is_imm) {
    if (is_imm) {
        read_next_byte();
        return;
    }
    uint8_t source_val = CPU->DATA_BUS;
    uint8_t result = CPU->REGS[A] & source_val;

    CPU->REGS[A] = result;
    CPU->REGS[F] = get_zero_flag(result) | HALF_CARRY_BIT;
}

/*
 * Bitwise OR instruction
 * Results are stored in Accumulator Register
 * Zero flag is set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
*/
void or(uint8_t is_imm) {
    if (is_imm) {
        read_next_byte();
        return;
    }
    uint8_t source_val = CPU->DATA_BUS;
    uint8_t result = CPU->REGS[A] | source_val;

    CPU->REGS[A] = result;
    CPU->REGS[F] = get_zero_flag(result);
}

/*
 * Bitwise XOR instruction
 * Results are stored in Accumulator Register
 * Zero flag is set
 * Takes in either 8bit reg, 8bit const, or register pointer (HL)
*/
void xor(uint8_t is_imm) {
    if (is_imm) {
        read_next_byte();
        return;
    }
    uint8_t source_val = CPU->DATA_BUS;
    uint8_t result = CPU->REGS[A] ^ source_val;

   CPU->REGS[A] = result;
   CPU->REGS[F] = get_zero_flag(result);
}

/*
 * Complement accumulator instruction
 * Replaces value in accumulator with its complement
 */
void cpl() {
    CPU->REGS[A] = ~CPU->REGS[A];
    CPU->REGS[F] = SET_BIT(NEGATIVE_BIT | HALF_CARRY_BIT, CPU->REGS[F]);
}

///////////////////////////////////////// BIT FLAG INSTRUCTIONS /////////////////////////////////////////

/*
 * Bit instruction
 * Tests if BIT in SOURCE_REG is set, sets the zero flag if not set
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void bit(uint8_t bit_num) {
    uint8_t source_val = CPU->DATA_BUS;
    bool set = source_val & (0x0001 << bit_num);

    uint8_t flags = CARRY_FLAG(CPU->REGS[F]) | HALF_CARRY_BIT;
    if (!set) {
        flags |= ZERO_BIT;
    }
    CPU->REGS[F] = flags;
}

/*
 * Reset (clear) bit_num instruction
 * Clears BIT in SOURCE_REG
 * Source reg is either 8-bit_num reg or byte pointed to by HL, indicated by REG_8BIT
 */
void res(uint8_t opcode) {
    uint8_t source_reg = get_reg_dt(opcode & 0x07);
    uint8_t bit_num = (opcode & 0x38) >> 3;
    uint8_t bit = 0x01 << bit_num;
    if (source_reg != HL) {
        CPU->REGS[source_reg] = CLEAR_BIT(bit, CPU->REGS[source_reg]);
    }
    //RES u3,[HL] - 4 cycles: decode -> read [hl] -> set bit in Z reg -> write memory
    else {
        CPU->REGS[Z] = CLEAR_BIT(bit, CPU->DATA_BUS);
        CPU->DATA_BUS = CPU->REGS[Z];
    }
}

/*
 * Set bit instruction
 * Source reg is either 8-bit reg or byte pointed to by HL
 * Sets BIT in SOURCE_REG, indicated by REG_8BIT
 */
void set(uint8_t opcode) {
    uint8_t source_reg = get_reg_dt(opcode & 0x07);
    uint8_t bit_num = (opcode & 0x38) >> 3;
    uint8_t bit = 0x01 << bit_num;
    if (source_reg != HL) {
        CPU->REGS[source_reg] = SET_BIT(bit, CPU->REGS[source_reg]);
    }
    else {
        CPU->REGS[Z] = SET_BIT(bit, CPU->DATA_BUS);
        CPU->DATA_BUS = CPU->REGS[Z];
    }
}

///////////////////////////////////////// BIT SHIFT INSTRUCTIONS /////////////////////////////////////////

/*
 * Rotate left instruction
 * Rotates bits in SOURCE_REG left, through the carry flag
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rl(uint8_t reg_index) {
    uint8_t reg = get_reg_dt(reg_index);
    uint8_t source_val = reg_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    uint8_t carry_flag_old = CARRY_FLAG(CPU->REGS[F]) ? 0x01 : 0x00;
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = (source_val << 1) | carry_flag_old;

    if (reg_index != 6) {
        CPU->REGS[reg] = new_val;
    }
    else {
        CPU->DATA_BUS = new_val;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = get_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Rotate left accumulator instruction
 * Rotates bits in accumulator left, through the carry flag
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rla() {
    uint8_t carry_flag_old = CARRY_FLAG(CPU->REGS[F]) ? 0x01 : 0x00;
    uint8_t source_val = CPU->REGS[A];
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = (source_val << 1) | carry_flag_old;

    CPU->REGS[A] = new_val;
    CPU->REGS[F] = get_rot_flags(new_val, false, carry_flag_new);
}

/*
 * Rotate left circular instruction
 * Rotate bits in SOURCE_REG left circularly, from MSB to LSB
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rlc(uint8_t reg_index) {
    uint8_t reg = get_reg_dt(reg_index);
    uint8_t source_val = reg_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = source_val << 1;
    new_val |= carry_flag_new ? 0x01 : 0x00;

    if (reg_index != 6) {
        CPU->REGS[reg] = new_val;
    }
    else {
        CPU->DATA_BUS = new_val;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = get_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Rotate left circular accumulator instruction
 * Rotate bits in accumulator left circularly, from MSB to LSB
 */
void rlca() {
    uint8_t source_val = CPU->REGS[A];
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = source_val << 1;
    new_val |= carry_flag_new ? 0x01 : 0x00;

    CPU->REGS[A] = new_val;
    CPU->REGS[F] = get_rot_flags(new_val, false, carry_flag_new);
}

/*
 * Rotate right instruction
 * Rotate bits in SOURCE_REG right, through the carry flag
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rr(uint8_t reg_index) {
    uint8_t reg = get_reg_dt(reg_index);
    uint8_t source_val = reg_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    uint8_t carry_flag_old = CARRY_FLAG(CPU->REGS[F]) ? 0x01 : 0x00;
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_val = (source_val >> 1);
    new_val |= carry_flag_old << 7;

    if (reg_index != 6) {
        CPU->REGS[reg] = new_val;
    }
    else {
        CPU->DATA_BUS = new_val;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = get_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Rotate right accumulator instruction
 * Rotate bits in SOURCE_REG right, through the carry flag
 * SOURCE reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rra() {
    uint8_t source_val = CPU->REGS[A];
    uint8_t carry_flag_old = CARRY_FLAG(CPU->REGS[F]) ? 0x01 : 0x00;
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_val = (source_val >> 1);
    new_val |= carry_flag_old << 7;

    CPU->REGS[A] = new_val;
    CPU->REGS[F] = get_rot_flags(new_val, false, carry_flag_new);
}

/*
 * Rotate right circular instruction
 * Rotate bits in SOURCE_REG right, from LSB to MSB
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rrc(uint8_t reg_index) {
    uint8_t reg = get_reg_dt(reg_index);
    uint8_t source_val = reg_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_msb = source_val & 0x0001;
    uint8_t new_val = (source_val >> 1);
    new_val |= new_msb << 7;

    if (reg_index != 6) {
        CPU->REGS[reg] = new_val;
    }
    else {
        CPU->DATA_BUS = new_val;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = get_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Rotate right circular accumulator instruction
 * Rotate bits in accumulator right, from LSB to MSB
 */
void rrca() {
    uint8_t source_val = CPU->REGS[A];
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_msb = source_val & 0x01;
    uint8_t new_val = (source_val >> 1);
    new_val |= new_msb << 7;

    CPU->REGS[A] = new_val;
    CPU->REGS[F] = get_rot_flags(new_val, false, carry_flag_new);
}

/*
 * Shift Left Arithmetically
 * Shift bits in SOURCE_REG left arithmetically
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void sla(uint8_t reg_index) {
    uint8_t reg = get_reg_dt(reg_index);
    uint8_t source_val = reg_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = source_val << 1;

    if (reg_index != 6) {
        CPU->REGS[reg] = new_val;
    }
    else {
        CPU->DATA_BUS = new_val;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = get_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Shift Right Arithmetically
 * Shift bits in SOURCE_REG right arithmetically
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void sra(uint8_t reg_index) {
    uint8_t reg = get_reg_dt(reg_index);
    uint8_t source_val = reg_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_msb = source_val & 0x80;
    uint8_t new_val = source_val >> 1;
    new_val |= new_msb;

    if (reg_index != 6) {
        CPU->REGS[reg] = new_val;
    }
    else {
        CPU->DATA_BUS = new_val;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = get_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Shit Right Logically
 * Shift bits in SOURCE_REG right logically
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void srl(uint8_t reg_index) {
    uint8_t reg = get_reg_dt(reg_index);
    uint8_t source_val = reg_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_val = source_val >> 1;

    if (reg_index != 6) {
        CPU->REGS[reg] = new_val;
    }
    else {
        CPU->DATA_BUS = new_val;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = get_rot_flags(new_val, true, carry_flag_new);
}

/*
 * Swap Instruction
 * Swap the upper 4 bits and the lower 4 bits in SOURCE_REG
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
*/
void swap(uint8_t reg_index) {
    uint8_t reg = get_reg_dt(reg_index);
    uint8_t source_val = reg_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    uint8_t new_val = ((source_val & 0x0F) << 4) | ((source_val & 0xF0) >> 4);

    if (reg_index != 6) {
        CPU->REGS[reg] = new_val;
    }
    else {
        CPU->DATA_BUS = new_val;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = get_zero_flag(new_val);
}

///////////////////////////////////////// JUMPS AND SUBROUTINE INSTRUCTIONS /////////////////////////////////////////

/*
 * Helper function for jumps and subroutines that evaluates if given condition codes match with those in flag register
 * Returns true if condition codes match, false otherwise
 */
static bool evaluate_condition_codes(uint8_t cc) {
    uint8_t flags = CPU->REGS[F];
    switch (cc) {
        case NOT_ZERO:
            return (flags | ~ZERO_BIT) == ~ZERO_BIT;
        case ZERO:
            return (flags & ZERO_BIT) == ZERO_BIT;
        case NOT_CARRY:
            return (flags | ~CARRY_BIT) == ~CARRY_BIT;
        case CARRY:
            return (flags & CARRY_BIT) == CARRY_BIT;
        default:
            return true;
    }
}

/*
 * Call Instruction
 * Pushes the address of the instruction after the call on the stack, such that RET can queue_pop it later;
 * then it executes an implicit JP using the address pointed to by ADDRESS
 * Only executes if CC is met
 */
void call_cycle3(uint8_t cc) {
    read_next_byte();
    CPU->REGS[W] = CPU->DATA_BUS;
    if (!evaluate_condition_codes(cc)) {
        queue_pop(INSTR_QUEUE);
        queue_pop(INSTR_QUEUE);
        queue_pop(INSTR_QUEUE);
    }
}

void call_writes(uint8_t cycle_num) {
    if (cycle_num == 5) {
        CPU->ADDRESS_BUS = read_16bit_reg(SP);
        CPU->DATA_BUS = CPU->REGS[PC1];
        write_memory(UNUSED_VAL);
        write_16bit_reg(SP, read_16bit_reg(SP) - 1);
    }
    else {
        CPU->ADDRESS_BUS = read_16bit_reg(SP);
        CPU->DATA_BUS = CPU->REGS[PC0];
        write_memory(UNUSED_VAL);
        write_16bit_reg(PC, read_16bit_reg(WZ));
    }
}

/*
 * Jump Instruction
 * Jumps to address pointed to by either the next word or by the contents of HL, indicated by IS_HL, if CC is met
 */
void jp_cycle3(uint8_t cc) {
    read_next_byte();
    CPU->REGS[W] = CPU->DATA_BUS;
    if (!evaluate_condition_codes(cc)) {
        queue_pop(INSTR_QUEUE);
    }
}

void jp(uint8_t is_hl) {
    if (is_hl) {
        write_16bit_reg(PC, read_16bit_reg(HL));
    }
    else {
        write_16bit_reg(PC, read_16bit_reg(WZ));
    }
}

/*
 * Relative Jump Instruction
 * Jumps to some relative offset if CC is met
 */
 //JR e - 3 cycles -> read memory -> add offset
void jr_cycle2(uint8_t cc) {
    read_next_byte();
    CPU->REGS[Z] = CPU->DATA_BUS;
    if (!evaluate_condition_codes(cc)) {
        queue_pop(INSTR_QUEUE);
    }
    else {
        CPU->DATA_BUS = CPU->REGS[Z];
    }
}
void jr(uint8_t UNUSED) {
    (void)UNUSED;
    write_16bit_reg(PC, read_16bit_reg(PC) + (int8_t)CPU->DATA_BUS);
}

/*
 * Return from subroutine
 * Pops the PC from the stack if cc is met
 */
//ret - 4 cycles -> decode -> read_next_byte -> read_next_byte -> pc = wz
//ret cc - 5 cycles -> decode -> eval_cc -> read -> read -> pc = wz
//reti - 4 cycles -> decode -> read_next_byte -> read next byte -> pz = wz & IME = = 1

void ret_eval_cc(uint8_t cc) {
    if (!evaluate_condition_codes(cc)) {
        queue_pop(INSTR_QUEUE);
        queue_pop(INSTR_QUEUE);
        queue_pop(INSTR_QUEUE);
    }
}


void ret(uint8_t cycle) {
    switch (cycle) {
        case 0:
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            read_memory(UNUSED_VAL);
            CPU->REGS[Z] = CPU->DATA_BUS;
            write_16bit_reg(SP, read_16bit_reg(SP) + 1);
            break;
        case 1:
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            read_memory(UNUSED_VAL);
            CPU->REGS[W] = CPU->DATA_BUS;
            write_16bit_reg(SP, read_16bit_reg(SP) + 1);
            break;
        case 2:
            write_16bit_reg(PC, read_16bit_reg(WZ));
            break;
        default:
            perror("Invalid cycle number passed into ret");
    }
}

void reti(uint8_t cycle) {
    switch (cycle) {
        case 2:
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            read_memory(UNUSED_VAL);
            CPU->REGS[Z] = CPU->DATA_BUS;
            write_16bit_reg(SP, read_16bit_reg(SP) + 1);
            break;
        case 3:
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            read_memory(UNUSED_VAL);
            CPU->REGS[W] = CPU->DATA_BUS;
            write_16bit_reg(SP, read_16bit_reg(SP) + 1);
            break;
        case 4:
            write_16bit_reg(PC, read_16bit_reg(WZ));
            CPU->IME = 1;
            break;
        default:
            perror("Invalid cycle number passed into reti");
    }
}

void rst(uint8_t cycle) {
    switch (cycle) {
        case 2:
            write_16bit_reg(SP, read_16bit_reg(SP) - 1);
            CPU->REGS[Z] = CPU->DATA_BUS;
            return;
        case 3:
            CPU->DATA_BUS = CPU->REGS[PC1];
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            write_memory(UNUSED_VAL);
            write_16bit_reg(SP, read_16bit_reg(SP) - 1);
            return;
        case 4:
            CPU->DATA_BUS = CPU->REGS[PC1];
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            write_memory(UNUSED_VAL);
            return;
        default:
            perror("Invalid cycle number passed into rst");
    }
}

///////////////////////////////////////// CARRY FLAG INSTRUCTIONS /////////////////////////////////////////

/*
 * Complement Carry Flag
 */
void ccf() {
    uint8_t flags = CPU->REGS[F];
    flags = (CARRY_FLAG(flags) ^ CARRY_BIT) | ZERO_FLAG(flags);
    CPU->REGS[F] = flags;
}

/*
 * Set Carry Flag
 */
void scf() {
    uint8_t flags = CARRY_BIT | ZERO_FLAG(CPU->REGS[F]);
    CPU->REGS[F] = flags;
}

///////////////////////////////////////// STACK MANIPULATION /////////////////////////////////////////

/*
 * Pop Instruction
 * Pop register whose index is indicated by REG_16 from the stack
 */

void pop_reads(uint8_t cycle) {
    switch (cycle) {
        case 2:
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            read_memory(UNUSED_VAL);
            CPU->REGS[Z] = CPU->DATA_BUS;
            write_16bit_reg(SP, read_16bit_reg(SP) + 1);
            break;
        case 3:
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            read_memory(UNUSED_VAL);
            CPU->REGS[W] = CPU->DATA_BUS;
            write_16bit_reg(SP, read_16bit_reg(SP) + 1);
            break;
        default:
            perror("Invalid cycle number passed into pop_reads");
    }
}

void pop_load(uint8_t reg_16) {
    write_16bit_reg(reg_16, read_16bit_reg(WZ));
    if (reg_16 == AF) {
        CPU->REGS[F] &= 0xFFF0;
    }
}

/*
 * Push Instruction
 * Push register whose index is indicated by REG_16 from the stack
 */
void push(uint8_t cycle) {
    switch (cycle) {
        case 2:
            write_16bit_reg(SP, read_16bit_reg(SP) - 1);
            return;
        case 3:
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            CPU->DATA_BUS = CPU->REGS[W];
            write_memory(UNUSED_VAL);
            write_16bit_reg(SP, read_16bit_reg(SP) - 1);
            return;
        case 4:
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            CPU->DATA_BUS = CPU->REGS[Z];
            write_memory(UNUSED_VAL);
            return;
        default:
            perror("Invalid cycle number passed into push instruction");
            return;
    }
}
///////////////////////////////////////// INTERRUPT-RELATED INSTRUCTIONS /////////////////////////////////////////

/*
 * Disable Interrupts
*/
void di() {
    CPU->IME = false;
}

/*
 * Enable Interrupts
*/
void ei() {
    CPU->IME = true;
}

/*
 * Halt
 */
//TODO needs to correctly implement halt
void halt() {
    if (CPU->IME) {
        CPU->STATE = HALTED;
    }
}

///////////////////////////////////////// MISC. INSTRUCTIONS /////////////////////////////////////////

void daa() {
    uint8_t accumulator = CPU->REGS[A];
    uint8_t flags = CPU->REGS[F];
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
    CPU->REGS[A] = accumulator;
    CPU->REGS[F] = new_flags;
}

/*
 * No Operation
 * Do nothing
 */
void nop([[maybe_unused]] uint8_t opcode) {
    CPU->REGS[A] += 0;
}

void stop() {
    CPU->STATE = HALTED;
}