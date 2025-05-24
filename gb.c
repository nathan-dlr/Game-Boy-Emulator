#include <stdlib.h>
#include "gb.h"
#include "cpu.h"

int main() {
    MEMORY = malloc(0xFFFF);
    cpu_init();
}
