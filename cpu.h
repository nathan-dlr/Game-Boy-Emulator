#ifndef GB_EMU_CPU_H
#define GB_EMU_CPU_H

#define P1 0xFF00
#define SB 0xFF01
#define SC 0xFF02
#define DIV 0xFF04
#define TIMA 0xFF05
#define TMA 0xFF06
#define TAC 0xFF07
#define IF 0xFF0F
#define NR10 0xFF10
#define NR11 0xFF11
#define NR12 0xFF12
#define NR13 0xFF13
#define NR14 0xFF14
#define NR21 0xFF16
#define NR22 0xFF17
#define NR23 0xFF18
#define NR24 0xFF19
#define NR30 0xFF1A
#define NR31 0xFF1B
#define NR32 0xFF1C
#define NR33 0xFF1D
#define NR34 0xFF1E
#define NR41 0xFF20
#define NR42 0xFF21
#define NR43 0xFF22
#define NR44 0xFF23
#define NR50 0xFF24
#define NR51 0xFF25
#define NR52 0xFF26
#define LCDC 0xFF40
#define STAT 0xFF41
#define SCY 0xFF42
#define SCX 0xFF43
#define LY 0xFF44
#define LYC 0xFF45
#define DMA 0xFF46
#define BGP 0xFF47
#define OBP0 0xFF48
#define OBP1 0xFF49
#define WY 0xFF4A
#define WX 0xFF4B
#define KEY1 0xFF4D
#define VBK 0xFF4f
#define HDMA1 0xFF51
#define HDMA2 0xFF52
#define HDMA3 0xFF53
#define HDMA4 0xFF54
#define HDMA5 0xFF55
#define RP 0xFF56
#define BCPs 0cFF68
#define BCPD 0xFF69
#define OCPS 0xFF6A
#define OCPD 0xFF6B
#define SVBK 0xFF70
#define IE 0xFFFF


uint16_t get_debug_pc();
void cpu_init();
uint8_t fetch_byte();
uint16_t fetch_word();
void execute_next_instruction();

void nop(uint8_t opcode);
void stop();
void ld(uint16_t dest, uint16_t source, uint8_t dest_type, uint8_t source_type);
void ld_inc(uint8_t action);
void ld_sp_off(int8_t offset);
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
void call(uint8_t cc, uint16_t address);
void jp(uint8_t cc, bool is_hl);
void jr(uint8_t cc);
void ret(uint8_t cc);
void reti();
void di();
void ei();
void pop(uint8_t reg_16);
void push(uint8_t reg_16);
void rst(uint8_t opcode);
void bit(uint8_t bit_num, uint8_t source_reg, bool reg_8bit);
void res(uint8_t bit_num, uint8_t source_reg, bool reg_8bit);
void set(uint8_t bit_num, uint8_t source_reg, bool reg_8bit);

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

enum CC {
    NZ,
    Z,
    NC,
    CARRY,
    NONE
};

enum ACTION {
    DEST_INC,
    DEST_DEC,
    SOURCE_INC,
    SOURCE_DEC
};

enum CPU_STATES {
    RUNNING,
    HALTED
};
#endif //GB_EMU_CPU_H
