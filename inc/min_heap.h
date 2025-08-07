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
OAM_STRUCT* heap_peek();
void heap_delete_min();
void heapify_up(uint8_t index);
void heapify_down();
void heap_clear();


#endif //GB_EMU_MIN_HEAP_H
