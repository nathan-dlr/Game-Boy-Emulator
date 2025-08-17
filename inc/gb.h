#ifndef GB_EMU_GB_H
#define GB_EMU_GB_H

uint8_t* MEMORY;
unsigned long long CYCLE_COUNT;

void read_memory(uint8_t UNUSED);
void write_memory(uint8_t UNUSED);
void free_resources();
void OAM_DMA();
void set_refresh();

#endif //GB_EMU_GB_H
