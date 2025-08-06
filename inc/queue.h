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

func_queue* INSTR_QUEUE;


void queue_init();
void queue_free();
bool is_empty(const func_queue* queue);
void pixel_fifo_clear(PIXEL_FIFO* PIXEL_FIFO);
void instr_queue_push(execute_func func, uint8_t parameter);
void background_fifo_push(const PIXEL_DATA* pixel_data);
void sprite_fifo_push(const PIXEL_DATA* pixel_data);
func_and_param_wrapper* instr_queue_pop();
void pixel_fifo_pop(PIXEL_FIFO* PIXEL_FIFO, PIXEL_DATA* ret);
uint8_t pixel_fifo_size(PIXEL_FIFO* PIXEL_FIFO);
bool pixel_fifo_is_empty(PIXEL_FIFO* PIXEL_FIFO);
#endif //GB_EMU_QUEUE_H
