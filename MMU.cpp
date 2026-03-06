#include "MMU.H"

MMU::MMU(size_t physMemSize)
    : physMem(physMemSize)
{
    if (physMemSize % PAGE_SIZE != 0) 
    {
        throw std::runtime_error("Physical memory size must be a multiple of PAGE_SIZE (256).");
    }
    const uint32_t numPages = numPhysPages();

    physUsed.assign(numPages, false); // Initialize all physical pages as free

    pageTable.clear(); // Clear page table
}
uint8_t MMU::read8(uint32_t vaddr) const 
{
    uint32_t paddr = translate(vaddr);
    return physMem.read8(paddr);
}

void MMU::write8(uint32_t vaddr, uint8_t val) 
{
    uint32_t paddr = translate(vaddr);
    physMem.write8(paddr, val);
}

uint32_t MMU::read32(uint32_t vaddr) const // Little-endian read
{
    uint32_t val = 0;
    val |= static_cast<uint32_t>(read8(vaddr)) << 0;
    val |= static_cast<uint32_t>(read8(vaddr + 1)) << 8;
    val |= static_cast<uint32_t>(read8(vaddr + 2)) << 16;
    val |= static_cast<uint32_t>(read8(vaddr + 3)) << 24;
    return val;
}

void MMU::write32(uint32_t vaddr, uint32_t val) // Little-endian write
{
    // Write using MMU write8 so that translation happens per byte
    write8(vaddr, static_cast<uint8_t>((val >> 0) & 0xFF));
    write8(vaddr + 1, static_cast<uint8_t>((val >> 8) & 0xFF));
    write8(vaddr + 2, static_cast<uint8_t>((val >> 16) & 0xFF));
    write8(vaddr + 3, static_cast<uint8_t>((val >> 24) & 0xFF));
}

uint32_t MMU::numPhysPages() const
{
    return static_cast<uint32_t>(physMem.size() / PAGE_SIZE);
}

void MMU:: mapPage(uint32_t vpage, uint32_t ppage) 
{
    if (ppage >= numPhysPages()) 
    {
        throw std::runtime_error("Physical page number out of range.");
    }

    if (vpage >= pageTable.size()) 
    {
        pageTable.resize(vpage + 1, UNMAPPED);
    }

    if (physUsed[ppage]) 
    {
        throw std::runtime_error("Physical page already mapped.");
    }

    pageTable[vpage] = ppage;
    physUsed[ppage] = true;
}

uint32_t MMU::allocAndMap(uint32_t vpage) 
{
    for (uint32_t p = 0; p < numPhysPages(); ++p) 
    {
        if (!physUsed[p]) 
        {
            mapPage(vpage, p);
            return p;
        }
    }
    throw std::runtime_error("No free physical pages available.");
}

uint32_t MMU::getPhysPage(uint32_t vpage) const 
{
    if (vpage >= pageTable.size() || pageTable[vpage] == UNMAPPED) 
    {
        throw std::runtime_error("Virtual page number out of range or not mapped.");
    }
    return pageTable[vpage];
}

uint32_t MMU::translate(uint32_t vaddr) const 
{
    uint32_t vpage = vaddr >> 8;
    uint32_t offset = vaddr & 0xFF; 

    if (vpage >= pageTable.size()) 
    {
        throw std::runtime_error("Virtual page number out of range.");
    }

    uint32_t ppage = pageTable[vpage];
    if (ppage == UNMAPPED)
    {
        throw std::runtime_error("Virtual page not mapped.");
    }

    uint32_t paddr = ppage * PAGE_SIZE + offset;

    if (paddr > physMem.size()) 
    {
        throw std::runtime_error("Translated physical address out of bounds.");
    }

    return paddr;
}

void MMU::setPageTable(const std::vector<uint32_t>* pt) {
    activePageTable_ = pt;
}

bool MMU::isValidVaddr_(uint32_t vaddr) const {
    // simplest: allow if inside any segment
    bool inCode = (vaddr >= codeBase_ && vaddr < codeBase_ + codeSize_);
    bool inData = (vaddr >= dataBase_ && vaddr < dataBase_ + dataSize_);
    bool inHeap = (vaddr >= heapStart_ && vaddr < heapEnd_);
    // stack grows down: valid if vaddr in [stackTop-stackMax, stackTop)
    bool inStack = (vaddr < stackTop_ && vaddr >= (stackTop_ - stackMax_));
    return inCode || inData || inHeap || inStack;
}

uint32_t MMU::translate(uint32_t vaddr) const {
    if (!activePageTable_) throw std::runtime_error("MMU: no active page table set");
    if (!isValidVaddr_(vaddr)) throw std::runtime_error("MMU: vaddr outside process bounds");

    uint32_t vpage  = vaddr >> 8;
    uint32_t offset = vaddr & 0xFF;

    if (vpage >= activePageTable_->size()) throw std::runtime_error("MMU: vpage out of range");
    uint32_t ppage = (*activePageTable_)[vpage];
    if (ppage == PCB::UNMAPPED) throw std::runtime_error("MMU: unmapped page");

    return ppage * 256 + offset;
}