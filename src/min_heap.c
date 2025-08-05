#include <stdint.h>
#include <stdlib.h>
#include <ppu.h>
#include <min_heap.h>
#include <stdio.h>

#define OBJ_HEAP_CAPACITY 10

void heap_init() {
    OBJ_HEAP = (object_min_heap*) malloc(sizeof(object_min_heap));
    OBJ_HEAP->size = 0;
    OBJ_HEAP->capacity = OBJ_HEAP_CAPACITY;
    OBJ_HEAP->objects = (OAM_STRUCT**) malloc(OBJ_HEAP_CAPACITY * sizeof(OAM_STRUCT*));

    for (int i = 0; i < OBJ_HEAP_CAPACITY; i++) {
        OBJ_HEAP->objects[i] = (OAM_STRUCT*) calloc(1, sizeof(OAM_STRUCT));

    }
}

void heap_free() {
    for (int i = 0; i < OBJ_HEAP_CAPACITY; i++) {
        if (OBJ_HEAP->objects[i]) {
            free(OBJ_HEAP->objects[i]);
        }
    }
    free(OBJ_HEAP->objects);
    free(OBJ_HEAP);
}

static void copy_object(OAM_STRUCT* dest, const OAM_STRUCT* src) {
    dest->y_pos = src->y_pos;
    dest->x_pos = src->x_pos;
    dest->tile_index = src->tile_index;
    dest->priority = src->priority;
    dest->y_flip = src->y_flip;
    dest->x_flip = src->x_flip;
    dest->palette = src->palette;
}

 void heap_insert(const OAM_STRUCT* object) {
    if (OBJ_HEAP->size < OBJ_HEAP->capacity) {
        copy_object(OBJ_HEAP->objects[OBJ_HEAP->size], object);
        heapify_up(OBJ_HEAP->size);
        OBJ_HEAP->size++;
    }
}

uint8_t heap_peek_x_pos() {
    if (OBJ_HEAP->size == 0) {
        return -1;
    }
    else {
        return OBJ_HEAP->objects[0]->x_pos;
    }
}

uint8_t heap_peek_tile_num() {
    if (OBJ_HEAP->size == 0) {
        return -1;
    }
    else {
        return OBJ_HEAP->objects[0]->tile_index;
    }
}

bool heap_peek_x_flip() {
    if (OBJ_HEAP->size == 0) {
        return false;
    }
    else {
        return OBJ_HEAP->objects[0]->x_flip;
    }
}

bool heap_peek_priority() {
    if (OBJ_HEAP->size == 0) {
        return false;
    }
    else {
        return OBJ_HEAP->objects[0]->priority;
    }
}


enum PALETTE heap_peek_palette() {
    return OBJ_HEAP->objects[0]->palette;
};

void heap_delete_min() {
    if (OBJ_HEAP->size == 0) return;
    copy_object(OBJ_HEAP->objects[0], OBJ_HEAP->objects[OBJ_HEAP->size - 1]);
    OBJ_HEAP->size--;
    heapify_down();
}

static bool is_higher_priority(const OAM_STRUCT* a, const OAM_STRUCT* b) {
    if (a->x_pos < b->x_pos) return true;
    if (a->x_pos == b->x_pos && a->address < b->address) return true;
    return false;
}

void heapify_up(uint8_t index) {
    if (index == 0) return;
    uint8_t parent = (index - 1) / 2;
    OAM_STRUCT* temp = NULL;
    while (index > 0 && is_higher_priority(OBJ_HEAP->objects[index], OBJ_HEAP->objects[parent])) {
        temp = OBJ_HEAP->objects[parent];
        OBJ_HEAP->objects[parent] = OBJ_HEAP->objects[index];
        OBJ_HEAP->objects[index] = temp;

        index = parent;
        parent = (index - 1) / 2;
    }
}

static int8_t compare_nodes(int8_t current, int8_t left, int8_t right) {
    uint8_t min = current;
    if (left >= OBJ_HEAP->size) {
        left = -1;
    }
    if (right >= OBJ_HEAP->size) {
        right = -1;
    }

    if ((left != -1) && (is_higher_priority(OBJ_HEAP->objects[left], OBJ_HEAP->objects[min]))) {
        min = left;
    }
    if ((right != -1) && (is_higher_priority(OBJ_HEAP->objects[right], OBJ_HEAP->objects[min]))) {
        min = right;
    }
    return min;
}

void heapify_down() {
    OAM_STRUCT* temp = NULL;
    int8_t left = 1;
    int8_t right = 2;
    int8_t current = 0;
    int8_t min = compare_nodes(current, left, right);

    while (min != current) {
        temp = OBJ_HEAP->objects[min];
        OBJ_HEAP->objects[min] = OBJ_HEAP->objects[current];
        OBJ_HEAP->objects[current] = temp;

        current = min;
        left = current * 2 + 1;
        right = current * 2 + 2;
        min = compare_nodes(current, left, right);
    }
}

void heap_clear() {
    OBJ_HEAP->size = 0;
}
