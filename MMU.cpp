// MMU.cpp  paged virtual memory with LRU eviction, dirty tracking,
// and page fault exceptions for demand paging.

#include "MMU.H"


MMU::MMU(size_t physMemSize) : physMem(physMemSize) {
    if (physMemSize % PAGE_SIZE != 0)
        throw std::runtime_error("Physical memory size must be multiple of PAGE_SIZE");
    physUsed.assign(numPhysPages(), false);
}

uint32_t MMU::numPhysPages() const {
    return static_cast<uint32_t>(physMem.size() / PAGE_SIZE);
}

void MMU::zeroPhysPage(uint32_t ppage) {
    uint32_t base = ppage * PAGE_SIZE;
    for (uint32_t i = 0; i < PAGE_SIZE; ++i)
        physMem.write8(base + i, 0);
}

uint32_t MMU::allocPhysPage() {
    for (uint32_t p = 0; p < numPhysPages(); ++p) {
        if (!physUsed[p]) {
            physUsed[p] = true;
            zeroPhysPage(p);   // always start clean
            touchPhysPage(p);  // add to LRU as MRU
            return p;
        }
    }
    throw std::runtime_error("Out of physical memory");
}

void MMU::freePhysPage(uint32_t ppage) {
    if (ppage >= numPhysPages())
        throw std::runtime_error("Physical page out of range");
    physUsed[ppage] = false;
    removeLRUEntry(ppage);
}


void MMU::setPageTable(std::vector<PageEntry>* pt) {
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

uint32_t MMU::mapPageInto(std::vector<PageEntry>& pageTable,
                          uint32_t vpage, uint32_t pp) {
    if (vpage >= pageTable.size())
        pageTable.resize(vpage + 1, PageEntry{});
    pageTable[vpage].physPage = pp;
    pageTable[vpage].isValid  = true;
    pageTable[vpage].isDirty  = false;
    return vpage * PAGE_SIZE;
}

void MMU::addSharedRegion(uint32_t base, uint32_t size) {
    sharedRegions_.push_back({base, size});
}


bool MMU::isValidVaddr_(uint32_t vaddr) const {
    if (vaddr >= codeBase_  && vaddr < codeBase_  + codeSize_)  return true;
    if (vaddr >= dataBase_  && vaddr < dataBase_  + dataSize_)  return true;
    if (vaddr >= heapStart_ && vaddr < heapEnd_)                return true;
    if (vaddr <  stackTop_  && vaddr >= (stackTop_ - stackMax_)) return true;
    for (const auto& r : sharedRegions_)
        if (vaddr >= r.base && vaddr < r.base + r.size)         return true;
    return false;
}

uint32_t MMU::translate(uint32_t vaddr, bool isWrite) {
    if (!activePageTable_)
        throw std::runtime_error("MMU: no active page table");
    if (!isValidVaddr_(vaddr))
        throw std::runtime_error("MMU: virtual address outside process bounds");

    const uint32_t vpage  = vaddr >> OFFSET_BITS;
    const uint32_t offset = vaddr & OFFSET_MASK;

    // Page not yet in page table  →  page fault (lazy allocation)
    if (vpage >= activePageTable_->size())
        throw PageFaultException(vpage);

    PageEntry& entry = (*activePageTable_)[vpage];

    // Page not in physical memory  →  page fault (demand paging / swap-in)
    if (!entry.isValid)
        throw PageFaultException(vpage);

    // Update dirty flag on writes
    if (isWrite)
        entry.isDirty = true;

    touchPhysPage(entry.physPage);
    return entry.physPage * PAGE_SIZE + offset;
}



uint8_t  MMU::read8(uint32_t vaddr)           { return physMem.read8 (translate(vaddr, false)); }
void     MMU::write8(uint32_t vaddr, uint8_t v){ physMem.write8(translate(vaddr, true),  v); }

uint32_t MMU::read32(uint32_t vaddr) {
    uint32_t val = 0;
    val |= static_cast<uint32_t>(read8(vaddr))     ;
    val |= static_cast<uint32_t>(read8(vaddr + 1)) <<  8;
    val |= static_cast<uint32_t>(read8(vaddr + 2)) << 16;
    val |= static_cast<uint32_t>(read8(vaddr + 3)) << 24;
    return val;
}

void MMU::write32(uint32_t vaddr, uint32_t val) {
    write8(vaddr,     static_cast<uint8_t>( val        & 0xFF));
    write8(vaddr + 1, static_cast<uint8_t>((val >>  8) & 0xFF));
    write8(vaddr + 2, static_cast<uint8_t>((val >> 16) & 0xFF));
    write8(vaddr + 3, static_cast<uint8_t>((val >> 24) & 0xFF));
}

// ─────────────────────────────────────────────────────────────────────────────
// LRU management  (front = MRU, back = LRU)
// ─────────────────────────────────────────────────────────────────────────────

void MMU::touchPhysPage(uint32_t ppage) {
    auto it = lruPos_.find(ppage);
    if (it != lruPos_.end())
        lruList_.erase(it->second);
    lruList_.push_front(ppage);
    lruPos_[ppage] = lruList_.begin();
}

uint32_t MMU::getLRUPhysPage(const std::vector<uint32_t>& pinned) const {
    // Scan from the LRU end, skipping pages in `pinned` (e.g. OS-owned shared pages)
    for (auto it = lruList_.rbegin(); it != lruList_.rend(); ++it) {
        uint32_t pp = *it;
        bool isPinned = false;
        for (uint32_t pin : pinned)
            if (pin == pp) { isPinned = true; break; }
        if (!isPinned) return pp;
    }
    throw std::runtime_error("MMU: no evictable physical page");
}

void MMU::removeLRUEntry(uint32_t ppage) {
    auto it = lruPos_.find(ppage);
    if (it != lruPos_.end()) {
        lruList_.erase(it->second);
        lruPos_.erase(it);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Physical page I/O  (used by OS for disk-swap simulation)
// ─────────────────────────────────────────────────────────────────────────────

void MMU::readPhysPage(uint32_t ppage, std::vector<uint8_t>& buf) const {
    buf.resize(PAGE_SIZE);
    uint32_t base = ppage * PAGE_SIZE;
    for (uint32_t i = 0; i < PAGE_SIZE; ++i)
        buf[i] = physMem.read8(base + i);
}

void MMU::writePhysPage(uint32_t ppage, const std::vector<uint8_t>& buf) {
    uint32_t base = ppage * PAGE_SIZE;
    for (uint32_t i = 0; i < PAGE_SIZE && i < buf.size(); ++i)
        physMem.write8(base + i, buf[i]);
}