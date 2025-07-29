#ifndef GB_EMU_QUEUE_H
#define GB_EMU_QUEUE_H

typedef void (*execute_func)(uint8_t);

typedef struct func_and_parm_wrapper {
    uint8_t parameter;
    execute_func func;
} func_and_param_wrapper;

typedef struct func_queue {
    int8_t front;
    int8_t back;
    func_and_param_wrapper* functions;
} func_queue;


typedef struct pixel_queue {
    int8_t front;
    int8_t back;
    uint8_t* pixels;
} pixel_queue;

func_queue* INSTR_QUEUE;
pixel_queue* PIXEL_QUEUE;

void queue_init();
bool is_empty(const func_queue* queue);
void instr_queue_push(execute_func func, uint8_t parameter);
func_and_param_wrapper* instr_queue_pop(func_queue* queue);
#endif //GB_EMU_QUEUE_H
