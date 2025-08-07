#include <stdlib.h>
#include <stdio.h>
#include <common.h>
#include <ppu.h>
#include <min_heap.h>
#include <queue.h>

#define QUEUE_CAPACITY 10
#define PIXEL_FIFO_CAPACITY 8


void queue_init() {
    INSTR_QUEUE = (func_queue*) malloc(sizeof(func_queue));
    INSTR_QUEUE->functions = (func_and_param_wrapper*) calloc(QUEUE_CAPACITY, sizeof(func_and_param_wrapper));
    INSTR_QUEUE->front = -1;
    INSTR_QUEUE->back = -1;

    PPU->BACKGROUND_FIFO = (PIXEL_FIFO*) malloc(sizeof(PIXEL_FIFO));
    PPU->BACKGROUND_FIFO->pixel_data = (PIXEL_DATA**)malloc(PIXEL_FIFO_CAPACITY * sizeof(PIXEL_DATA*));
    for (int i = 0; i < PIXEL_FIFO_CAPACITY; i++) {
        PPU->BACKGROUND_FIFO->pixel_data[i] = (PIXEL_DATA*)calloc(1, sizeof(PIXEL_DATA));
    }
    PPU->BACKGROUND_FIFO->front = -1;
    PPU->BACKGROUND_FIFO->back = -1;
    PPU->BACKGROUND_FIFO->size = 0;

    PPU->SPRITE_FIFO = (PIXEL_FIFO*) malloc(sizeof(PIXEL_FIFO));
    PPU->SPRITE_FIFO->pixel_data = (PIXEL_DATA**)malloc(PIXEL_FIFO_CAPACITY * sizeof(PIXEL_DATA*));
    for (int i = 0; i < PIXEL_FIFO_CAPACITY; i++) {
        PPU->SPRITE_FIFO->pixel_data[i] = (PIXEL_DATA*)calloc(1, sizeof(PIXEL_DATA));
    }
    PPU->SPRITE_FIFO->front = -1;
    PPU->SPRITE_FIFO->back = -1;
    PPU->SPRITE_FIFO->size = 0;
}

void queue_free() {
    free(INSTR_QUEUE->functions);
    free(INSTR_QUEUE);
    free(PPU->BACKGROUND_FIFO->pixel_data);
    free(PPU->BACKGROUND_FIFO);
    free(PPU->SPRITE_FIFO->pixel_data);
    free(PPU->SPRITE_FIFO);
}

bool is_empty(const func_queue* queue) {
    return queue->front == -1;
}

void instr_queue_push(execute_func func, uint8_t parameter) {
    if (INSTR_QUEUE->front == -1) {
        INSTR_QUEUE->front = 0;
    }
    INSTR_QUEUE->back = (INSTR_QUEUE->back + 1) % QUEUE_CAPACITY;
    INSTR_QUEUE->functions[INSTR_QUEUE->back].func = func;
    INSTR_QUEUE->functions[INSTR_QUEUE->back].parameter = parameter;
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

void pixel_fifo_clear(PIXEL_FIFO* PIXEL_FIFO) {
    PIXEL_FIFO->front = -1;
    PIXEL_FIFO->back = -1;
    PIXEL_FIFO->size = 0;
}

void background_fifo_push(const PIXEL_DATA* pixel_data) {
    PIXEL_FIFO* PIXEL_FIFO = PPU->BACKGROUND_FIFO;
    for (uint8_t i = 0; i < 8; i++) {
        if (PIXEL_FIFO->front == -1) {
            PIXEL_FIFO->front = 0;
        }
        PIXEL_FIFO->back = (PIXEL_FIFO->back + 1) % PIXEL_FIFO_CAPACITY;
        PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->binary_data = pixel_data[i].binary_data;
        PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->source = pixel_data[i].source;
        PIXEL_FIFO->size++;
    }
}

void sprite_fifo_push(const PIXEL_DATA* pixel_data) {
    OAM_STRUCT* obj = heap_peek();
    uint8_t index;
    uint8_t palette = obj->palette;
    bool priority = obj->priority;
    PIXEL_FIFO* PIXEL_FIFO = PPU->SPRITE_FIFO;
    if (obj->x_flip) {
        for (int8_t i = 0, j = 7; i < 8; i++, j--) {
            if (PIXEL_FIFO->front == -1) {
                PIXEL_FIFO->front = 0;
            }
            PIXEL_FIFO->back = (PIXEL_FIFO->back + 1) % PIXEL_FIFO_CAPACITY;
            PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->binary_data = pixel_data[j].binary_data;
            PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->source = pixel_data[j].source;
            PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->palette = palette;
            PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->priority = priority;
            PIXEL_FIFO->size++;
        }
    }
    else {
        for (int8_t i = 0; i < 8; i++) {
            if (PIXEL_FIFO->front == -1) {
                PIXEL_FIFO->front = 0;
            }
            PIXEL_FIFO->back = (PIXEL_FIFO->back + 1) % PIXEL_FIFO_CAPACITY;
            PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->binary_data = pixel_data[i].binary_data;
            PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->source = pixel_data[i].source;
            PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->palette = palette;
            PIXEL_FIFO->pixel_data[PIXEL_FIFO->back]->priority = priority;
            PIXEL_FIFO->size++;
        }
    }
}

void pixel_fifo_pop(PIXEL_FIFO* PIXEL_FIFO, PIXEL_DATA* ret) {
    ret->binary_data = PIXEL_FIFO->pixel_data[PIXEL_FIFO->front]->binary_data;
    ret->source = PIXEL_FIFO->pixel_data[PIXEL_FIFO->front]->source;
    ret->palette = PIXEL_FIFO->pixel_data[PIXEL_FIFO->front]->palette;
    ret->priority = PIXEL_FIFO->pixel_data[PIXEL_FIFO->front]->priority;

    PIXEL_FIFO->pixel_data[PIXEL_FIFO->front]->binary_data = 0xFF;

    if (PIXEL_FIFO->front == PIXEL_FIFO->back) {
        PIXEL_FIFO->front = PIXEL_FIFO->back = -1;
    }
    else {
        PIXEL_FIFO->front = (PIXEL_FIFO->front + 1) % PIXEL_FIFO_CAPACITY;
    }

    PIXEL_FIFO->size--;
}

uint8_t pixel_fifo_size(const PIXEL_FIFO* PIXEL_FIFO) {
    return PIXEL_FIFO->size;
}

bool pixel_fifo_is_empty(const PIXEL_FIFO* PIXEL_FIFO) {
    return PIXEL_FIFO->size == 0;
}