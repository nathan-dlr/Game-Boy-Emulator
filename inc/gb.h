#ifndef GB_EMU_GB_H
#define GB_EMU_GB_H

#define UNUSED_VAL 0x00

uint8_t* MEMORY;
unsigned long long CYCLE_COUNT;

void read_memory(uint8_t UNUSED);
void write_memory(uint8_t UNUSED);
void free_resources();
void OAM_DMA();

#endif //GB_EMU_GB_H
