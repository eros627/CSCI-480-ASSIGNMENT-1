#include <iostream>
#include <stdexcept>
#include <cstdint>
#include "MMU.H"

static void expect(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}

int main() {
    try {
        std::cout << "=== MMU PAGING TEST START ===\n";

        // 4 pages of physical memory = 4 * 256 bytes
        MMU mmu(4 * 256);

        // Fake mapping (from your spec idea):
        // vpage 0 -> ppage 0  (physical start 0)
        // vpage 1 -> ppage 2  (physical start 512)
        // vpage 2 -> ppage 1  (physical start 256)
        mmu.mapPage(0, 0);
        mmu.mapPage(1, 2);
        mmu.mapPage(2, 1);

        // Write values to virtual addresses and read back
        mmu.write8(0,   0xAA);    // vpage0 offset0
        mmu.write8(255, 0xBB);    // vpage0 offset255
        mmu.write8(256, 0xCC);    // vpage1 offset0 (should hit physical page 2)
        mmu.write8(511, 0xDD);    // vpage1 offset255

        expect(mmu.read8(0)   == 0xAA, "read8(0) failed");
        expect(mmu.read8(255) == 0xBB, "read8(255) failed");
        expect(mmu.read8(256) == 0xCC, "read8(256) failed");
        expect(mmu.read8(511) == 0xDD, "read8(511) failed");

        // Cross-page read32 test: start at 254 uses bytes 254,255,256,257 across boundary
        mmu.write8(254, 0x11);
        mmu.write8(255, 0x22);
        mmu.write8(256, 0x33);
        mmu.write8(257, 0x44);

        uint32_t x = mmu.read32(254);
        // little-endian: 0x44332211
        expect(x == 0x44332211, "read32 crossing boundary failed");

        std::cout << "read32 crossing page boundary PASSED: 0x" 
                  << std::hex << x << std::dec << "\n";
        std::cout << "=== MMU PAGING TEST PASSED ===\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
