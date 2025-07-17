#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <decode.h>
#include <cpu.h>
#include <gb.h>
#include <queue.h>

#define PREFIX 0xCB
#define GET_FIRST_OCTAL_DIGIT(byte) ((byte & 0xC0) >> 6)
#define GET_SECOND_OCTAL_DIGIT(byte) ((byte & 0x38) >> 3)
#define GET_THIRD_OCTAL_DIGIT(byte) (byte & 0x07)
#define GET_BIT_THREE(byte) ((byte & 0x08) >> 3)
#define GET_BITS_FOUR_FIVE(byte) ((byte & 0x30) >> 4)
#define TRUE 1
#define FALSE 0

static void relative_jumps(uint8_t opcode);
static void load_immediate_add_16bit(uint8_t opcode);
static void indirect_loading(uint8_t opcode);
static void inc_or_dec(uint8_t opcode);
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
static void rst_instr(uint8_t opcode);
static void cb_prefixed_ops(uint8_t opcode);

/*
 * DISASSEMBLY TABLES
 * These tables aid in decoding instructions as outlined in "DECODING Gameboy Z80 OPCODES" by Scott Mansell
 * These tables DO NOT directly access any registers
 */
static uint8_t REGISTERS_DT[8] = {B, C, D, E, H, L, HL, A};
static uint8_t REGISTER_PAIRS_DT[4] = {BC, DE, HL, SP};
static uint8_t REGISTER_PAIRS2_DT[4] = {BC, DE, HL, AF};
static uint8_t CC[4] = {NOT_ZERO, ZERO, NOT_CARRY, CARRY};

typedef void (*opcode_func)(uint8_t);
static opcode_func decode_lookup[5][8] = {
        {relative_jumps,load_immediate_add_16bit,indirect_loading,inc_or_dec,inc_or_dec,inc_or_dec,ld_8bit,ops_on_accumulator},
        {ld_or_halt,nop,nop,nop,nop,nop,nop,nop},
        {alu,alu,alu,alu,alu,alu,alu,alu},
        {mem_mapped_ops, pop_various, conditional_jumps,assorted_ops, conditional_calls, push_call_nop, alu, rst_instr},
        {cb_prefixed_ops,nop,nop,nop,nop,nop,nop,nop}
};

static execute_func rotation_shift_ops[8] = {rlc, rrc, rl ,rr, sla, sra, swap, srl};

static execute_func alu_ops[8] = {add_A_8bit, adc, sub, sbc, and, xor, or, cp};
uint8_t get_reg_dt(uint8_t index) {
    return REGISTERS_DT[index];
}
/*
 * Gets indexes for decode lookup table using OPCODE and jumps to that function
 * Algorithm described in "DECODING Game Boy Z80 OPCODES" by Scott Mansell
*/
void decode() {
    uint8_t opcode = CPU->DATA_BUS;
    bool CB_prefix = opcode == PREFIX;
    uint8_t first_index = GET_FIRST_OCTAL_DIGIT(opcode);
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    uint8_t third_octal_dig = GET_THIRD_OCTAL_DIGIT(opcode);
    if (CB_prefix) {
        queue_push(INSTR_QUEUE, cb_prefixed_ops, UNUSED_VAL);
        return;
    }
    uint8_t second_index;
    switch (first_index) {
        case 0:
            second_index = third_octal_dig;
            break;
        case 1:
            second_index = 0;
            break;
        case 2:
            second_index = second_octal_dig;
            break;
        case 3:
            second_index = third_octal_dig;
            break;
        default:
            second_index = 0;
            perror("Invalid opcode in decode");
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
            return;
        //LD [n16],SP - 5 cycles: decode -> read_next_byte -> read_next_byte -> write_memory -> write_memory
        case 1:
            queue_push(INSTR_QUEUE, ld_r8_imm8, Z);
            queue_push(INSTR_QUEUE, ld_rW_imm8, FALSE);
            queue_push(INSTR_QUEUE, ld_imm16_sp, 0);
            queue_push(INSTR_QUEUE, ld_imm16_sp, 1);
            return;
        case 2:
            stop();
            return;
        // jr n16 - 3 cycles
        case 3:
            queue_push(INSTR_QUEUE, jr_cycle2, NONE);
            queue_push(INSTR_QUEUE, jr, UNUSED_VAL);
            return;
        //jr cc n16 - 3 cycles taken/ 2 cycles untaken
        default:
            queue_push(INSTR_QUEUE, jr_cycle2, CC[second_octal_dig - 4]);
            queue_push(INSTR_QUEUE, jr, UNUSED_VAL);
            break;
    }
}

