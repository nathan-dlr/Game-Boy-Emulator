#include <stdlib.h>
#include <queue.h>
#define QUEUE_CAPACITY 10


void queue_init(func_queue* queue) {
    queue->functions = malloc(QUEUE_CAPACITY * sizeof(func_and_param_wrapper));
    queue->front = -1;
    queue->back = -1;
}

bool is_empty(const func_queue* queue) {
    return queue->front == -1;
}

void queue_push(func_queue* queue, execute_func func, uint8_t parameter) {
    if (queue->front == -1) {
        queue->front = 0;
    }
    queue->back = (queue->back + 1) % QUEUE_CAPACITY;
    queue->functions[queue->back].func = func;
    queue->functions[queue->back].parameter = parameter;
}

func_and_param_wrapper* queue_pop(func_queue* queue) {
    func_and_param_wrapper* data = &queue->functions[queue->front];
    if (queue->front == queue->back) {
        queue->front = queue->back = -1;
    }
    else {
        queue->front = (queue->front + 1) % QUEUE_CAPACITY;
    }
    return data;
}

