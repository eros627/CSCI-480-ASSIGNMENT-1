// Implementing paged virtual memory using active page table and process bounds.

#include "MMU.H"

MMU::MMU(size_t physMemSize) : physMem(physMemSize) {
    if (physMemSize % PAGE_SIZE != 0) {
        throw std::runtime_error("Physical memory size must be multiple of PAGE_SIZE");
    }
    physUsed.assign(numPhysPages(), false);
}

uint32_t MMU::numPhysPages() const {
    return static_cast<uint32_t>(physMem.size() / PAGE_SIZE);
}

uint32_t MMU::allocPhysPage() {
    for (uint32_t p = 0; p < numPhysPages(); ++p) {
        if (!physUsed[p]) {
            physUsed[p] = true;
            uint32_t base = p * PAGE_SIZE;
            for (uint32_t i = 0; i < PAGE_SIZE; ++i)
                physMem.write8(base + i, 0);
            return p;
        }
    }
    throw std::runtime_error("Out of physical memory");
}

void MMU::freePhysPage(uint32_t ppage) {
    if (ppage >= numPhysPages())
        throw std::runtime_error("Physical page out of range");
    physUsed[ppage] = false;
}

void MMU::setPageTable(const std::vector<uint32_t>* pt) {
    activePageTable_ = pt;
}

void MMU::setBounds(uint32_t codeBase, uint32_t codeSize,
                    uint32_t dataBase, uint32_t dataSize,
                    uint32_t heapStart, uint32_t heapEnd,
                    uint32_t stackTop, uint32_t stackMax) {
    codeBase_  = codeBase;  codeSize_  = codeSize;
    dataBase_  = dataBase;  dataSize_  = dataSize;
    heapStart_ = heapStart; heapEnd_   = heapEnd;
    stackTop_  = stackTop;  stackMax_  = stackMax;
}

// Map physical page pp at virtual page vpage in the given page table.
// Grows the table if needed. Returns the virtual base address of that page.
uint32_t MMU::mapPageInto(std::vector<uint32_t>& pageTable,
                          uint32_t vpage, uint32_t pp) {
    if (vpage >= pageTable.size())
        pageTable.resize(vpage + 1, UNMAPPED);
    pageTable[vpage] = pp;
    return vpage * PAGE_SIZE;
}

// Register a shared virtual address region so isValidVaddr_ allows it.
void MMU::addSharedRegion(uint32_t base, uint32_t size) {
    sharedRegions_.push_back({base, size});
}

bool MMU::isValidVaddr_(uint32_t vaddr) const {
    if (vaddr >= codeBase_  && vaddr < codeBase_  + codeSize_)  return true;
    if (vaddr >= dataBase_  && vaddr < dataBase_  + dataSize_)  return true;
    if (vaddr >= heapStart_ && vaddr < heapEnd_)                return true;
    if (vaddr <  stackTop_  && vaddr >= (stackTop_ - stackMax_)) return true;

    for (const auto& r : sharedRegions_)
        if (vaddr >= r.base && vaddr < r.base + r.size) return true;

    return false;
}

uint32_t MMU::translate(uint32_t vaddr) const {
    if (!activePageTable_)
        throw std::runtime_error("MMU: no active page table");
    if (!isValidVaddr_(vaddr))
        throw std::runtime_error("MMU: virtual address outside process bounds");

    const uint32_t vpage  = vaddr >> OFFSET_BITS;
    const uint32_t offset = vaddr & OFFSET_MASK;

    if (vpage >= activePageTable_->size())
        throw std::runtime_error("MMU: virtual page out of range");

    const uint32_t ppage = (*activePageTable_)[vpage];
    if (ppage == UNMAPPED)
        throw std::runtime_error("MMU: unmapped page");

    return ppage * PAGE_SIZE + offset;
}

uint8_t  MMU::read8(uint32_t vaddr) const         { return physMem.read8(translate(vaddr)); }
void     MMU::write8(uint32_t vaddr, uint8_t val)  { physMem.write8(translate(vaddr), val); }

uint32_t MMU::read32(uint32_t vaddr) const {
    uint32_t val = 0;
    val |= static_cast<uint32_t>(read8(vaddr));
    val |= static_cast<uint32_t>(read8(vaddr + 1)) << 8;
    val |= static_cast<uint32_t>(read8(vaddr + 2)) << 16;
    val |= static_cast<uint32_t>(read8(vaddr + 3)) << 24;
    return val;
}

void MMU::write32(uint32_t vaddr, uint32_t val) {
    write8(vaddr,     static_cast<uint8_t>(val & 0xFF));
    write8(vaddr + 1, static_cast<uint8_t>((val >>  8) & 0xFF));
    write8(vaddr + 2, static_cast<uint8_t>((val >> 16) & 0xFF));
    write8(vaddr + 3, static_cast<uint8_t>((val >> 24) & 0xFF));
}