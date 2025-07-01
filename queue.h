#ifndef GB_EMU_QUEUE_H
#define GB_EMU_QUEUE_H

#include <stdint.h>

typedef void (*execute_func)(uint8_t);

typedef struct func_and_parm_wrapper {
    uint8_t parameter;
    execute_func func;
} func_and_param_wrapper;

typedef struct func_queue {
    uint8_t front;
    uint8_t back;
    func_and_param_wrapper functions[15];
} func_queue;

func_queue* INSTR_QUEUE;

void queue_init(func_queue* queue);
bool is_empty(func_queue* queue);
void push(func_queue* queue, execute_func func, uint8_t parameter);
func_and_param_wrapper* pop(func_queue* queue);
#endif //GB_EMU_QUEUE_H
