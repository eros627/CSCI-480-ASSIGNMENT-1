#include <iostream>
#include <cassert>

#include "MMU.H"
#include "CPU.H"
#include "ENUMOPCODE.H"

// Write one 12-byte instruction: [opcode][opA][opB]
static void writeInstr(MMU& mmu, uint32_t addr, Opcode op, uint32_t opA, uint32_t opB) {
    mmu.write32(addr,     static_cast<uint32_t>(op));
    mmu.write32(addr + 4, opA);
    mmu.write32(addr + 8, opB);
}

int main() {
    std::cout << "=== CPU TEST START ===\n";

    MMU mmu(64 * 1024);   // 64KB simulated RAM
    CPU cpu(mmu);

    const uint32_t programBase = 0x1000;
    const uint32_t stackTop    = 0x9000;

    // Program:
    // movi r1, #1
    // incr r1
    // printr r1
    // exit
    writeInstr(mmu, programBase + 0,  Opcode::Movi,   1, 1);
    writeInstr(mmu, programBase + 12, Opcode::Incr,   1, 0);
    writeInstr(mmu, programBase + 24, Opcode::Printr, 1, 0);
    writeInstr(mmu, programBase + 36, Opcode::Exit,   0, 0);

    cpu.SetIP(programBase);
    cpu.SetSP(stackTop);

    std::cout << "Expected output: 2\nActual output:\n";
    cpu.Run();

    // Strong check: register should be 2
    assert(cpu.getReg(1) == 2);

    std::cout << "=== CPU TEST PASSED (r1 == 2) ===\n";
    return 0;
}
