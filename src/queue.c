#include <stdlib.h>
#include <ppu.h>
#include <queue.h>
#include <min_heap.h>

#define QUEUE_CAPACITY 10
#define PIXEL_FIFO_CAPACITY 16


void queue_init() {
    INSTR_QUEUE->functions = (func_and_param_wrapper*)calloc(QUEUE_CAPACITY, sizeof(func_and_param_wrapper));
    INSTR_QUEUE->front = -1;
    INSTR_QUEUE->back = -1;

    PIXEL_FIFO->pixel_data = (PIXEL_DATA*)calloc(PIXEL_FIFO_CAPACITY, sizeof(PIXEL_DATA));
    PIXEL_FIFO->front = -1;
    PIXEL_FIFO->back = -1;
    PIXEL_FIFO->size = 0;
}

bool is_empty(const func_queue* queue) {
    return queue->front == -1;
}

uint8_t pixel_fifo_clear() {
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

void pixel_fifo_push(PIXEL_DATA* pixel_data) {
    for (uint8_t i = 0; i < 7; i++) {
        if (PIXEL_FIFO->front == -1) {
            PIXEL_FIFO->front = 0;
        }
        PIXEL_FIFO->back = (PIXEL_FIFO->back + 1) % PIXEL_FIFO_CAPACITY;
        PIXEL_FIFO->pixel_data[PIXEL_FIFO->back].binary_data = pixel_data[i].binary_data;
        PIXEL_FIFO->pixel_data[PIXEL_FIFO->back].source = pixel_data[i].source;
        PIXEL_FIFO->size++;
    }
}

void pixel_fifo_merge_object(PIXEL_DATA* pixel_data) {
    bool transparent;
    bool sprite_in_queue;
    uint8_t palette = heap_peek_palette();
    uint8_t priority = heap_peek_priority();
    if (heap_peek_x_flip()) {
        for (uint8_t i = 0, j = 7; j >=0; i++, j--) {
            transparent = pixel_data[j].binary_data == 0x00;
            sprite_in_queue = PIXEL_FIFO->pixel_data[PIXEL_FIFO->front + i].source == OBJECT;
            if (!(transparent || sprite_in_queue || priority)) {
                PIXEL_FIFO->pixel_data[PIXEL_FIFO->front + i].binary_data = pixel_data[j].binary_data;
                PIXEL_FIFO->pixel_data[PIXEL_FIFO->front + i].source = pixel_data[j].source;
                PIXEL_FIFO->pixel_data[PIXEL_FIFO->front + i].palette = palette;
            }
        }
    }
    else {
        for (uint8_t i = 0; i < 8; i++) {
            transparent = pixel_data[i].binary_data == 0x00;
            sprite_in_queue = PIXEL_FIFO->pixel_data[PIXEL_FIFO->front + i].source == OBJECT;
            if (!(transparent || sprite_in_queue || priority)) {
                PIXEL_FIFO->pixel_data[PIXEL_FIFO->front + i].binary_data = pixel_data[i].binary_data;
                PIXEL_FIFO->pixel_data[PIXEL_FIFO->front + i].source = pixel_data[i].source;
                PIXEL_FIFO->pixel_data[PIXEL_FIFO->front + i].palette = palette;
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

PIXEL_DATA pixel_fifo_pop() {
    PIXEL_DATA pixel_data = PIXEL_FIFO->pixel_data[PIXEL_FIFO->back];
    if (PIXEL_FIFO->front == PIXEL_FIFO->back) {
        PIXEL_FIFO->front = PIXEL_FIFO->back = -1;
    }
    else {
        PIXEL_FIFO->front = (PIXEL_FIFO->front + 1) % PIXEL_FIFO_CAPACITY;
    }

    PIXEL_FIFO->size--;
    return pixel_data;
}

uint8_t pixel_fifo_size() {
    return PIXEL_FIFO->size;
}