static void load_immediate_add_16bit(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    //ADD HL,r16 - 2 cycles: decode -> add first bit -> add_A_8bit second bit
    if (bit_three) {
        switch (bits_four_five) {
            case 0:
                add_HL_16bit(C);
                queue_push(INSTR_QUEUE, add_HL_16bit, B);
                break;
            case 1:
                add_HL_16bit(E);
                queue_push(INSTR_QUEUE, add_HL_16bit, D);
                break;
            case 2:
                add_HL_16bit(L);
                queue_push(INSTR_QUEUE, add_HL_16bit, H);
                break;
            case 3:
                add_HL_16bit(SP0);
                queue_push(INSTR_QUEUE, add_HL_16bit, SP1);
                break;
            default:
                perror("Invalid opcode during load_immediate_add_16bit, add HL r16");
                break;
        }
    }
    //LD r16,n16 - 3 cycles: decode -> ld_r8_imm -> ld_r8_imm
    else {
        switch (bits_four_five) {
            case 0:
                queue_push(INSTR_QUEUE, ld_r8_imm8, C);
                queue_push(INSTR_QUEUE, ld_r8_imm8, B);
                return;
            case 1:
                queue_push(INSTR_QUEUE, ld_r8_imm8, E);
                queue_push(INSTR_QUEUE, ld_r8_imm8, D);
                return;
            case 2:
                queue_push(INSTR_QUEUE, ld_r8_imm8, L);
                queue_push(INSTR_QUEUE, ld_r8_imm8, H);
                return;
            case 3:
                queue_push(INSTR_QUEUE, ld_r8_imm8, SP0);
                queue_push(INSTR_QUEUE, ld_r8_imm8, SP1);
                return;
            default:
                perror("Invalid opcode during load_immediate_add_16bit");
                return;
        }
    }
}

static void indirect_loading(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    execute_func func = bit_three ? &ld_r8_addr_bus : &write_memory;
    uint8_t param = bit_three ? A : UNUSED_VAL;
    CPU->DATA_BUS = CPU->REGS[A];
    switch (bits_four_five) {
        case 0:
            CPU->ADDRESS_BUS = read_16bit_reg(BC);
            queue_push(INSTR_QUEUE, func, param);
            return;
        case 1:
            CPU->ADDRESS_BUS = read_16bit_reg(DE);
            queue_push(INSTR_QUEUE, func, param);
            return;
        case 2:
            CPU->ADDRESS_BUS = read_16bit_reg(HL);
            write_16bit_reg(HL, read_16bit_reg(HL) + 1);
            queue_push(INSTR_QUEUE, func, param);
            return;
        case 3:
            CPU->ADDRESS_BUS = read_16bit_reg(HL);
            write_16bit_reg(HL, read_16bit_reg(HL) - 1);
            queue_push(INSTR_QUEUE, func, param);
            return;
        default:
            perror("Invalid opcode in decoding indirect loading");
    }
}


static void inc_or_dec(uint8_t opcode) {
    uint8_t third_octal_dig = GET_THIRD_OCTAL_DIGIT(opcode);
    uint8_t operand;
    execute_func func;
    //operand is 16 bit register
    if (third_octal_dig == 3) {
        uint8_t bit_three = GET_BIT_THREE(opcode);
        uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
        func = bit_three ? &dec_16bit : &inc_16bit;
        operand = REGISTER_PAIRS_DT[bits_four_five];
        queue_push(INSTR_QUEUE, func, operand);
    }
    //operand is 8 bit register or byte pointed to by HL
    else {
        uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
        func = third_octal_dig == 4 ? &inc_8bit : &dec_8bit;
        operand = second_octal_dig;
        if (second_octal_dig == 6) {
            CPU->ADDRESS_BUS = read_16bit_reg(HL);
            queue_push(INSTR_QUEUE, read_memory, UNUSED_VAL);
            queue_push(INSTR_QUEUE, func, operand);
        }
        else {
            func(operand);
        }
    }
}

