#ifndef MMU_H
#define MMU_H

#include "PHYSMEM.H"
#include <cstdint>
#include <limits>
#include <vector>

class MMU {

public:

    static constexpr uint32_t PAGE_SIZE   = 256;
    static constexpr uint32_t OFFSET_MASK = 0xFF;
    static constexpr uint32_t OFFSET_BITS = 8;
    static constexpr uint32_t UNMAPPED    = std::numeric_limits<uint32_t>::max();

    explicit MMU(size_t physMemSize);

    uint8_t read8(uint32_t vaddr) const;
    void write8(uint32_t vaddr, uint8_t val);

    uint32_t read32(uint32_t vaddr) const;
    void write32(uint32_t vaddr, uint32_t val);

    void mapPage(uint32_t vpage, uint32_t ppage);
    uint32_t allocAndMap(uint32_t vpage);

    uint32_t getPhysPage(uint32_t vpage) const;

    void setPageTable(const std::vector<uint32_t>* pt);

    void setBounds(uint32_t codeBase, uint32_t codeSize,
                   uint32_t dataBase, uint32_t dataSize,
                   uint32_t heapStart, uint32_t heapEnd,
                   uint32_t stackTop, uint32_t stackMax);

private:

    PhysMem physMem;

    std::vector<bool> physUsed;

    uint32_t numPhysPages() const;

    const std::vector<uint32_t>* activePageTable_ = nullptr;

    uint32_t codeBase_=0, codeSize_=0;
    uint32_t dataBase_=0, dataSize_=0;
    uint32_t heapStart_=0, heapEnd_=0;
    uint32_t stackTop_=0, stackMax_=0;

    bool isValidVaddr_(uint32_t vaddr) const;

    uint32_t translate(uint32_t vaddr) const;

};

#endif