#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gb.h"
#include "cpu.h"

#define ROM_SIZE 0x8000
static void memory_init(const char* file_name);
static void check_sp();

//we know it works at least until 0xc229
int main(int argc, char *argv[]) {
    memory_init(argv[1]);
    cpu_init();
    while (true) {
        execute_next_instruction();
        check_sp();
    }
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

}


void check_sp() {
    if (MEMORY[0xFF02] == 0x81) {
        char c = MEMORY[0xFF01];

        printf("%c", c);

        MEMORY[0xFF02] = 0;
    }
}