#include <stdlib.h>
#include <queue.h>
#include <ppu.h>
#define QUEUE_CAPACITY 10
#define PIXEL_FIFO_CAPACITY 16

//TODO MAKE WORK WITH OTHER QUEUE'S, maybe take no paramters and just initalize both queues here so this function
//is only called once in gb.c probably
void queue_init() {
    INSTR_QUEUE->functions = (func_and_param_wrapper*)calloc(QUEUE_CAPACITY, sizeof(func_and_param_wrapper));
    INSTR_QUEUE->front = -1;
    INSTR_QUEUE->back = -1;

    PIXEL_QUEUE->pixels = (uint8_t*)calloc(PIXEL_FIFO_CAPACITY, sizeof(uint8_t));
    PIXEL_QUEUE->front = -1;
    PIXEL_QUEUE->back = -1;
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

void pixel_queue_push(uint8_t pixel) {
    if (PIXEL_QUEUE->front == -1) {
        PIXEL_QUEUE->front = 0;
    }
    PIXEL_QUEUE->back = (PIXEL_QUEUE->back + 1) % PIXEL_FIFO_CAPACITY;
    PIXEL_QUEUE->pixels[PIXEL_QUEUE->back] = pixel;
}

func_and_param_wrapper* instr_queue_pop(func_queue* queue) {
    func_and_param_wrapper* data = &queue->functions[queue->front];
    if (queue->front == queue->back) {
        queue->front = queue->back = -1;
    }
    else {
        queue->front = (queue->front + 1) % QUEUE_CAPACITY;
    }
    return data;
}

uint8_t pixel_queue_pop() {
    uint8_t pixel = PIXEL_QUEUE->pixels[PIXEL_QUEUE->back];
    if (PIXEL_QUEUE->front == PIXEL_QUEUE->back) {
        PIXEL_QUEUE->front = PIXEL_QUEUE->back = -1;
    }
    else {
        PIXEL_QUEUE->front = (PIXEL_QUEUE->front + 1) % PIXEL_FIFO_CAPACITY;
    }
    return pixel;
}