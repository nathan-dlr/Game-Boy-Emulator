#ifndef GB_EMU_CPU_H
#define GB_EMU_CPU_H

void cpu_init();
uint8_t fetch_byte();
uint16_t fetch_word();

void nop(uint8_t opcode);
void stop();
void ld(uint16_t dest, uint16_t source, uint8_t dest_type, uint8_t source_type);
void jr();
void inc(uint8_t opcode);
void dec(uint8_t opcode);
void rlca();
void rrca();
void rla();
void rra();
void daa();
void cpl();
void scf();
void ccf();
void halt();
void add(uint16_t operand, uint8_t operand_type);
void adc(uint8_t operand, uint8_t operand_type);
void sub(uint8_t operand, uint8_t operand_type);
void sbc(uint8_t operand, uint8_t operand_type);
void and();
void xor();
void or();
void cp(uint8_t operand, uint8_t operand_type);
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
void bit();
void res();
void set();

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

#endif //GB_EMU_CPU_H
