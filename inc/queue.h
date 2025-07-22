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

typedef struct object_queue {
    int8_t front;
    int8_t back;
    struct OAM_STRUCT* objects;
} object_queue;

func_queue* INSTR_QUEUE;
object_queue* OBJ_QUEUE;

void queue_init(func_queue* queue);
bool is_empty(const func_queue* queue);
void instr_queue_push(execute_func func, uint8_t parameter);
void object_queue_push(struct OAM_STRUCT* current_object);
func_and_param_wrapper* queue_pop(func_queue* queue);
#endif //GB_EMU_QUEUE_H
