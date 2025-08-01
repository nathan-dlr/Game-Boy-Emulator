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
    uint8_t size;
    PIXEL_DATA** pixel_data;
} pixel_queue;

func_queue* INSTR_QUEUE;
pixel_queue* PIXEL_FIFO;

void queue_init();
void queue_free();
bool is_empty(const func_queue* queue);
void pixel_fifo_clear();
void instr_queue_push(execute_func func, uint8_t parameter);
void pixel_fifo_push(const PIXEL_DATA* pixel_data);
void pixel_fifo_merge_object(const PIXEL_DATA* pixel_data);
func_and_param_wrapper* instr_queue_pop();
void pixel_fifo_pop(PIXEL_DATA* ret);
uint8_t pixel_fifo_size();
#endif //GB_EMU_QUEUE_H
