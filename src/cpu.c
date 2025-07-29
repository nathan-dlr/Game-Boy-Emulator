#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <gb.h>
#include <cpu.h>
#include <decode.h>
#include <queue.h>
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
#define IS_BYTE_IN_MEM(dt_index) (dt_index == 6)
#define VBLANK_BIT 0x01
#define LCD_BIT 0x02
#define TIMER_BIT 0x04
#define SERIAL_BIT 0x08
#define JOYPAD_BIT 0x10
#define VLANK_VEC 0x40
#define STAT_VEC 0x48
#define TIMER_VEC 0x50
#define SERIAL_VEC 0x58
#define JOYPAD_VEC 0x60

static bool check_interrupts();

void cpu_init() {
    CPU = malloc(sizeof(CPU_STRUCT));
    INSTR_QUEUE = malloc(sizeof(func_queue));
    write_16bit_reg(AF, 0x01B0);
    write_16bit_reg(BC, 0x0013);
    write_16bit_reg(DE, 0x00D8);
    write_16bit_reg(HL, 0x014D);
    write_16bit_reg(SP, 0xFFFE);
    write_16bit_reg(PC, 0x0100);
    CPU->STATE = RUNNING;
    CPU->IME = false;
    CPU->DMA_CYCLE = 0;
    CYCLE_COUNT = 0;
}

/*
 * Executes the next instruction by checking to see
 * if the INSTR_QUEUE is empty, if so it decodes another
 * instruction, if not it executes the next instruction
 * in the queue
 */
void execute_next_cycle() {
    if (is_empty(INSTR_QUEUE)) {
        if (!check_interrupts() && CPU->STATE == RUNNING) {
            read_next_byte();
            decode();
        }
    }
    else {
        const func_and_param_wrapper* next_func = instr_queue_pop();
        next_func->func(next_func->parameter);
    }
    if (CPU->STATE == OAM_DMA_TRANSFER) {
        OAM_DMA();
    }
    CYCLE_COUNT++;
}

/*
 * Checks to see if interrupt needs to be serviced, if so, queues up appropriate functions
 */
