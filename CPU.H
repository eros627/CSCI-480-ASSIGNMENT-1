#ifndef CPU_H
#define CPU_H

#include <cstdint>
#include <array>
#include "MMU.H"
#include "ENUMOPCODE.H"



class CPU {
public:
    CPU(MMU & mmu); // Constructor that takes a reference to an MMU object

    void SetIP(uint32_t ip); // Set the instruction pointer

    void SetSP(uint32_t sp); // Set the stack pointer

    void Run(uint64_t maxsteps = 1'000'000); // Execute instructions

    void step();// Execute a single instruction

    int64_t getReg(uint32_t r) const; // Get the value of a register

    private:

    MMU & mmu; // Reference to the memory management unit

    array<int64_t, 16> regs_{}; // General-purpose registers

    uint32_t ip_ = 0; // Instruction pointer
    uint32_t sp_ = 0; // Stack pointer

    // flags for CMP instruction
    bool zeroFlag_ = false;
    bool signFlag_ = false;
    bool running_ = true; // CPU running state

    // Helper: validate register index is 1..10
    void checkReg(uint32_t r) const;

    // Fetch the 3-word instruction at current IP (12 bytes)
    void fetch(uint32_t& rawOpcode, uint32_t& opA, uint32_t& opB);

    // Execute one decoded instruction
    void execute(Opcode opcode, uint32_t opA, uint32_t opB);
};
#endif // CPU_H