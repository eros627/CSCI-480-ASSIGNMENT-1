#ifndef CPU_H
#define CPU_H

#include <array>
#include <cstdint>
#include "MMU.H"
#include "ENUMOPCODE.H"
#include "PCB.h"
#include "trap.h"

class CPU {
public:
    explicit CPU(MMU& mmu);
    void SetIP(uint32_t ip);
    void SetSP(uint32_t sp);
    Trap Run(uint64_t maxsteps);
    Trap step();
    void saveTo(PCB& out) const;
    void loadFrom(const PCB& in);
    int64_t getReg(uint32_t r) const;

private:
    MMU& mmu;
    std::array<int64_t, 16> regs_{};
    uint32_t ip_ = 0, sp_ = 0;
    bool zeroFlag_ = false, signFlag_ = false, running_ = false;

    // Saved when a trap with register operands fires (Alloc, FreeMemory).
    uint32_t trapRegA_ = 0;
    uint32_t trapRegB_ = 0;

    void checkReg(uint32_t r) const;
    void fetch(uint32_t& rawOpcode, uint32_t& opA, uint32_t& opB);
    Trap execute(Opcode opcode, uint32_t opA, uint32_t opB);
};

#endif