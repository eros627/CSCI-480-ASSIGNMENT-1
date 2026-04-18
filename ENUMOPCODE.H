#ifndef ENUMOPCODE_H
#define ENUMOPCODE_H

#include <cstdint>

enum class Opcode : uint32_t {
    // Arithmetic
    Incr = 1, Addi = 2, Addr = 3,
    // Data Movement
    Movi = 4, Movr = 5, Movmr = 6, Movrm = 7, Movmm = 8,
    // Output
    Printr = 9, Printm = 10, Printcr = 11, Printcm = 12,
    // Control Flow
    Jmp = 13, Jmpi = 14, Jmpa = 15,
    Cmpi = 16, Cmpr = 17,
    Jlt = 18, Jlti = 19, Jlta = 20,
    Jgt = 21, Jgti = 22, Jgta = 23,
    Je  = 24, Jei  = 25, Jea  = 26,
    Call = 27, Callm = 28, Ret = 29,
    // Process Control
    Exit = 30, Sleep = 31,
    // Stack
    Pushr = 32, Pushi = 33, Popr = 34, Popm = 35,
    // Input
    Input = 36, Inputc = 37, Setpriority = 38, Setpriorityi = 39,
    // Shared Memory (Assignment 4)
    MapSharedMem = 40,
    // Locks (Assignment 4)
    AcquireLock  = 41, AcquireLockI = 42,
    ReleaseLock  = 43, ReleaseLockI = 44,
    // Events (Assignment 4)
    SignalEvent  = 45, SignalEventI = 46,
    WaitEvent    = 47, WaitEventI   = 48,
    // Heap Allocation (Module 5)
    Alloc      = 49,
    FreeMemory = 50,
};

#endif // ENUMOPCODE_H