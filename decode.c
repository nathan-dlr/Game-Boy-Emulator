#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "cpu.h"
#include "gb.h"

#define PREFIX 0xCB
#define GET_FIRST_OCTAL_DIGIT(byte) ((byte & 0xC0) >> 6)
#define GET_SECOND_OCTAL_DIGIT(byte) ((byte & 0x38) >> 3)
#define GET_THIRD_OCTAL_DIGIT(byte) (byte & 0x07)
#define GET_BIT_THREE(byte) ((byte & 0x08) >> 3)
#define GET_BITS_FOUR_FIVE(byte) ((byte & 0x30) >> 4)

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
static void cb_prefixed_ops(uint8_t opcode);

//TODO CONSIDER CHANGING TO CONTROL.C
//TODO CONSIDER LOAD REG ON BUS FUNC

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
        {mem_mapped_ops, pop_various, conditional_jumps,assorted_ops, conditional_calls, push_call_nop, alu,     rst},
        {cb_prefixed_ops,nop,nop,nop,nop,nop,nop,nop}
};

typedef void (*rot_shift_func)(uint8_t, bool);
static rot_shift_func rotation_shift_ops[8] = {rlc, rrc, rl ,rr, sla, sra, swap, srl};
/*
 * Gets indexes for decode lookup table using OPCODE and jumps to that function
 * Algorithm described in "DECODING Game Boy Z80 OPCODES" by Scott Mansell
*/
void decode(uint8_t opcode) {
    bool CB_prefix = opcode == PREFIX;
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
            second_index = 0;
            break;
        case 2:
            second_index = second_octal_dig;
            break;
        case 3:
            second_index = third_octal_dig;
            break;
        case 4:
            second_index = 0;
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
        case 1:
            //LD [n16],SP - 5 cycles: decode -> read_next_byte -> read_next_byte -> write_memory -> write_memory
            //ld(fetch_word(), SP, POINTER, REG_16BIT);
            ld_r8_imm8(Z);
            ld_rW_imm8(0);
            ld_imm16_sp(0);
            ld_imm16_sp(1);
            return;
        case 2:
            stop();
            return;
        case 3:
            jr(NONE);
            break;
        default:
            jr(CC[second_octal_dig - 4]);
            break;
    }
}

//BC, DE, HL, SP};
static void load_immediate_add_16bit(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    //ADD HL,r16 - 2 cycles: decode -> add first bit -> add second bit
    if (bit_three) {

    }
    //LD r16,n16 - 3 cycles: decode -> ld_r8_imm -> ld_r8_imm
    else {
        switch (bits_four_five) {
            case 0:
                ld_r8_imm8(C);
                ld_r8_imm8(B);
                return;
            case 1:
                ld_r8_imm8(E);
                ld_r8_imm8(D);
                return;
            case 2:
                ld_r8_imm8(L);
                ld_r8_imm8(H);
                return;
            case 3:
                ld_r8_imm8(SP0);
                ld_r8_imm8(SP1);
        }

    }
    //bit_three ? add(REGISTER_PAIRS_DT[bits_four_five], REG_16BIT) : ld(REGISTER_PAIRS_DT[bits_four_five], fetch_word(), REG_16BIT, CONST_16BIT);
}

