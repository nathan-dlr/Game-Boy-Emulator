#include <stdlib.h>
#include <queue.h>
#include <ppu.h>
#define QUEUE_CAPACITY 10
#define PIXEL_FIFO_CAPACITY 16

void queue_init(func_queue* queue) {
    queue->functions = malloc(QUEUE_CAPACITY * sizeof(func_and_param_wrapper));
    queue->front = -1;
    queue->back = -1;
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

static void copy_object(OAM_STRUCT* dest, OAM_STRUCT* src) {
    dest->y_pos = src->y_pos;
    dest->x_pos = src->x_pos;
    dest->tile_index = src->tile_index;
    dest->priority = src->priority;
    dest->y_flip = src->y_flip;
    dest->x_flip = src->x_flip;
    dest->palette = dest->palette;
}

void object_queue_push(OAM_STRUCT* current_object) {
    if (OBJ_QUEUE->front == -1) {
        OBJ_QUEUE->front = 0;
    }
    OBJ_QUEUE->back = (OBJ_QUEUE->back + 1) % QUEUE_CAPACITY;
    copy_object(&OBJ_QUEUE->back, current_object);
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

