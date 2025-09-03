#include <common.h>
#include <cpu.h>
#include <ppu.h>
#include <min_heap.h>
#include <lcd.h>
#include <queue.h>
#include <memory.h>
#include <gb.h>
#define BANK_SIZE 0x4000
#define TAC_ENABlE(tac) (tac & 0x04)
#define TAC_CLOCK_SELECT(tac) (tac & 0x03)
#define DIV_INCREMENT 256
#define CLOCK_FREQ 4194304.0
#define CYCLES_PER_FRAME 70224
#define FRAME_TIME_MS    (1000.0 * CYCLES_PER_FRAME / CLOCK_FREQ) // ~16.74 ms

static void memory_init(const char* file_name);
static void io_ports_init();
static void check_sp();
static void increment_timers();

static uint16_t timer_internal_counter;
static uint16_t div_internal_counter;
static uint16_t cycles_to_increment_timer;
static bool refresh;


void free_resources() {
    printf("Freeing resources\n");
    free(CPU);
    free(MEMORY);
    free(CARTRIDGE->MEMORY);
    queue_free();
    ppu_free();
    heap_free();
    lcd_free();
}

/*
 * Main function loop
 * Initializes memory and CPU then continually executes instructions
 */
int main(int argc, char* argv[]) {
    memory_init(argv[1]);
    heap_init();
    cpu_init();
    ppu_init();
    queue_init();
    lcd_init();
    uint8_t ppu_cycles = 0;
    refresh = false;
    while (LCD->is_running) {
        uint64_t frame_start = SDL_GetPerformanceCounter();

        while (!refresh) {
            execute_next_PPU_cycle();
            ppu_cycles++;
            if (ppu_cycles == 4) {
                execute_next_CPU_cycle();
                increment_timers();
                check_sp();
                ppu_cycles = 0;
            }
        }
        process_events();
        lcd_update_screen();
        refresh = false;

        //frame limiter
        uint64_t frame_end = SDL_GetPerformanceCounter();
        double elapsed_ms = (double)(frame_end - frame_start) * 1000.0 / (double)SDL_GetPerformanceFrequency();

        if (elapsed_ms < FRAME_TIME_MS) {
            SDL_Delay((uint32_t)(FRAME_TIME_MS - elapsed_ms));
        }
    }
    free_resources();
}

void OAM_DMA() {
    uint16_t source_address = (MEMORY[DMA] << 8) | CPU->DMA_CYCLE;
    MEMORY[0xFE00 | CPU->DMA_CYCLE] = MEMORY[source_address];
    if (CPU->DMA_CYCLE == 0xDF) {
        CPU->STATE = RUNNING;
    }
    else {
        CPU->DMA_CYCLE++;
    }
}

static void increment_timers() {
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
    FILE* gb_game =  fopen(file_name, "rb");

    if (!gb_game) {
        perror("Couldn't open .gb file");
        free(MEMORY);
        exit(1);
    }
    uint8_t header;
    fseek(gb_game, 0x0147, SEEK_SET);
    fread(&header, sizeof(uint8_t), 1, gb_game);
    uint32_t rom_size;
    switch (header) {
        case 0:
            rom_size = BANK_SIZE * 2;
            break;
        case 1:
            rom_size = BANK_SIZE * 4;
            break;
        case 2:
            rom_size = BANK_SIZE * 8;
            break;
        case 3:
            rom_size = BANK_SIZE * 16;
            break;
        default:
            perror("Cartridge not yet supported");
            free(MEMORY);
            exit(1);
    }

    MEMORY = malloc(0x10000);
    CARTRIDGE = malloc(sizeof(CARTRIDGE_STRUCT));
    CARTRIDGE->MEMORY = malloc(rom_size);
    fseek(gb_game, 0, SEEK_SET);
    fread(CARTRIDGE->MEMORY, sizeof(uint8_t), rom_size, gb_game);
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
    MEMORY[STAT] = 0x81;
    MEMORY[SCY] = 0x00;
    MEMORY[SCX] = 0x00;
    MEMORY[LY] = 0x91;
    MEMORY[LYC] = 0x00;
    MEMORY[DMA] = 0xFF;
    MEMORY[BGP] = 0xFC;
    MEMORY[OBP0] = 0x00;
    MEMORY[OBP1] = 0x00;
    MEMORY[WY] = 0x00;
    MEMORY[WX] = 0x00;
    timer_internal_counter = 0;
    div_internal_counter = 0;
    cycles_to_increment_timer = 256;
}

/*
 * Checks the serial transfer control input for if a byte has been written to
 * the serial transfer data input If so, prints the data and resets serial transfer
 * control input
 */
static void check_sp() {
    if (MEMORY[SC] == 0x81) {
        char c = (char) MEMORY[SB];

        printf("%c", c);

        MEMORY[SC] = 0;
    }
}

void set_refresh() {
    refresh = true;
}

void set_tac() {
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
