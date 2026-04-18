// Defines CPU stop reasons, letting the OS differentiate between a normal exit or other.
#ifndef TRAP_H
#define TRAP_H

enum class Trap {
    None,
    Exit,
    QuantumExpired,
    Sleep,
    Input,
    Inputc,
    MapSharedMem,
    AcquireLock,
    ReleaseLock,
    SignalEvent,
    WaitEvent,
    Alloc,       // Module 5: process issued Alloc rx, ry
    FreeMemory,  // Module 5: process issued FreeMemory rx
};

#endif