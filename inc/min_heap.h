#ifndef GB_EMU_MIN_HEAP_H
#define GB_EMU_MIN_HEAP_H

typedef struct object_min_heap {
    int8_t size;
    int8_t capacity;
    struct OAM_STRUCT** objects;
} object_min_heap;

object_min_heap* OBJ_HEAP;

void heap_init();
void heap_insert(OAM_STRUCT* object);
void peek_min(OAM_STRUCT* ret);
void extract_min(OAM_STRUCT* ret);
void heapify_up(uint8_t index);
void heapify_down();


#endif //GB_EMU_MIN_HEAP_H
