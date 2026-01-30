#include <iostream>
#include <iomanip>
#include <cassert>

#include "PHYSMEM.H"
#include "MMU.H"

int main() {
    std::cout << "=== MMU TEST START ===\n";

    // Create 1 KB of physical memory through the MMU
    MMU mmu(1024);

    uint32_t addr  = 100;
    uint32_t value = 0x12345678;

    // Write a 32-bit value
    mmu.write32(addr, value);

    // Read it back
    uint32_t readBack = mmu.read32(addr);

    // Check correctness
    assert(readBack == value);

    std::cout << "write32 / read32 PASSED\n";
    std::cout << "Value read: 0x"
              << std::hex << std::setw(8) << std::setfill('0')
              << readBack << std::dec << "\n";

    // Show underlying bytes to verify little-endian
    std::cout << "Memory bytes (little-endian expected):\n";
    for (int i = 0; i < 4; i++) {
        uint8_t b = mmu.read8(addr + i);
        std::cout << "  mem[" << (addr + i) << "] = 0x"
                  << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(b) << std::dec << "\n";
    }

    std::cout << "Expected order: 78 56 34 12\n";
    std::cout << "=== MMU TEST PASSED ===\n";

    return 0;
}