static void ld_8bit(uint8_t opcode) {
    uint8_t reg_dt_index = GET_SECOND_OCTAL_DIGIT(opcode);
    //LD r8,n8 - 2 cycles: decode -> ld_r8_imm
    if (reg_dt_index != 6) {
        queue_push(INSTR_QUEUE, ld_r8_imm8, REGISTERS_DT[reg_dt_index]);
    }
    //LD [HL],n8 - 3 cycles: decode -> read_next_byte -> write_bus
    else {
        queue_push(INSTR_QUEUE, ld_hl_imm8, 2);
        queue_push(INSTR_QUEUE, ld_hl_imm8, 3);
    }
}

static void ops_on_accumulator(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        case 0:
            rlca();
            return;
        case 1:
            rrca();
            return;
        case 2:
            rla();
            return;
        case 3:
            rra();
            return;
        case 4:
            daa();
            return;
        case 5:
            cpl();
            return;
        case 6:
            scf();
            return;
        case 7:
            ccf();
            return;
        default:
            perror("Invalid opcode in decoding ops on accumulator");
    }
}

static void ld_or_halt(uint8_t opcode) {
    uint8_t dest_reg = GET_SECOND_OCTAL_DIGIT(opcode);
    uint8_t source_reg = GET_THIRD_OCTAL_DIGIT(opcode);
    if ((source_reg == 6) && (dest_reg == 6)) {
        halt();
    }
    //LD r8,r8 - 1 cycle: decode/ld_r8_bus
    else if (source_reg != 6 && dest_reg != 6) {
        CPU->DATA_BUS = CPU->REGS[REGISTERS_DT[source_reg]];
        ld_r8_data_bus(REGISTERS_DT[dest_reg]);
    }
    //LD r8,[HL] - 2 cycles: decode -> ld_r8_addr_bus
    else  if (source_reg == 6) {
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        queue_push(INSTR_QUEUE, ld_r8_addr_bus, REGISTERS_DT[dest_reg]);
    }
    //LD [HL],r8 - 2 cycles: decode -> write_memory
    else {
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        CPU->DATA_BUS = CPU->REGS[REGISTERS_DT[source_reg]];
        queue_push(INSTR_QUEUE, write_memory, UNUSED_VAL);
    }
}

static void alu(uint8_t opcode) {
    uint8_t first_octal_dig = GET_FIRST_OCTAL_DIGIT(opcode);
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    uint8_t third_octal_dig = GET_THIRD_OCTAL_DIGIT(opcode);
    uint8_t operand;

    //alu imm - 2 cycles
    if (first_octal_dig == 3) {
        alu_ops[second_octal_dig](TRUE);
        queue_push(INSTR_QUEUE, alu_ops[second_octal_dig], FALSE);
    }
    else {
        operand = REGISTERS_DT[third_octal_dig];
        //alu A [HL] - 2 cycles
        if (third_octal_dig == 6) {
            CPU->ADDRESS_BUS = read_16bit_reg(HL);
            read_memory(UNUSED_VAL);
            queue_push(INSTR_QUEUE, alu_ops[second_octal_dig], FALSE);
        }
        //alu r8, r8 - 1 cycles
        else {
            CPU->DATA_BUS = CPU->REGS[operand];
            alu_ops[second_octal_dig](FALSE);
        }
    }
}

