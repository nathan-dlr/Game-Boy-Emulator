#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gb.h"
#include "cpu.h"

#define ROM_SIZE 0x8000
static void memory_init(const char* file_name);

int main(int argc, char *argv[]) {
    MEMORY = malloc(0xFFFF);
    memory_init(argv[0]);
    cpu_init();
}

static void memory_init(const char* file_name) {
    const FILE* gb_game =  fopen(file_name, "rb");
    memcpy(MEMORY, gb_game, ROM_SIZE);

}