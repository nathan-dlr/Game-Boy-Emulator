#include <stdlib.h>
#include "queue.h"
#define QUEUE_CAPACITY 10


void queue_init(func_queue* queue) {
    queue->functions = malloc(QUEUE_CAPACITY * sizeof(func_and_param_wrapper));
    queue->front = -1;
    queue->back = 0;
}

bool is_empty(const func_queue* queue) {
    uint8_t val = queue->back - 1;
    return queue->front == val;
}

void queue_push(func_queue* queue, execute_func func, uint8_t parameter) {
    queue->functions[queue->back].func = func;
    queue->functions[queue->back].parameter = parameter;
    queue->back++;
}

func_and_param_wrapper* queue_pop(func_queue* queue) {
    queue->front++;
    return &queue->functions[queue->front];
}