static void indirect_loading(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    switch (bits_four_five) {
        case 0:
            //bit_three ? ld(A, BC, REG_8BIT, REG_POINTER) : ld(BC, A, REG_POINTER, REG_8BIT);
            //LD A,[r16] - 2 cycles: decode -> read_memory/ld_r8_bus
            if (bit_three) {
                //ld(A, BC, REG_8BIT, REG_POINTER);
                CPU.ADDRESS_BUS = CPU.REGS[BC];
                ld_r8_addr_bus(A);
            }
            //LD [r16],A - 2 cycles: decode -> write_memory
            else {
                //ld(BC, A, REG_POINTER, REG_8BIT);
                CPU.ADDRESS_BUS = CPU.REGS[BC];
                CPU.DATA_BUS = CPU.REGS[A]; //TODO need to change into 8bit read
                write_memory2();
            }
            return;
        case 1:
            //bit_three ? ld(A, DE, REG_8BIT, REG_POINTER) : ld(DE, A, REG_POINTER, REG_8BIT);
            //LD A,[r16] - 2 cycles: decode -> read_memory/ld_r8_bus
            if (bit_three) {
                //ld(A, DE, REG_8BIT, REG_POINTER);
                CPU.ADDRESS_BUS = CPU.REGS[DE];
                ld_r8_addr_bus(A);
            }
            //LD [r16],A - 2 cycles: decode -> write_memory
            else {
                //ld(DE, A, REG_POINTER, REG_8BIT);
                CPU.ADDRESS_BUS = CPU.REGS[BC];
                CPU.DATA_BUS = CPU.REGS[A]; //TODO need to change into 8bit read
                write_memory2();
            }
            return;
        case 2:
            //bit_three ? ld_inc(SOURCE_INC) : ld_inc(DEST_INC);
            //LD A,[HLI] - 2 cycles: decode -> ld_r8_addr_bus
            if (bit_three) {
                //ld_inc(SOURCE_INC);
                CPU.ADDRESS_BUS = CPU.REGS[HL];
                CPU.REGS[HL]++;
                ld_r8_addr_bus(A);
            }
            //LD [HLI],A - 2 cycles: decode -> write_memory
            else {
                //ld_inc(DEST_INC);
                CPU.ADDRESS_BUS = CPU.REGS[HL];
                CPU.REGS[HL]++;
                CPU.DATA_BUS = CPU.REGS[A]; //TODO CHANGE TO 8bit read
                write_memory2();
            }
            return;
        case 3:
            //bit_three ? ld_inc(SOURCE_DEC) : ld_inc(DEST_DEC);
            if (bit_three) {
                //ld_inc(SOURCE_DEC);
                CPU.ADDRESS_BUS = CPU.REGS[HL];
                CPU.REGS[HL]--;
                ld_r8_addr_bus(A);
            }
            else {
                //ld_inc(DEST_DEC);
                CPU.ADDRESS_BUS = CPU.REGS[HL];
                CPU.REGS[HL]--;
                CPU.DATA_BUS = CPU.REGS[A]; //TODO CHANGE TO 8bit read
                write_memory2();
            }
            return;
        default:
            perror("Invalid opcode in decoding indirect loading");
    }
}

static void inc_or_dec(uint8_t opcode) {
    uint8_t third_octal_dig = GET_THIRD_OCTAL_DIGIT(opcode);
    uint8_t operand;
    //operand is 16 bit register
    if (third_octal_dig == 3) {
        uint8_t bit_three = GET_BIT_THREE(opcode);
        uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
        operand = REGISTER_PAIRS_DT[bits_four_five];
        bit_three ? dec(operand, REG_16BIT) : inc(operand,REG_16BIT);
    }
    //operand is 8 bit register or byte pointed to by HL
    else {
        uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
        uint8_t operand_type = second_octal_dig == 6 ? REG_POINTER : REG_8BIT;
        operand = REGISTERS_DT[second_octal_dig];
        third_octal_dig == 4 ? inc(operand, operand_type) : dec(operand, operand_type);
    }
}

//ld r8 imm8 or ld [hl] imm8
static void ld_8bit(uint8_t opcode) {
    uint8_t reg_dt_index = GET_SECOND_OCTAL_DIGIT(opcode);
    uint8_t dest_type = reg_dt_index == 6 ? REG_POINTER : REG_8BIT;
    ld(REGISTERS_DT[reg_dt_index], fetch_byte(), dest_type, CONST_8BIT);
    //LD r8,n8 - 2 cycles: decode -> ld_r8_imm
    if (dest_type != 6) {
        ld_r8_imm8(REGISTERS_DT[reg_dt_index]);
    }
    //LD [HL],n8 - 3 cycles: decode -> read_next_byte -> write_bus
    else {
        read_next_byte();
        write_memory2();
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
    else if (source_reg != 6 && dest_reg != 8) {
        write_8bit_reg(dest_reg, read_8bit_reg(source_reg));
    }
    //LD r8,[HL] - 2 cycles: decode -> ld_r8_addr_bus
    else  if (source_reg == 6) {
        CPU.ADDRESS_BUS = CPU.REGS[HL];
        ld_r8_addr_bus(dest_reg);
    }
    //LD [HL],r8 - 2 cycles: decode -> write_memory
    else {
        CPU.ADDRESS_BUS = CPU.REGS[HL];
        CPU.DATA_BUS = read_8bit_reg(source_reg); //MAKE SURE 8bit read works
        write_memory2();
    }
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
        if (third_octal_dig == 6) {
            CPU.ADDRESS_BUS = CPU.REGS[HL];
            //TODO GO IN QUEUE
            read_memory2();
        }
        else {
            CPU.DATA_BUS = CPU.REGS[operand]
        }

    }

    switch (second_octal_dig) {
        case 0:
            add(operand);
            return;
        case 1:
            adc(operand, operand_type);
            return;
        case 2:
            sub(operand, operand_type);
            return;
        case 3:
            sbc(operand, operand_type);
            return;
        case 4:
            and(operand, operand_type);
            return;
        case 5:
            xor(operand, operand_type);
            return;
        case 6:
            or(operand, operand_type);
            return;
        case 7:
            cp(operand, operand_type);
            return;
        default:
            perror("Invalid opcode in alu decode");
    }
}

