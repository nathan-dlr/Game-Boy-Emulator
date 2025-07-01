#include "queue.h"


void queue_init(func_queue* queue) {
    queue->front = -1;
    queue->back = 0;
}

bool is_empty(func_queue* queue) {
    return queue->front == queue->back - 1;
}

void push(func_queue* queue, execute_func func, uint8_t parameter) {
    queue->functions[queue->back].func = func;
    queue->functions[queue->back].parameter = parameter;
    queue->back++;
}

func_and_param_wrapper* pop(func_queue* queue) {
    queue->front++;
    return &queue->functions[queue->front];
}

