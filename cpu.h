#ifndef GB_EMU_CPU_H
#define GB_EMU_CPU_H

void cpu_init();
void nop(uint8_t opcode);
void stop();
void ld();
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
void add();
void adc();
void sub();
void sbc();
void and();
void xor();
void or();
void cp();
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

#endif //GB_EMU_CPU_H
