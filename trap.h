// Defines CPU stop reasons, letting the OS differentiate between a normal exit or other.
#ifndef TRAP_H
#define TRAP_H

enum class Trap {
    None,
    Exit,
    QuantumExpired,
    Sleep,
    Input,
    Inputc
};

#endif
