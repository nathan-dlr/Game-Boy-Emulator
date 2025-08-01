#ifndef GB_EMU_MIN_HEAP_H
#define GB_EMU_MIN_HEAP_H

typedef struct object_min_heap {
    int8_t size;
    int8_t capacity;
    struct OAM_STRUCT** objects;
} object_min_heap;

object_min_heap* OBJ_HEAP;

void heap_init();
void heap_free();
void heap_insert(const OAM_STRUCT* object);
uint8_t heap_peek_x_pos();
uint8_t heap_peek_tile_num();
bool heap_peek_x_flip();
enum PALETTE heap_peek_palette();
bool heap_peek_priority();
void heap_delete_min();
void heapify_up(uint8_t index);
void heapify_down();
void heap_clear();


#endif //GB_EMU_MIN_HEAP_H