static void mem_mapped_ops(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        default:
            ret(CC[second_octal_dig]);
            return;
        //LDH [n16],A - 3 cycles: decode -> read_next_byte -> write_memory
        case 4:
            //ld(fetch_byte(), A, OFFSET, REG_8BIT);
            CPU.DATA_BUS = read_8bit_reg(A); //TODO doesnt work with this function yet
            ldh_imm8();
            write_memory2();
            return;
        //ADD A,n8 - 2 cycles: decode -> read_next_byte/add
        case 5:
            add_imm();
            return;
        //LDH A,[n16] - 3 cycles: decode -> read_next_byte -> read_memory/ld_r8_bus
        case 6:
            //ld(A, fetch_byte(), REG_8BIT, OFFSET);
            ldh_imm8();
            ld_r8_addr_bus(A);
            return;
        //LD HL,SP+e8 - 3 cycles: decode -> read_next_byte -> add and load
        //TODO make this more accurate according to docs
        case 7:
            //ld_sp_off(fetch_byte());
            read_next_byte();
            ld_hl_sp8();
            return;
    }
}

static void pop_various(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    if (!bit_three) {
        pop(REGISTER_PAIRS2_DT[bits_four_five]);
    }
    else {
        switch (bits_four_five) {
            case 0:
                ret(NONE);
                return;
            case 1:
                reti();
                return;
            case 2:
                jp(NONE, true);
                return;
            //LD SP,HL - 2 cycles : decode -> ld_sp_hl
            case 3:
                //ld(SP, HL, REG_16BIT, REG_16BIT);
                ld_sp_hl();
                return;
            default:
                perror("Invalid opcode in decoding pop");
        }
    }
}

static void conditional_jumps(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        default:
            jp(CC[second_octal_dig], false);
            return;
        //LDH [C],A - 2 cycles: decode -> write_memory
        case 4:
            //ld(C, A, REG_OFFSET, REG_8BIT);
            CPU.ADDRESS_BUS = 0xFF00 + read_8bit_reg(C); //TODO DOESNT WORK YET
            CPU.DATA_BUS = read_8bit_reg(A);
            write_memory2();
            return;
        //LD [n16],A - 4 cycles: decode -> read_next_byte -> read_next_byte -> write_memory
        case 5:
            //ld(fetch_word(), A, POINTER, REG_8BIT);
            ld_r8_imm8(Z);
            ld_rW_imm8(1);
            write_memory2();
            return;
        //LDH A,[C] - 2 cycles: decode -> read_memory/ld_r8_bus
        case 6:
            //ld(A, C, REG_8BIT, REG_OFFSET);
            CPU.ADDRESS_BUS = 0xFF00 + read_8bit_reg(C); //TODO DOESNT WORK YET
            ld_r8_addr_bus(A);
            return;
        //LD A,[n16] - 4 cycles: decode -> read_next_byte -> read_next_byte -> read_memory/ld_r8_bus
        case 7:
            //ld(A, fetch_word(), REG_8BIT, POINTER);
            ld_r8_imm8(Z);
            ld_rW_imm8(0);
            ld_r8_addr_bus(A);
            return;
    }
}

static void assorted_ops(uint8_t opcode) {
    uint8_t second_octal_dig = GET_SECOND_OCTAL_DIGIT(opcode);
    switch (second_octal_dig) {
        case 0:
            jp(NONE, false);
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
    second_octal_dig < 4 ? call(CC[second_octal_dig], fetch_word()) : nop(opcode);
}

static void push_call_nop(uint8_t opcode) {
    uint8_t bit_three = GET_BIT_THREE(opcode);
    uint8_t bits_four_five = GET_BITS_FOUR_FIVE(opcode);
    if (!bit_three) {
        push(REGISTER_PAIRS2_DT[bits_four_five]);
    }
    else {
        bits_four_five == 0 ? call(NONE, fetch_word()) : nop(opcode);
    }
}

static void cb_prefixed_ops(uint8_t opcode) {
    opcode = fetch_byte();
    uint8_t first_octal_dig = GET_FIRST_OCTAL_DIGIT(opcode);
    uint8_t bit_num = GET_SECOND_OCTAL_DIGIT(opcode);
    uint8_t reg = GET_THIRD_OCTAL_DIGIT(opcode);
    bool reg_8bit = reg != 6;
    switch (first_octal_dig) {
        case 0:
            //TODO change "bit_num" to something that makes more sense?
            rotation_shift_ops[bit_num](REGISTERS_DT[reg], reg_8bit);
            return;
        case 1:
            bit(bit_num, REGISTERS_DT[reg], reg_8bit);
            return;
        case 2:
            res(bit_num, REGISTERS_DT[reg], reg_8bit);
            return;
        case 3:
            set(bit_num, REGISTERS_DT[reg], reg_8bit);
            return;
        default:
            perror("Invalid opcode in cb_prefixed_op");
    }
}
