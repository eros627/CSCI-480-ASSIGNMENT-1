#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include "PCB.h"

struct Process {
    PCB pcb;
    std::string name;     // e.g. filename
};

#endif // PROCESS_H