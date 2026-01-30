#ifndef MMU_H
#define MMU_H

#include "PHYSMEM.H"
#include <cstdint>


// Memory Management Unit (MMU) class
class MMU {
    public:
        explicit MMU(size_t physMemSize) : physMem(physMemSize) {}// Constructor initializing physical memory

        uint8_t read8(uint32_t addr) const;
        void write8(uint32_t addr, uint8_t val);

        uint32_t read32(uint32_t addr) const;
        void write32(uint32_t addr, uint32_t val);

    private:
        PhysMem physMem;// Reference to physical memory
    
};

#endif // MMU_H