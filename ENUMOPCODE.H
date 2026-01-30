#ifndef ENUMOPCODE_H
#define ENUMOPCODE_H

#include <cstdint> // for uint32_t

// Define an enumeration for opcodes used in a virtual machine or assembly language adding comments between different opcode sections for readability.
enum class Opcode : uint32_t {
    // Arithmetic Operations
    Incr = 1,
    Addi = 2,
    Addr = 3,

    // Data Movement Operations
    Movi = 4,
    Movr = 5,
    Movmr = 6,
    Movrm = 7,
    Movmm = 8,

    // Output Operations
    Printr = 9,
    Printm = 10,
    Printcr = 11,
    Printcm = 12,

    // control flow
    Jmp = 13,
    Jmpi = 14,
    Jmpa = 15,
    Cmpi = 16,
    Cmpr = 17, 
    Jlt = 18,
    Jlti = 19,
    Jlta = 20,
    Jgt = 21,
    Jgti = 22,
    Jgta = 23,
    Je = 24,
    Jei = 25,
    Jea = 26,
    Call = 27,
    Callm = 28,
    Ret = 29,

    //Process Control
    Exit = 30,
    Sleep = 31,
    
    //Stack Operations
    Pushr = 32,
    Pushi = 33,
    Popr = 34,
    Popm = 35,
    
    //Input Operations
    Input = 36,
    Inputc = 37,
    Setpriority = 38,
    Setpriorityi = 39

};

#endif // ENUMOPCODE_H
