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

            // zero out page
            uint32_t base = p * PAGE_SIZE;
            for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
                physMem.write8(base + i, 0);
            }

            return p;
        }
    }
    throw std::runtime_error("Out of physical memory");
}

void MMU::freePhysPage(uint32_t ppage) {
    if (ppage >= numPhysPages()) {
        throw std::runtime_error("Physical page out of range");
    }
    physUsed[ppage] = false;
}

void MMU::setPageTable(const std::vector<uint32_t>* pt) {
    activePageTable_ = pt;
}

void MMU::setBounds(uint32_t codeBase,  uint32_t codeSize,
                    uint32_t dataBase,  uint32_t dataSize,
                    uint32_t heapStart, uint32_t heapEnd,
                    uint32_t stackTop,  uint32_t stackMax) {
    codeBase_  = codeBase;
    codeSize_  = codeSize;
    dataBase_  = dataBase;
    dataSize_  = dataSize;
    heapStart_ = heapStart;
    heapEnd_   = heapEnd;
    stackTop_  = stackTop;
    stackMax_  = stackMax;
}

bool MMU::isValidVaddr_(uint32_t vaddr) const {
    bool inCode  = (vaddr >= codeBase_ && vaddr < codeBase_ + codeSize_);
    bool inData  = (vaddr >= dataBase_ && vaddr < dataBase_ + dataSize_);
    bool inHeap  = (vaddr >= heapStart_ && vaddr < heapEnd_);
    bool inStack = (vaddr < stackTop_ && vaddr >= (stackTop_ - stackMax_));

    return inCode || inData || inHeap || inStack;
}

uint32_t MMU::translate(uint32_t vaddr) const {
    if (!activePageTable_) {
        throw std::runtime_error("MMU: no active page table");
    }

    if (!isValidVaddr_(vaddr)) {
        throw std::runtime_error("MMU: virtual address outside process bounds");
    }

    uint32_t vpage  = vaddr >> OFFSET_BITS;
    uint32_t offset = vaddr & OFFSET_MASK;

    if (vpage >= activePageTable_->size()) {
        throw std::runtime_error("MMU: virtual page out of range");
    }

    uint32_t ppage = (*activePageTable_)[vpage];
    if (ppage == UNMAPPED) {
        throw std::runtime_error("MMU: unmapped page");
    }

    return ppage * PAGE_SIZE + offset;
}

uint8_t MMU::read8(uint32_t vaddr) const {
    return physMem.read8(translate(vaddr));
}

void MMU::write8(uint32_t vaddr, uint8_t val) {
    physMem.write8(translate(vaddr), val);
}

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
    write8(vaddr + 1, static_cast<uint8_t>((val >> 8) & 0xFF));
    write8(vaddr + 2, static_cast<uint8_t>((val >> 16) & 0xFF));
    write8(vaddr + 3, static_cast<uint8_t>((val >> 24) & 0xFF));
}