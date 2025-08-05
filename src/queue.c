#include <stdlib.h>
#include <ppu.h>
#include <queue.h>
#include <min_heap.h>

#define QUEUE_CAPACITY 10
#define PIXEL_FIFO_CAPACITY 16


void queue_init() {
    INSTR_QUEUE = (func_queue*) malloc(sizeof(func_queue));
    INSTR_QUEUE->functions = (func_and_param_wrapper*) calloc(QUEUE_CAPACITY, sizeof(func_and_param_wrapper));
    INSTR_QUEUE->front = -1;
    INSTR_QUEUE->back = -1;

    PIXEL_FIFO = (pixel_queue*) malloc(sizeof(pixel_queue));
    PIXEL_FIFO->pixel_data = (PIXEL_DATA**)malloc(PIXEL_FIFO_CAPACITY * sizeof(PIXEL_DATA*));
    for (int i = 0; i < PIXEL_FIFO_CAPACITY; i++) {
        PIXEL_FIFO->pixel_data[i] = (PIXEL_DATA*)calloc(1, sizeof(PIXEL_DATA));
    }
    PIXEL_FIFO->front = -1;
    PIXEL_FIFO->back = -1;
    PIXEL_FIFO->size = 0;
}

void queue_free() {
    free(INSTR_QUEUE->functions);
    free(INSTR_QUEUE);
    free(PIXEL_FIFO->pixel_data);
    free(PIXEL_FIFO);
}

bool is_empty(const func_queue* queue) {
    return queue->front == -1;
}

void pixel_fifo_clear() {
    PIXEL_FIFO->front = -1;
    PIXEL_FIFO->back = -1;
    PIXEL_FIFO->size = 0;
}

void instr_queue_push(execute_func func, uint8_t parameter) {
    if (INSTR_QUEUE->front == -1) {
        INSTR_QUEUE->front = 0;
    }
    INSTR_QUEUE->back = (INSTR_QUEUE->back + 1) % QUEUE_CAPACITY;
    INSTR_QUEUE->functions[INSTR_QUEUE->back].func = func;
    INSTR_QUEUE->functions[INSTR_QUEUE->back].parameter = parameter;
}

void pixel_fifo_push(const PIXEL_DATA* pixel_data) {
    for (uint8_t i = 0; i < 7; i++) {
        if (PIXEL_FIFO->front == -1) {
            PIXEL_FIFO->front = 0;
        }
        PIXEL_FIFO->back = (PIXEL_FIFO->back + 1) % PIXEL_FIFO_CAPACITY;
        PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->binary_data = pixel_data[i].binary_data;
        PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->source = pixel_data[i].source;
        PIXEL_FIFO->size++;
    }
}

void pixel_fifo_merge_object(const PIXEL_DATA* pixel_data) {
    bool transparent;
    bool sprite_in_queue;
    uint8_t index;
    uint8_t palette = heap_peek_palette();
    uint8_t priority = heap_peek_priority();
    if (heap_peek_x_flip()) {
        for (int8_t i = 0, j = 7; j >=0; i++, j--) {
            index = (PIXEL_FIFO->front + i) % PIXEL_FIFO_CAPACITY;
            transparent = pixel_data[j].binary_data == 0x00;
            sprite_in_queue = PIXEL_FIFO->pixel_data[index]->source == OBJECT;
            if (!(transparent || sprite_in_queue || priority)) {
                PIXEL_FIFO->pixel_data[index]->binary_data = pixel_data[j].binary_data;
                PIXEL_FIFO->pixel_data[index]->source = pixel_data[j].source;
                PIXEL_FIFO->pixel_data[index]->palette = palette;
            }
        }
    }
    else {
        for (uint8_t i = 0; i < 8; i++) {
            index = (PIXEL_FIFO->front + i) % PIXEL_FIFO_CAPACITY;
            transparent = pixel_data[i].binary_data == 0x00;
            sprite_in_queue = PIXEL_FIFO->pixel_data[index]->source == OBJECT;
            if (!(transparent || sprite_in_queue || priority)) {
                PIXEL_FIFO->pixel_data[index]->binary_data = pixel_data[i].binary_data;
                PIXEL_FIFO->pixel_data[index]->source = pixel_data[i].source;
                PIXEL_FIFO->pixel_data[index]->palette = palette;
            }
        }
    }
}

func_and_param_wrapper* instr_queue_pop() {
    func_and_param_wrapper* data = &INSTR_QUEUE->functions[INSTR_QUEUE->front];
    if (INSTR_QUEUE->front == INSTR_QUEUE->back) {
        INSTR_QUEUE->front = INSTR_QUEUE->back = -1;
    }
    else {
        INSTR_QUEUE->front = (INSTR_QUEUE->front + 1) % QUEUE_CAPACITY;
    }
    return data;
}

void pixel_fifo_pop(PIXEL_DATA* ret) {
    ret->binary_data = PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->binary_data;
    ret->source = PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->source;
    ret->palette = PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->palette;

    if (PIXEL_FIFO->front == PIXEL_FIFO->back) {
        PIXEL_FIFO->front = PIXEL_FIFO->back = -1;
    }
    else {
        PIXEL_FIFO->front = (PIXEL_FIFO->front + 1) % PIXEL_FIFO_CAPACITY;
    }

    PIXEL_FIFO->size--;
}

uint8_t pixel_fifo_size() {
    return PIXEL_FIFO->size;
}