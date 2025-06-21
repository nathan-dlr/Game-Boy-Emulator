#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gb.h"
#include "cpu.h"

#define ROM_SIZE 0x8000
static void memory_init(const char* file_name);
static void io_ports_init();
static void check_sp();


//we know it works at least until 0xc018
int main(int argc, char *argv[]) {
    memory_init(argv[1]);
    cpu_init();
    while (true) {
        execute_next_instruction();
        check_sp();
    }
}

uint8_t read_memory(uint16_t address) {
    //for debugging with gameboy doctor
    if (address == LY) {
        return 0x90;
    }
    return MEMORY[address];
}

void write_memory(uint16_t address, uint8_t value) {
    MEMORY[address] = value;
}

static void memory_init(const char* file_name) {
    MEMORY = malloc(0xFFFF);
    const FILE* gb_game =  fopen(file_name, "rb");
    if (gb_game != NULL) {
        fread(MEMORY, sizeof(uint8_t), ROM_SIZE, gb_game);
        if (ferror(gb_game) != 0) {
            fputs("Error reading file", stderr);
        }
    }
    fclose(gb_game);
    io_ports_init();
}

static void io_ports_init() {
    MEMORY[P1] = 0xCF;
    MEMORY[SB] = 0x00;
    MEMORY[SC] = 0x7E;
    MEMORY[DIV] = 0xAB;
    MEMORY[TIMA] = 0x00;
    MEMORY[TMA] = 0x00;
    MEMORY[TAC] = 0xF8;
    MEMORY[IF] = 0xE1;
    MEMORY[NR10] = 0x80;
    MEMORY[NR11] = 0xBF;
    MEMORY[NR12] = 0xF3;
    MEMORY[NR13] = 0xFF;
    MEMORY[NR14] = 0xBF;
    MEMORY[NR21] = 0x3F;
    MEMORY[NR22] = 0x00;
    MEMORY[NR23] = 0xFF;
    MEMORY[NR24] = 0xBF;
    MEMORY[NR30] = 0x7F;
    MEMORY[NR31] = 0xFf;
    MEMORY[NR32] = 0x9F;
    MEMORY[NR33] = 0xFF;
    MEMORY[NR34] = 0xBF;
    MEMORY[NR41] = 0xFF;
    MEMORY[NR42] = 0x00;
    MEMORY[NR43] = 0x00;
    MEMORY[NR44] = 0xBF;
    MEMORY[NR50] = 0x77;
    MEMORY[NR51] = 0xF3;
    MEMORY[NR52] = 0xF1;
    MEMORY[LCDC] = 0x91;
    MEMORY[STAT] = 0x85;
    MEMORY[SCY] = 0x00;
    MEMORY[SCX] = 0x00;
    MEMORY[LY] = 0x00;
    MEMORY[LYC] = 0x00;
    MEMORY[DMA] = 0xFF;
    MEMORY[BGP] = 0xFC;
    MEMORY[OBP0] = 0x00;
    MEMORY[OBP1] = 0x00;
    MEMORY[WY] = 0x00;
    MEMORY[WX] = 0x00;
}


void check_sp() {
    if (MEMORY[SC] == 0x81) {
        char c = MEMORY[SB];

        printf("%c", c);

        MEMORY[SC] = 0;
    }
}
