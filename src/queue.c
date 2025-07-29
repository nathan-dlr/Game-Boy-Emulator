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

    PIXEL_FIFO->pixels = (uint8_t*)calloc(PIXEL_FIFO_CAPACITY, sizeof(uint8_t));
    PIXEL_FIFO->front = -1;
    PIXEL_FIFO->back = -1;
    PIXEL_FIFO->size = 0;
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

void pixel_fifo_push(uint8_t pixel) {
    if (PIXEL_FIFO->front == -1) {
        PIXEL_FIFO->front = 0;
    }
    PIXEL_FIFO->back = (PIXEL_FIFO->back + 1) % PIXEL_FIFO_CAPACITY;
    PIXEL_FIFO->pixels[PIXEL_FIFO->back] = pixel;
    PIXEL_FIFO->size++;
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

uint8_t pixel_fifo_pop() {
    uint8_t pixel = PIXEL_FIFO->pixels[PIXEL_FIFO->back];
    if (PIXEL_FIFO->front == PIXEL_FIFO->back) {
        PIXEL_FIFO->front = PIXEL_FIFO->back = -1;
    }
    else {
        PIXEL_FIFO->front = (PIXEL_FIFO->front + 1) % PIXEL_FIFO_CAPACITY;
    }

    PIXEL_FIFO->size--;
    return pixel;
}

uint8_t pixel_fifo_size() {
    if (PIXEL_FIFO->front == -1) {
        return 0;
    }

}