static void mem_mapped_ops(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        //RET cc - 5 cycles taken/ 3 untaken
        default:
            queue_push(INSTR_QUEUE, ret_eval_cc, CC[second_octal_dig]);
            queue_push(INSTR_QUEUE, ret, 0);
            queue_push(INSTR_QUEUE, ret, 1);
            queue_push(INSTR_QUEUE, ret, 2);
            return;
        //LDH [n16],A - 3 cycles: decode -> read_next_byte -> write_memory
        case 4:
            queue_push(INSTR_QUEUE, ldh_imm8, UNUSED_VAL);
            queue_push(INSTR_QUEUE, write_memory, UNUSED_VAL);
            return;
        //ADD sp e8 - 4 cycles
        case 5:
            queue_push(INSTR_QUEUE, add_sp_e8, 2);
            queue_push(INSTR_QUEUE, add_sp_e8, 3);
            queue_push(INSTR_QUEUE, add_sp_e8, 4);
            return;
        //LDH A,[n16] - 3 cycles: decode -> read_next_byte -> read_memory/ld_r8_bus
        case 6:
            queue_push(INSTR_QUEUE, ldh_imm8, UNUSED_VAL);
            queue_push(INSTR_QUEUE, ld_r8_addr_bus, A);
            return;
        //LD HL,SP+e8 - 3 cycles: decode -> read_next_byte -> add_A_8bit and load
        case 7:
            queue_push(INSTR_QUEUE, ld_hl_sp8, 2);
            queue_push(INSTR_QUEUE, ld_hl_sp8, 3);
            return;
    }
}

static void pop_various(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    // POP [r16] - 3 cycles
    if (!bit_three) {
        pop_reads(2);
        queue_push(INSTR_QUEUE, pop_reads, 3);
        queue_push(INSTR_QUEUE, pop_load, REGISTER_PAIRS2_DT[bits_four_five]);
    }
    else {
        switch (bits_four_five) {
            //ret 4 cycles
            case 0:
                queue_push(INSTR_QUEUE, ret, 0);
                queue_push(INSTR_QUEUE, ret, 1);
                queue_push(INSTR_QUEUE, ret, 2);
                return;
            //reti 4 cycles
            case 1:
                queue_push(INSTR_QUEUE, reti, 2);
                queue_push(INSTR_QUEUE, reti, 3);
                queue_push(INSTR_QUEUE, reti, 4);
                return;
            //jp hl - 1 cycle
            case 2:
                jp(TRUE);
                return;
            //LD SP,HL - 2 cycles : decode -> ld_sp_hl
            case 3:
                queue_push(INSTR_QUEUE, ld_sp_hl, UNUSED_VAL);
                return;
            default:
                perror("Invalid opcode in decoding queue_pop");
        }
    }
}

static void conditional_jumps(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        //jp cc - 4 cycles taken/ 3 untaken
        default:
            queue_push(INSTR_QUEUE, ld_r8_imm8, Z);
            queue_push(INSTR_QUEUE, jp_cycle3, CC[second_octal_dig]);
            queue_push(INSTR_QUEUE, jp, FALSE);
            return;
        //LDH [C],A - 2 cycles: decode -> write_memory
        case 4:
            CPU->ADDRESS_BUS = 0xFF00 + CPU->REGS[C];
            CPU->DATA_BUS = CPU->REGS[A];
            queue_push(INSTR_QUEUE, write_memory, UNUSED_VAL);
            return;
        //LD [n16],A - 4 cycles: decode -> read_next_byte -> read_next_byte -> write_memory
        case 5:
            queue_push(INSTR_QUEUE, ld_r8_imm8, Z);
            queue_push(INSTR_QUEUE, ld_rW_imm8, TRUE);
            queue_push(INSTR_QUEUE, write_memory, UNUSED_VAL);
            return;
        //LDH A,[C] - 2 cycles: decode -> read_memory/ld_r8_bus
        case 6:
            CPU->ADDRESS_BUS = 0xFF00 + CPU->REGS[C];
            queue_push(INSTR_QUEUE, ld_r8_addr_bus, A);
            return;
        //LD A,[n16] - 4 cycles: decode -> read_next_byte -> read_next_byte -> read_memory/ld_r8_bus
        case 7:
            queue_push(INSTR_QUEUE, ld_r8_imm8, Z);
            queue_push(INSTR_QUEUE, ld_rW_imm8, FALSE);
            queue_push(INSTR_QUEUE, ld_r8_addr_bus, A);
            return;
    }
}

