#ifndef TRAP_H
#define TRAP_H

#include <cstdint>

enum class TrapType { None, Exit, Sleep, IOBlock };

struct Trap {
    TrapType type = TrapType::None;
    uint32_t value = 0;  // e.g. sleep cycles
};

#endif // TRAP_H