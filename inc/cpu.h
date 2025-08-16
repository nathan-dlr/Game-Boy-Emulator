#ifndef GB_EMU_CPU_H
#define GB_EMU_CPU_H


enum REGISTERS_16BIT {
    AF,
    BC,
    DE,
    HL,
    SP,
    PC,
    WZ
};

enum REGISTERS_8BIT {
    A,
    F,
    B,
    C,
    D,
    E,
    H,
    L,
    SP1,
    SP0,
    PC1,
    PC0,
    W,
    Z
};

enum CC {
    NOT_ZERO,
    ZERO,
    NOT_CARRY,
    CARRY,
    NONE
};

enum CPU_STATES {
    RUNNING,
    HALTED,
    OAM_DMA_TRANSFER
};

typedef struct CPU_STRUCT {
    uint16_t REGS[14];
    uint8_t IME;
    uint8_t DATA_BUS;
    uint16_t ADDRESS_BUS;
    uint8_t DMA_CYCLE;
    enum CPU_STATES STATE;
} CPU_STRUCT;

CPU_STRUCT* CPU;
void cpu_init();
void execute_next_CPU_cycle();

void read_next_byte();
uint16_t read_16bit_reg(uint8_t reg_pair);
void write_16bit_reg(uint8_t reg_pair, uint16_t value);

//Loads
void ld_r8_imm8(uint8_t dest);
void ld_rW_imm8(uint8_t load_a);
void ld_r8_data_bus(uint8_t dest);
void ldh_imm8(uint8_t UNUSED);
void ld_imm16_sp(uint8_t byte_num);
void ld_hl_sp8(uint8_t cycle);
void ld_sp_hl(uint8_t UNUSED);
void ld_hl_imm8(uint8_t cycle);
void ld_a_imm16(uint8_t cycle);
void ldh_a_imm8(uint8_t cycle);
//8-bit arithmetic
void add_A_8bit(uint8_t is_imm);
void adc(uint8_t operand_type);
void cp(uint8_t is_imm);
void sub(uint8_t is_imm);
void sbc(uint8_t is_imm);
void inc_8bit(uint8_t disassembly_table_index);
void dec_8bit(uint8_t disassembly_table_index);
//16-bit arithmetic
void add_HL_16bit(uint8_t source);
void add_sp_e8(uint8_t cycle);
void inc_16bit(uint8_t dest);
void dec_16bit(uint8_t dest);
//8-bit logic
void and(uint8_t is_imm);
void or(uint8_t is_imm);
void xor(uint8_t is_imm);
void cpl();
//Bit flags instructions
void bit(uint8_t bit_num);
void res(uint8_t opcode);
void set(uint8_t opcode);
//Bit shift instructions
void rl(uint8_t disassembly_table_index);
void rla();
void rlc(uint8_t disassembly_table_index);
void rlca();
void rr(uint8_t disassembly_table_index);
void rra();
void rrc(uint8_t disassembly_table_index);
void rrca();
void sla(uint8_t disassembly_table_index);
void sra(uint8_t disassembly_table_index);
void srl(uint8_t disassembly_table_index);
void swap(uint8_t disassembly_table_index);
//Jumps and subroutine instructions
void call_cycle3(uint8_t cc);
void call_writes(uint8_t cycle_num);
void jp_cycle3(uint8_t cc);
void jp(uint8_t is_hl);
void jr_cycle2(uint8_t cc);
void jr(uint8_t UNUSED);
void ret_eval_cc(uint8_t cc);
void ret(uint8_t cycle);
void reti(uint8_t cycle);
void rst(uint8_t cycle);
//Carry Flag Instructions
void scf();
void ccf();
//Stack Manipulation
void pop_reads(uint8_t cycle);
void pop_load(uint8_t reg_16);
void push(uint8_t cycle);
//Interrupt-related instructions
void di();
void ei();
void halt();
//Miscellaneous instructions
void daa();
void nop(uint8_t opcode);
void stop();

#endif //GB_EMU_CPU_H
