#ifndef MMU_H
#define MMU_H

#include "PHYSMEM.H"
#include <cstdint>
#include <limits>
#include <vector>

// Memory Management Unit (MMU) class
class MMU {
    public:

        static constexpr uint32_t PAGE_SIZE   = 256;
        static constexpr uint32_t OFFSET_MASK = 0xFF;
        static constexpr uint32_t OFFSET_BITS = 8;
        static constexpr uint32_t UNMAPPED    = std::numeric_limits<uint32_t>::max();// Special value indicating unmapped page

        explicit MMU(size_t physMemSize);// Constructor initializing physical memory

        uint8_t read8(uint32_t vaddr) const;
        void write8(uint32_t vaddr, uint8_t val);

        uint32_t read32(uint32_t vaddr) const;
        void write32(uint32_t vaddr, uint32_t val);

        void mapPage(uint32_t vpage, uint32_t ppage) ; // Map virtual page to physical page

        uint32_t allocAndMap(uint32_t vpage) ; // Map and allocate a physical page for the given virtual page

        uint32_t getPhysPage(uint32_t vpage) const; // Get physical page number for a given virtual page, or UNMAPPED if not mapped


    private:
        PhysMem physMem;// Physical memory instance
    
        uint32_t translate(uint32_t vaddr) const; // Virtual to physical address translation

        std:: vector<uint32_t> pageTable; // Simple page table mapping virtual pages to physical pages
        std:: vector<bool> physUsed; // Track used physical pages

        uint32_t numPhysPages() const;
};

#endif // MMU_H