static bool check_interrupts() {
    uint8_t interrupt_flag = MEMORY[IF];
    uint8_t interrupt_enable = MEMORY[IE];
    uint8_t interrupt_requested = interrupt_flag & interrupt_enable;
    if (!interrupt_requested) {
        return false;
    }
    else if (!CPU->IME && interrupt_requested) {
        CPU->STATE = RUNNING;
        return false;
    }
    CPU->STATE = RUNNING;
    bool vblank = interrupt_requested & VBLANK_BIT;
    bool lcd = interrupt_requested & LCD_BIT;
    bool timer = interrupt_requested & TIMER_BIT;
    bool serial = interrupt_requested & SERIAL_BIT;
    bool joypad = interrupt_requested & JOYPAD_BIT;
    if (vblank) {
        CPU->DATA_BUS = VLANK_VEC;
        MEMORY[IF] = CLEAR_BIT(VBLANK_BIT, interrupt_flag);
    }
    else if (lcd) {
        CPU->DATA_BUS = STAT_VEC;
        MEMORY[IF] = CLEAR_BIT(STAT_VEC, interrupt_flag);
    }
    else if (timer) {
        CPU->DATA_BUS = TIMER_VEC;
        MEMORY[IF] = CLEAR_BIT(TIMER_BIT, interrupt_flag);
    }
    else if (serial) {
        CPU->DATA_BUS = SERIAL_VEC;
        MEMORY[IF] = CLEAR_BIT(SERIAL_BIT, interrupt_flag);
    }
    else if (joypad) {
        CPU->DATA_BUS = JOYPAD_VEC;
        MEMORY[IF] = CLEAR_BIT(JOYPAD_BIT, interrupt_flag);
    }
    instr_queue_push(nop, UNUSED_VAL);
    instr_queue_push(rst, 2);
    instr_queue_push(rst, 3);
    instr_queue_push(rst, 4);
    return true;
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
    int8_t result;
    switch (cycle) {
        case 2:
            read_next_byte();
            CPU->REGS[Z] = CPU->DATA_BUS;
            return;
        case 3:
            result = (int8_t) CPU->REGS[Z] + CPU->REGS[SP0];
            CPU->REGS[L] = (uint8_t)result;
            CPU->REGS[F] = get_add_flags_byte((uint8_t)result, CPU->DATA_BUS);

            result = CPU->REGS[SP1] + (CARRY_FLAG(CPU->REGS[F]) >> 4);
            result += (CPU->REGS[Z] & 0x80) ? 0xFF : 0x00;
            CPU->REGS[H] = (uint8_t)result;
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
            CPU->ADDRESS_BUS = read_16bit_reg(HL);
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
 * If operand type is an immediate value, indicated by IS_IMM, it reads the next byte from PC before adding
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
 * Adds a 16bit register indicated by SOURCE to HL
 */
void add_HL_16bit(uint8_t source) {
    uint8_t dest;
    uint8_t source_val;
    uint8_t flags;
    uint8_t result;
    if (source % 2 == 0) {
        dest = CPU->REGS[H];
        source_val = CPU->REGS[source] + (CARRY_FLAG(CPU->REGS[F]) >> 4);
        result = source_val + dest;
        flags = ZERO_FLAG(CPU->REGS[F]);
        flags |= get_add_flags_byte(source_val, CPU->REGS[source]);
        flags |= get_add_flags_byte(result, dest);
        CPU->REGS[H] = result;
    }
    else {
        dest = CPU->REGS[L];
        source_val = CPU->REGS[source];
        result = source_val + dest;
        flags = ZERO_FLAG(CPU->REGS[F]) | get_add_flags_byte(result, dest);
        CPU->REGS[L] = result;
    }

    CPU->REGS[F] = flags;
}

/*
 * Adds a signed offset to SP
 * Takes in the cycle number to perform the correct part of the instruction
 */
void add_sp_e8(uint8_t cycle) {
    int8_t result;
    switch (cycle) {
        case 2:
            read_next_byte();
            CPU->REGS[Z] = CPU->DATA_BUS;
            return;
        case 3:
            result = (int8_t) CPU->REGS[Z] + CPU->REGS[SP0];
            CPU->REGS[SP0] = (uint8_t)result;
            CPU->REGS[F] = get_add_flags_byte((uint8_t)result, CPU->REGS[Z]);
            return;
        case 4:
            result = CPU->REGS[SP1] + (CARRY_FLAG(CPU->REGS[F]) >> 4);
            result += (CPU->REGS[Z] & 0x80) ? 0xFF : 0x00;
            CPU->REGS[SP1] = result;
            return;
        default:
            perror("Invalid input in ADD HL SP+e8");
            return;
    }
}

/*
 * Add-Carry Instruction - adds operand, and carry bit to the accumulator
 * If operand type is an immediate value, indicated by IS_IMM, it reads the next byte from PC before adding
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
 * Compare Instruction - Compares the value in A with the value in the data bus
 * Results are stored in Flag Register - all flags are set
 * If operand type is an immediate value, indicated by IS_IMM, it reads the next byte from PC before adding
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
 * Subtract Instruction - Subtracts the value in A with the value in data bus
 * All flags are set
 * If operand type is an immediate value, indicated by IS_IMM, it reads the next byte from PC before adding
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
 * Subtract Carry Instruction - Subtracts the value in A with the value in data bus and the carry bit
 * All flags are set
 * If operand type is an immediate value, indicated by IS_IMM, it reads the next byte from PC before adding
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

/*
 * Increments an 8-bit register or a byte in memory
 * disassembly_table_index is used to find the source register
 * If the index is 6 then the source operand is a byte in memory
 */
void inc_8bit(uint8_t disassembly_table_index) {
    uint8_t reg = get_reg_dt(disassembly_table_index);
    bool byte_in_mem = IS_BYTE_IN_MEM(disassembly_table_index);
    uint8_t source = !byte_in_mem ? CPU->REGS[reg] : CPU->DATA_BUS;
    uint8_t result = source + 1;
    uint8_t flags = CARRY_FLAG(CPU->REGS[F]);

    flags |= get_zero_flag(result);
    if (FIRST_NIBBLE(source) == 0x0F) {
        flags |= HALF_CARRY_BIT;
    }
    if (!byte_in_mem) {
        CPU->REGS[reg] = result;
    }
    else {
        CPU->DATA_BUS = result;
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        write_memory(UNUSED_VAL);
    }
    CPU->REGS[F] = flags;
}

/*
 * Increments 16bit register indicated by DEST
 */
void inc_16bit(uint8_t dest) {
    write_16bit_reg(dest, read_16bit_reg(dest) + 1);
}

/*
 * Decrements 16bit register indicated by DEST
 */
void dec_16bit(uint8_t dest) {
    write_16bit_reg(dest, read_16bit_reg(dest) - 1);
}

/*
 * Decrements an 8-bit register or a byte in memory
 * disassembly_table_index is used to find the source register
 * If the index is 6 then the source operand is a byte in memory
 */
void dec_8bit(uint8_t disassembly_table_index) {
    uint8_t reg = get_reg_dt(disassembly_table_index);
    bool byte_in_mem = IS_BYTE_IN_MEM(disassembly_table_index);
    uint8_t source = !byte_in_mem ? CPU->REGS[reg] : CPU->DATA_BUS;
    uint8_t result = source - 1;
    uint8_t flags = CARRY_FLAG(CPU->REGS[F]) | NEGATIVE_BIT;

    if ((source & 0x000F) == 0x00) {
        flags |= HALF_CARRY_BIT;
    }
    flags |= get_zero_flag(result);
    if (!byte_in_mem) {
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
 * ANDs Accumulator Register
 * Zero and half-carry flags are set
 * If operand type is an immediate value, indicated by IS_IMM, it reads the next byte from PC before adding
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
 * ORs Accumulator Register
 * Zero flag is set
 * If operand type is an immediate value, indicated by IS_IMM, it reads the next byte from PC before adding
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
 * XORs Accumulator Register
 * Zero flag is set
 * If operand type is an immediate value, indicated by IS_IMM, it reads the next byte from PC before adding
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
 * Tests if BIT_NUM in data bus is set, sets the zero flag if not set
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
 * Reset (clear) bit instruction
 * Clears a bit in source reg, bit and source reg are found from the opcode
 * Source reg is either 8-bit_num reg or byte pointed to by HL, indicated by REG_8BIT
 */
void res(uint8_t opcode) {
    uint8_t disassembly_table_index = opcode & 0x07;
    uint8_t source_reg = get_reg_dt(disassembly_table_index);
    uint8_t bit_num = (opcode & 0x38) >> 3;
    uint8_t bit = 0x01 << bit_num;
    if (disassembly_table_index != 6) {
        CPU->REGS[source_reg] = CLEAR_BIT(bit, CPU->REGS[source_reg]);
    }
    else {
        CPU->REGS[Z] = CLEAR_BIT(bit, CPU->DATA_BUS);
        CPU->DATA_BUS = CPU->REGS[Z];
        write_memory(UNUSED_VAL);
    }
}

/*
 * Set bit instruction
 * Set a bit in source reg, bit and source reg are found from the opcode
 */
void set(uint8_t opcode) {
    uint8_t reg_index = opcode & 0x07;
    uint8_t source_reg = get_reg_dt(reg_index);
    uint8_t bit_num = (opcode & 0x38) >> 3;
    uint8_t bit = 0x01 << bit_num;
    if (reg_index != 6) {
        CPU->REGS[source_reg] = SET_BIT(bit, CPU->REGS[source_reg]);
    }
    else {
        CPU->REGS[Z] = SET_BIT(bit, CPU->DATA_BUS);
        CPU->DATA_BUS = CPU->REGS[Z];
        write_memory(UNUSED_VAL);
    }
}

///////////////////////////////////////// BIT SHIFT INSTRUCTIONS /////////////////////////////////////////

/*
 * Rotate left instruction
 * disassembly_table_index is used to find the source register
 * Rotates bits in source_reg left, through the carry flag
 * Source reg is either 8-bit reg or byte pointed to by HL, indicated by REG_8BIT
 */
void rl(uint8_t disassembly_table_index) {
    uint8_t reg = get_reg_dt(disassembly_table_index);
    uint8_t source_val = disassembly_table_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    uint8_t carry_flag_old = CARRY_FLAG(CPU->REGS[F]) ? 0x01 : 0x00;
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = (source_val << 1) | carry_flag_old;

    if (disassembly_table_index != 6) {
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
 * disassembly_table_index is used to find the source register
 * Rotate bits in source reg left circularly, from MSB to LSB
 */
void rlc(uint8_t disassembly_table_index) {
    uint8_t reg = get_reg_dt(disassembly_table_index);
    uint8_t source_val = disassembly_table_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = source_val << 1;
    new_val |= carry_flag_new ? 0x01 : 0x00;

    if (disassembly_table_index != 6) {
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
 * disassembly_table_index is used to find the source register
 * Rotate bits in source reg right, through the carry flag
 */
void rr(uint8_t disassembly_table_index) {
    uint8_t reg = get_reg_dt(disassembly_table_index);
    uint8_t source_val = disassembly_table_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    uint8_t carry_flag_old = CARRY_FLAG(CPU->REGS[F]) ? 0x01 : 0x00;
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_val = (source_val >> 1);
    new_val |= carry_flag_old << 7;

    if (disassembly_table_index != 6) {
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
 * disassembly_table_index is used to find the source register
 * Rotate bits in source reg right, from LSB to MSB
 */
void rrc(uint8_t disassembly_table_index) {
    uint8_t reg = get_reg_dt(disassembly_table_index);
    uint8_t source_val = disassembly_table_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_msb = source_val & 0x0001;
    uint8_t new_val = (source_val >> 1);
    new_val |= new_msb << 7;

    if (disassembly_table_index != 6) {
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
 * disassembly_table_index is used to find the source register
 * Shift bits in source reg left arithmetically
 */
void sla(uint8_t disassembly_table_index) {
    uint8_t reg = get_reg_dt(disassembly_table_index);
    uint8_t source_val = disassembly_table_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    bool carry_flag_new = source_val & 0x80 ? true : false;
    uint8_t new_val = source_val << 1;

    if (disassembly_table_index != 6) {
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
 * disassembly_table_index is used to find the source register
 * Shift bits in source reg right arithmetically
 */
void sra(uint8_t disassembly_table_index) {
    uint8_t reg = get_reg_dt(disassembly_table_index);
    uint8_t source_val = disassembly_table_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_msb = source_val & 0x80;
    uint8_t new_val = source_val >> 1;
    new_val |= new_msb;

    if (disassembly_table_index != 6) {
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
 * disassembly_table_index is used to find the source register
 * Shift bits in source reg right logically
 */
void srl(uint8_t disassembly_table_index) {
    uint8_t reg = get_reg_dt(disassembly_table_index);
    uint8_t source_val = disassembly_table_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    bool carry_flag_new = source_val & 0x01 ? true : false;
    uint8_t new_val = source_val >> 1;

    if (disassembly_table_index != 6) {
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
 * disassembly_table_index is used to find the source register
 * Swap the upper 4 bits and the lower 4 bits in sourece reg
*/
void swap(uint8_t disassembly_table_index) {
    uint8_t reg = get_reg_dt(disassembly_table_index);
    uint8_t source_val = disassembly_table_index != 6 ? CPU->REGS[reg] : CPU->DATA_BUS;
    uint8_t new_val = ((source_val & 0x0F) << 4) | ((source_val & 0xF0) >> 4);

    if (disassembly_table_index != 6) {
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
 * Call Instruction - cycle 3
 * Checks condition codes (CC)
 * If flags don't match then pop's the rest of the instruction's cycles from the instruction queue
 */
void call_cycle3(uint8_t cc) {
    read_next_byte();
    CPU->REGS[W] = CPU->DATA_BUS;
    if (!evaluate_condition_codes(cc)) {
        instr_queue_pop();
        instr_queue_pop();
        instr_queue_pop();
    }
}

/*
 * Call Instructions - cycle 5
 * Takes in CYCLE_NUM to control what work is done
 * Pushes PC onto stash
 */
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
 * Jump Instruction - Cycle 3
 * Checks condition codes (CC)
 * If flags don't match then pop's the rest of the instruction's cycles from the instruction queue
 */
void jp_cycle3(uint8_t cc) {
    read_next_byte();
    CPU->REGS[W] = CPU->DATA_BUS;
    if (!evaluate_condition_codes(cc)) {
        instr_queue_pop();
    }
}

/*
 * Jump Instruction
 * Writes data from either HL or WZ into PC
 */
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
 * Checks condition codes (CC)
 * If flags don't match then pop's the rest of the instruction's cycles from the instruction queue
 */
void jr_cycle2(uint8_t cc) {
    read_next_byte();
    CPU->REGS[Z] = CPU->DATA_BUS;
    if (!evaluate_condition_codes(cc)) {
        instr_queue_pop();
    }
    else {
        CPU->DATA_BUS = CPU->REGS[Z];
    }
}

/*
 * Relative Jump Instruction
 * Adds offset to PC
 */
void jr(uint8_t UNUSED) {
    (void)UNUSED;
    write_16bit_reg(PC, read_16bit_reg(PC) + (int8_t)CPU->DATA_BUS);
}

//ret - 4 cycles -> decode -> read_next_byte -> read_next_byte -> pc = wz
//ret cc - 5 cycles -> decode -> eval_cc -> read -> read -> pc = wz
//reti - 4 cycles -> decode -> read_next_byte -> read next byte -> pz = wz & IME = = 1

/*
 * Return from subroutine - evaluate condition codes
 * Checks condition codes (CC)
 * If flags don't match then pop's the rest of the instruction's cycles from the instruction queue
 */
void ret_eval_cc(uint8_t cc) {
    if (!evaluate_condition_codes(cc)) {
        instr_queue_pop();
        instr_queue_pop();
        instr_queue_pop();
    }
}

/*
 * Return from Subroutine
 * Takes in cycle to complete the right work
 * Pops return address from stack and writes it into PC
 */
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

/*
 * Return from Subroutine
 * Takes in CYCLE to complete the right work
 * Pops return address from stack and writes it into PC and enable interrupts
 */
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

/*
 * Restart instruction
 * Takes in CYCLE number to complete the right work
 * Calls a vec address
 */
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
            CPU->DATA_BUS = CPU->REGS[PC0];
            CPU->ADDRESS_BUS = read_16bit_reg(SP);
            write_memory(UNUSED_VAL);
            CPU->REGS[PC0] = CPU->REGS[Z];
            CPU->REGS[PC1] = 0x00;
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
//TODO halt bug
void halt() {
    if (CPU->IME) {
        CPU->STATE = HALTED;
    }
    else if (!(MEMORY[IF] & MEMORY[IE])) {
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