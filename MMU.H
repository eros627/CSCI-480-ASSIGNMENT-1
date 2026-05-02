#ifndef MMU_H
#define MMU_H

#include "PHYSMEM.H"
#include "pageentry.h"    // PageEntry + PageFaultException
#include <array>
#include <cstdint>
#include <limits>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <vector>

class MMU {
public:
    static constexpr uint32_t PAGE_SIZE   = 256;
    static constexpr uint32_t OFFSET_MASK = 0xFF;
    static constexpr uint32_t OFFSET_BITS = 8;
    static constexpr uint32_t UNMAPPED    = std::numeric_limits<uint32_t>::max();

    explicit MMU(size_t physMemSize);

    // Memory access – non-const so translate can update LRU + dirty flag
    uint8_t  read8 (uint32_t vaddr);
    void     write8(uint32_t vaddr, uint8_t val);
    uint32_t read32(uint32_t vaddr);
    void     write32(uint32_t vaddr, uint32_t val);

    uint32_t allocPhysPage();
    void     freePhysPage(uint32_t ppage);

    // page table pointer is mutable (translate sets isDirty on writes)
    void setPageTable(std::vector<PageEntry>* pt);

    void setBounds(uint32_t codeBase,  uint32_t codeSize,
                   uint32_t dataBase,  uint32_t dataSize,
                   uint32_t heapStart, uint32_t heapEnd,
                   uint32_t stackTop,  uint32_t stackMax);

    // Map physical page pp at virtual page vpage; returns virtual base address.
    uint32_t mapPageInto(std::vector<PageEntry>& pageTable,
                         uint32_t vpage, uint32_t pp);

    // Register a shared memory region so isValidVaddr_ allows access to it.
    void addSharedRegion(uint32_t base, uint32_t size);

    // LRU management 
    // Called automatically by translate; also callable by OS after manual page setup.
    void     touchPhysPage(uint32_t ppage);
    // Returns the least-recently-used physical page not in `pinned`.
    uint32_t getLRUPhysPage(const std::vector<uint32_t>& pinned) const;
    // Remove a physical page from the LRU list (call before freePhysPage or on eviction).
    void     removeLRUEntry(uint32_t ppage);

    // page-granularity physical memory I/O (for disk swap) 
    void readPhysPage (uint32_t ppage, std::vector<uint8_t>& buf) const;
    void writePhysPage(uint32_t ppage, const std::vector<uint8_t>& buf);
    void zeroPhysPage (uint32_t ppage);

private:
    PhysMem physMem;
    std::vector<bool> physUsed;

    // Module 6: mutable pointer so translate can set isDirty on writes
    std::vector<PageEntry>* activePageTable_ = nullptr;

    uint32_t codeBase_  = 0, codeSize_  = 0;
    uint32_t dataBase_  = 0, dataSize_  = 0;
    uint32_t heapStart_ = 0, heapEnd_   = 0;
    uint32_t stackTop_  = 0, stackMax_  = 0;

    struct SharedRegion { uint32_t base; uint32_t size; };
    std::vector<SharedRegion> sharedRegions_;

    // LRU list: front = MRU, back = LRU
    std::list<uint32_t> lruList_;
    std::unordered_map<uint32_t, std::list<uint32_t>::iterator> lruPos_;

    uint32_t numPhysPages() const;
    bool     isValidVaddr_(uint32_t vaddr) const;
    // isWrite=true causes translate to set isDirty on the page entry
    uint32_t translate(uint32_t vaddr, bool isWrite = false);
};

#endif