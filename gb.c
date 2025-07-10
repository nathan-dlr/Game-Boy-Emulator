#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gb.h"
#include "cpu.h"
#include "queue.h"

#define ROM_SIZE 0x8000
#define TAC_ENABlE(tac) (tac & 0x04)
#define TAC_CLOCK_SELECT(tac) (tac & 0x03)
#define DIV_INCREMENT 256

static void memory_init(const char* file_name);
static void io_ports_init();
static void check_sp();
static void increment_timers();

static uint16_t timer_internal_counter;
static uint16_t div_internal_counter;
static uint16_t cycles_to_increment_timer;


void free_resources() {
    printf("Freeing resources");
    free(CPU);
    free(INSTR_QUEUE->functions);
    free(INSTR_QUEUE);
    free(MEMORY);
    exit(0);
}
/*
 * Main function loop
 * Initializes memory and CPU then continually executes instructions
 */
int main(int argc, char *argv[]) {
    memory_init(argv[1]);
    cpu_init();
    timer_internal_counter = 0;
    div_internal_counter = 0;
    cycles_to_increment_timer = 256;
    while (true) {
        execute_next_cycle();
        increment_timers();
        check_sp();
    }
}

/*
 * Reads byte at CPU->ADDRESS_BUS onto CPU->DATA_BUS
 */
void read_memory(uint8_t UNUSED) {
    (void)UNUSED;
    //for debugging with gameboy doctor
    if (CPU->ADDRESS_BUS == LY) {
        CPU->DATA_BUS = 0x90;
        return;
    }
    CPU->DATA_BUS = MEMORY[CPU->ADDRESS_BUS];
}

/*
 * Writes byte in CPU->DATA_BUS into the memory location
 * pointed to by CPU->ADDRESS_BUS
 */
void write_memory(uint8_t UNUSED) {
    (void)UNUSED;
    MEMORY[CPU->ADDRESS_BUS] = CPU->DATA_BUS;
    if (CPU->ADDRESS_BUS == TAC) {
        switch (TAC_CLOCK_SELECT(MEMORY[TAC])) {
            case 0:
                cycles_to_increment_timer = 256;
                break;
            case 1:
                cycles_to_increment_timer = 4;
                break;
            case 2:
                cycles_to_increment_timer = 16;
                break;
            case 3:
                cycles_to_increment_timer = 64;
                break;
            default:
                perror("Error in write memory");
        }
    }
    if (CPU->ADDRESS_BUS == DIV) {
        MEMORY[DIV] = 0x00;
    }
}

void increment_timers() {
    div_internal_counter++;
    if (div_internal_counter >= DIV_INCREMENT) {
        div_internal_counter -= DIV_INCREMENT;
        MEMORY[DIV]++;
    }
    if (TAC_ENABlE(MEMORY[TAC])) {
        timer_internal_counter++;
        while (timer_internal_counter >= cycles_to_increment_timer) {
            timer_internal_counter -= cycles_to_increment_timer;
            if (MEMORY[TIMA] != 0xFF) {
                MEMORY[TIMA]++;
            } else {
                MEMORY[TIMA] = MEMORY[TMA];
                MEMORY[IF] |= 0x04;
            }
        }
    }
}

/*
 * Opens .gb file and reads it into memory
 */
static void memory_init(const char* file_name) {
    MEMORY = malloc(0xFFFF);
    const FILE* gb_game =  fopen(file_name, "rb");
    if (gb_game != NULL) {
        fread(MEMORY, sizeof(uint8_t), ROM_SIZE, gb_game);
        if (ferror(gb_game) != 0) {
            fputs("Error reading file", stderr);
        }
    }
    else {
        perror("Couldn't open .gb file");
        exit(1);
    }
    fclose(gb_game);
    io_ports_init();
}

/*
 * Initializes IO Ports
 */
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

/*
 * Checks the serial transfer control input for if a byte has been written to
 * the serial transfer data input If so, prints the data and resets serial transfer
 * control input
 */
static void check_sp() {
    if (MEMORY[SC] == 0x81) {
        char c = MEMORY[SB];

        printf("%c", c);

        MEMORY[SC] = 0;
    }
}
