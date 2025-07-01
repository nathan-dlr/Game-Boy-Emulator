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

enum OPERAND_FORMAT {
    REG_8BIT,
    REG_16BIT,
    IMM_8BIT,
    IMM_16BIT,
    POINTER,
    REG_POINTER,
    OFFSET,
    REG_OFFSET,
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
    HALTED
};

typedef struct CPU_STRUCT {
    uint16_t REGS[14];
    uint8_t IME;
    uint8_t DATA_BUS;
    uint16_t ADDRESS_BUS;
    enum CPU_STATES STATE;
} CPU_STRUCT;

static CPU_STRUCT CPU;
void cpu_init();
void execute_next_instruction();

void read_next_byte();
uint16_t read_16bit_reg(uint8_t reg_pair);
void write_16bit_reg(uint8_t reg_pair, uint16_t value);

//Loads
void ld_r8_imm8(uint8_t dest);
void ld_rW_imm8(uint8_t load_a);
void ld_r8_addr_bus(uint8_t dest);
void ld_r8_data_bus(uint8_t dest);
void ldh_imm8();
void ld_imm16_sp(uint8_t byte_num);
void ld_hl_sp8();
void ld_sp_hl();
//8-bit arithmetic
void add_8bit(uint8_t operand_type);
void adc(uint8_t operand_type);
void cp(uint8_t operand_type);
void sub(uint8_t operand_type);
void sbc(uint8_t operand_type);
void inc_8bit(uint8_t dest);
void dec_8bit(uint8_t dest);
//16-bit arithmetic
void add_16bit(uint8_t source);
void add_sp_e8(uint8_t cycle);
void inc_16bit(uint8_t dest);
void dec_16bit(uint8_t dest);
//8-bit logic
void and(uint8_t operand_type);
void or(uint8_t operand_type);
void xor(uint8_t operand_type);
void cpl();
//Bit flags instructions
void bit(uint8_t bit_num);
void res(uint8_t opcode);
void set(uint8_t opcode);
//Bit shift instructions
void rl(uint8_t source_reg);
void rla();
void rlc(uint8_t source_reg);
void rlca();
void rr(uint8_t source_reg);
void rra();
void rrc(uint8_t source_reg);
void rrca();
void sla(uint8_t source_reg);
void sra(uint8_t source_reg);
void srl(uint8_t source_reg);
void swap(uint8_t source_reg);
//Jumps and subroutine instructions
void call_cycle3(uint8_t cc);
void call_writes(uint8_t cycle_num);
void jp_cycle3(uint8_t cc);
void jp(uint8_t is_hl);
void jr_cycle2(uint8_t cc);
void jr();
void ret_eval_cc(uint8_t cc);
void ret(uint8_t is_reti);
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
