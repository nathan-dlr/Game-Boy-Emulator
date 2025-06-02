#ifndef GB_EMU_CPU_H
#define GB_EMU_CPU_H

void cpu_init();
uint8_t fetch_byte();
uint16_t fetch_word();

void nop(uint8_t opcode);
void stop();
void ld(uint16_t dest, uint16_t source, uint8_t dest_type, uint8_t source_type);
void jr();
void inc(uint8_t operand, uint8_t operand_type);
void dec(uint8_t operand, uint8_t operand_type);
void rl(uint8_t source_reg, bool reg_8bit);
void rla();
void rlc(uint8_t source_reg, bool reg_8bit);
void rlca();
void rr(uint8_t source_reg, bool reg_8bit);
void rra();
void rrc(uint8_t source_reg, bool reg_8bit);
void rrca();
void sla(uint8_t source_reg, bool reg_8bit);
void sra(uint8_t source_reg, bool reg_8bit);
void swap(uint8_t source_reg, bool reg_8bit);
void srl(uint8_t source_reg, bool reg_8bit);
void daa();
void cpl();
void scf();
void ccf();
void halt();
void add(uint16_t operand, uint8_t operand_type);
void adc(uint8_t operand, uint8_t operand_type);
void sub(uint8_t operand, uint8_t operand_type);
void sbc(uint8_t operand, uint8_t operand_type);
void cp(uint8_t operand, uint8_t operand_type);
void and(uint8_t operand, uint8_t operand_type);
void xor(uint8_t operand, uint8_t operand_type);
void or(uint8_t operand, uint8_t operand_type);
void ret();
void pop();
void reti();
void jp();
void di();
void ei();
void call();
void push();
void rst(uint8_t opcode);
void rot();
void bit(uint8_t bit, uint8_t source_reg, bool reg_8bit);
void res(uint8_t bit, uint8_t source_reg, bool reg_8bit);
void set(uint8_t bit, uint8_t source_reg, bool reg_8bit);

enum REGISTERS_16BIT {
    AF,
    BC,
    DE,
    HL,
    SP,
    PC
};

enum REGISTERS_8BIT {
    A,
    F,
    B,
    C,
    D,
    E,
    H,
    L
};

enum OPERAND_FORMAT {
    REG_8BIT,
    REG_16BIT,
    CONST_8BIT,
    CONST_16BIT,
    POINTER,
    REG_POINTER,
    OFFSET,
    REG_OFFSET,
};

enum SUBTRACTION_TYPE {
    SUB,
    SBC,
    CP
};

enum LOGIC_TYPE
#endif //GB_EMU_CPU_H