static void assorted_ops(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        //jp n16 - 4 cycles
        case 0:
            queue_push(INSTR_QUEUE, ld_r8_imm8, Z);
            queue_push(INSTR_QUEUE, jp_cycle3, NONE);
            queue_push(INSTR_QUEUE, jp, FALSE);
            return;
        case 6:
            di();
            return;
        case 7:
            ei();
            return;
        //instructions whose opcode's second octal digit are 2-5 are usually implemented in the Z80 but not on the gbz80
        default:
            nop(opcode);
            return;
    }
}

static void conditional_calls(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    //instructions whose opcode's second octal digit are 4-7 are usually implemented in the Z80 but not on the gbz80
    if (second_octal_dig < 4) {
        //call 6 cycles taken/ 3 untakem
        queue_push(INSTR_QUEUE, ld_r8_imm8, Z);
        queue_push(INSTR_QUEUE, call_cycle3, CC[second_octal_dig]);
        queue_push(INSTR_QUEUE, dec_16bit, SP);
        queue_push(INSTR_QUEUE, call_writes, 5);
        queue_push(INSTR_QUEUE, call_writes, 6);
    }
    else {
        nop(opcode);
    }
}

static void push_call_nop(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    //push 4 cycles
    if (!bit_three) {
        write_16bit_reg(WZ, read_16bit_reg(REGISTER_PAIRS2_DT[bits_four_five]));
        queue_push(INSTR_QUEUE, push, 2);
        queue_push(INSTR_QUEUE, push, 3);
        queue_push(INSTR_QUEUE, push, 4);
    }
    else if (bits_four_five) {
        nop(opcode);
    }
    //call 6 cycles
    else {
        queue_push(INSTR_QUEUE, ld_r8_imm8, Z);
        queue_push(INSTR_QUEUE, call_cycle3, NONE);
        queue_push(INSTR_QUEUE, dec_16bit, SP);
        queue_push(INSTR_QUEUE, call_writes, 5);
        queue_push(INSTR_QUEUE, call_writes, 6);
    }
}

static void rst_instr(uint8_t opcode) {
    //rst 4 cycles
    CPU->DATA_BUS = GET_SECOND_OCTAL_DIGIT(opcode) * 8;
    queue_push(INSTR_QUEUE, rst, 2);
    queue_push(INSTR_QUEUE, rst, 3);
    queue_push(INSTR_QUEUE, rst, 4);
}


/*
 * INCORRECT TIMING HERE
 * CB 46 - bit 0 [HL], measure 4 cycles, correct is 3 cycles
 */
static void cb_prefixed_ops(uint8_t opcode) {
    read_next_byte();
    opcode = CPU->DATA_BUS;
    uint8_t first_octal_dig = GET_FIRST_OCTAL_DIGIT(opcode);
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    uint8_t bit_num = second_octal_dig;
    uint8_t reg = GET_THIRD_OCTAL_DIGIT(opcode);
    bool write_mem = false;
    if ((reg == 6) & (first_octal_dig != 1)) {
        CPU->ADDRESS_BUS = read_16bit_reg(HL);
        queue_push(INSTR_QUEUE, read_memory, UNUSED_VAL);
        write_mem = true;
    }
    switch (first_octal_dig) {
        case 0:
            if (write_mem) {
                queue_push(INSTR_QUEUE, rotation_shift_ops[second_octal_dig], reg);
            }
            else {
                rotation_shift_ops[bit_num](reg);
            }
            break;
        case 1:
            if (reg == 6) {
                CPU->ADDRESS_BUS = read_16bit_reg(HL);
                read_memory(UNUSED_VAL);
                queue_push(INSTR_QUEUE, bit, bit_num);
            }
            else {
                CPU->DATA_BUS = CPU->REGS[REGISTERS_DT[reg]];
                bit(bit_num);
            }
            break;
        case 2:
            if (write_mem) {
                queue_push(INSTR_QUEUE, res, opcode);
            }
            else {
                res(opcode);
            }
            break;
        case 3:
            if (write_mem) {
                queue_push(INSTR_QUEUE, set, opcode);
            }
            else {
                set(opcode);
            }
            break;
        default:
            perror("Invalid opcode in cb_prefixed_op");
    }
}
