#include "OS.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

OS::OS(size_t physMemBytes) : mmu_(physMemBytes), cpu_(mmu_), ready_(33) {
    for (uint32_t i = 0; i < NUM_SHARED_REGIONS; ++i)
        sharedPhysPages_.push_back(mmu_.allocPhysPage());
}

uint32_t OS::createProcessFromAsm(const std::string& asmFile, uint32_t priority) {
    Program prog;
    prog.loadFromFile(asmFile);

    Process p;
    p.pcb.pid      = nextPid_++;
    p.pcb.priority = priority;
    p.pcb.state    = ProcState::Ready;

    p.pcb.codeBase  = 0x1000;  p.pcb.codeSize  = prog.sizeBytes();
    p.pcb.dataBase  = 0x4000;  p.pcb.dataSize  = 0x1000;
    p.pcb.heapStart = 0x5000;  p.pcb.heapEnd   = 0x6000;
    p.pcb.stackTop  = 0xF000;  p.pcb.stackMax  = 0x1000;
    p.pcb.ip = p.pcb.codeBase;
    p.pcb.sp = p.pcb.stackTop;

    loadProcessImage_(p, prog);
    processes_.push_back(p);
    enqueueReady_(processes_.size() - 1);
    return p.pcb.pid;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 6: lazy process loading — only code pages are eagerly allocated.
// Stack, heap, and data pages fault in on first access (demand paging).
// ─────────────────────────────────────────────────────────────────────────────
void OS::loadProcessImage_(Process& p, const Program& prog) {
    if (p.pcb.codeSize == 0)
        throw std::runtime_error("Program has no code bytes");

    uint32_t firstCodePage = p.pcb.codeBase / MMU::PAGE_SIZE;
    uint32_t lastCodePage  = (p.pcb.codeBase + p.pcb.codeSize - 1) / MMU::PAGE_SIZE;
    uint32_t maxVPage      = (p.pcb.stackTop - 1) / MMU::PAGE_SIZE;

    // All virtual pages start as invalid (isValid=false, physPage=UNMAPPED).
    p.pcb.pageTable.assign(maxVPage + 1, PageEntry{});
    p.pcb.workingSetPages.clear();

    mmu_.setPageTable(&p.pcb.pageTable);
    mmu_.setBounds(p.pcb.codeBase, p.pcb.codeSize, p.pcb.dataBase, p.pcb.dataSize,
                   p.pcb.heapStart, p.pcb.heapEnd, p.pcb.stackTop, p.pcb.stackMax);

    // Eagerly allocate physical pages for code only.
    for (uint32_t vp = firstCodePage; vp <= lastCodePage; ++vp) {
        uint32_t pp = mmu_.allocPhysPage();
        p.pcb.pageTable[vp].physPage = pp;
        p.pcb.pageTable[vp].isValid  = true;
        // isDirty starts false; loadIntoMemory writes to the page via write8,
        // which calls translate(isWrite=true) and sets isDirty=true automatically.
        physPageOwner_[pp] = {p.pcb.pid, vp};
        p.pcb.workingSetPages.push_back(vp);
    }

    // Write program bytes into the code pages (this sets isDirty=true on them).
    prog.loadIntoMemory(mmu_, p.pcb.codeBase);
}

void OS::contextSwitchTo_(Process& p) {
    mmu_.setPageTable(&p.pcb.pageTable);
    mmu_.setBounds(p.pcb.codeBase, p.pcb.codeSize, p.pcb.dataBase, p.pcb.dataSize,
                   p.pcb.heapStart, p.pcb.heapEnd, p.pcb.stackTop, p.pcb.stackMax);
    cpu_.loadFrom(p.pcb);
    p.pcb.state = ProcState::Running;
    p.pcb.contextSwitches++;
}

void OS::contextSwitchOut_(Process& p) { cpu_.saveTo(p.pcb); }

// ─────────────────────────────────────────────────────────────────────────────
// Main scheduler loop
// ─────────────────────────────────────────────────────────────────────────────
void OS::run() {
    while (true) {
        tickSleepers_();
        int next = pickNextReady_();
        if (next < 0) { if (sleeping_.empty()) break; continue; }

        size_t   procIndex = static_cast<size_t>(next);
        Process& p         = processes_[procIndex];
        if (p.pcb.state == ProcState::Terminated) continue;

        contextSwitchTo_(p);
        try {
            Trap t = cpu_.Run(p.pcb.timeQuantum);
            contextSwitchOut_(p);
            p.pcb.cycles += p.pcb.timeQuantum;

            switch (t) {
                case Trap::Exit:
                    cleanupProcess_(p);
                    break;

                case Trap::QuantumExpired:
                    enqueueReady_(procIndex);
                    break;

                case Trap::Sleep:
                    p.pcb.state = ProcState::WaitingSleep;
                    p.pcb.sleepCounter = (p.pcb.regs[1] <= 0)
                                       ? 1u
                                       : static_cast<uint32_t>(p.pcb.regs[1]);
                    sleeping_.push_back(procIndex);
                    break;

                case Trap::Input:
                case Trap::Inputc:
                    p.pcb.state = ProcState::WaitingIO;
                    enqueueReady_(procIndex);
                    break;

                case Trap::MapSharedMem: {
                    int regionId = static_cast<int>(p.pcb.regs[1]);
                    int retReg   = static_cast<int>(p.pcb.regs[15]);
                    if (regionId >= 0 && regionId < (int)sharedPhysPages_.size()) {
                        uint32_t vpage = SHARED_MEM_VBASE / MMU::PAGE_SIZE + regionId;
                        uint32_t pp    = sharedPhysPages_[regionId];
                        uint32_t vaddr = mmu_.mapPageInto(p.pcb.pageTable, vpage, pp);
                        p.pcb.regs[retReg] = vaddr;
                        // Shared pages are NOT added to physPageOwner_ — they are
                        // pinned OS resources and must never be evicted.
                    }
                    enqueueReady_(procIndex);
                    break;
                }

                case Trap::AcquireLock: {
                    int lockId = static_cast<int>(p.pcb.regs[1]);
                    if (lockId < 0 || lockId >= NUM_LOCKS) {
                        enqueueReady_(procIndex);
                    } else if (!locks_[lockId].held) {
                        locks_[lockId].held     = true;
                        locks_[lockId].ownerPid = p.pcb.pid;
                        enqueueReady_(procIndex);
                    } else {
                        p.pcb.state = ProcState::WaitingLock;
                        p.pcb.waitingLockId = lockId;
                        locks_[lockId].waitQueue.push_back(procIndex);
                    }
                    break;
                }

                case Trap::ReleaseLock: {
                    int lockId = static_cast<int>(p.pcb.regs[1]);
                    if (lockId >= 0 && lockId < NUM_LOCKS
                        && locks_[lockId].held
                        && locks_[lockId].ownerPid == p.pcb.pid) {
                        if (!locks_[lockId].waitQueue.empty()) {
                            size_t nextIdx = locks_[lockId].waitQueue.front();
                            locks_[lockId].waitQueue.pop_front();
                            locks_[lockId].ownerPid = processes_[nextIdx].pcb.pid;
                            enqueueReady_(nextIdx);
                        } else {
                            locks_[lockId].held     = false;
                            locks_[lockId].ownerPid = 0;
                        }
                    }
                    enqueueReady_(procIndex);
                    break;
                }

                case Trap::WaitEvent: {
                    int eventId = static_cast<int>(p.pcb.regs[1]);
                    if (eventId < 0 || eventId >= NUM_EVENTS) {
                        enqueueReady_(procIndex);
                    } else if (events_[eventId].signaled) {
                        events_[eventId].signaled = false;
                        enqueueReady_(procIndex);
                    } else {
                        p.pcb.state = ProcState::WaitingEvent;
                        p.pcb.waitingEventId = eventId;
                        events_[eventId].waitQueue.push_back(procIndex);
                    }
                    break;
                }

                case Trap::SignalEvent: {
                    int eventId = static_cast<int>(p.pcb.regs[1]);
                    if (eventId >= 0 && eventId < NUM_EVENTS) {
                        if (!events_[eventId].waitQueue.empty()) {
                            while (!events_[eventId].waitQueue.empty()) {
                                size_t waitIdx = events_[eventId].waitQueue.front();
                                events_[eventId].waitQueue.pop_front();
                                enqueueReady_(waitIdx);
                            }
                        } else {
                            events_[eventId].signaled = true;
                        }
                    }
                    enqueueReady_(procIndex);
                    break;
                }

                case Trap::Alloc:
                    handleAlloc_(procIndex);
                    break;

                case Trap::FreeMemory:
                    handleFreeMemory_(procIndex);
                    break;

                case Trap::None:
                    enqueueReady_(procIndex);
                    break;
            }

        // ── Module 6: page fault ──────────────────────────────────────────────
        // CPU::step restores ip_ to the faulting instruction before rethrowing,
        // so the process will retry it once the page is brought in.
        } catch (const PageFaultException& pf) {
            contextSwitchOut_(p);
            handlePageFault_(procIndex, pf.vpage);

        } catch (const std::exception& ex) {
            std::cerr << "Process " << p.pcb.pid << " faulted: " << ex.what() << "\n";
            contextSwitchOut_(p);
            cleanupProcess_(p);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 6: page fault handler
// Brings vpage into physical memory (evicting LRU if needed) and re-queues
// the faulting process to retry its instruction.
// ─────────────────────────────────────────────────────────────────────────────
void OS::handlePageFault_(size_t procIndex, uint32_t vpage) {
    Process& p = processes_[procIndex];

    // Grow page table if this vpage was beyond its current size
    if (vpage >= p.pcb.pageTable.size())
        p.pcb.pageTable.resize(vpage + 1, PageEntry{});

    // Try to get a free physical page; evict LRU if memory is full.
    uint32_t ppage;
    bool gotFresh = false;
    try {
        ppage    = mmu_.allocPhysPage();  // zeroes page, adds to LRU
        gotFresh = true;
    } catch (...) {
        ppage = evictOnePage_();          // reclaim LRU page (still physUsed=true)
    }

    // If the page was previously swapped out, restore it from disk.
    auto diskKey = std::make_pair(p.pcb.pid, vpage);
    auto dit     = disk_.find(diskKey);
    if (dit != disk_.end()) {
        mmu_.writePhysPage(ppage, dit->second);
        disk_.erase(dit);
    } else if (!gotFresh) {
        // Evicted page: zero it out for a fresh stack/heap page.
        mmu_.zeroPhysPage(ppage);
    }
    // gotFresh path: allocPhysPage already zeroed it.

    // Wire up the page table entry.
    p.pcb.pageTable[vpage].physPage = ppage;
    p.pcb.pageTable[vpage].isValid  = true;
    p.pcb.pageTable[vpage].isDirty  = false;

    physPageOwner_[ppage] = {p.pcb.pid, vpage};
    mmu_.touchPhysPage(ppage);   // mark as MRU (allocPhysPage already did this,
                                  // but evictOnePage_ removed it — always safe to call)
    enqueueReady_(procIndex);
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 6: LRU eviction
// Finds the least-recently-used non-pinned physical page, writes it to the
// simulated disk if dirty, marks the owner's page table entry as invalid,
// and returns the reclaimed page number (physUsed remains true).
// ─────────────────────────────────────────────────────────────────────────────
uint32_t OS::evictOnePage_() {
    uint32_t ppage = mmu_.getLRUPhysPage(sharedPhysPages_);

    auto ownerIt = physPageOwner_.find(ppage);
    if (ownerIt != physPageOwner_.end()) {
        uint32_t ownerPid   = ownerIt->second.first;
        uint32_t ownerVpage = ownerIt->second.second;

        for (auto& proc : processes_) {
            if (proc.pcb.pid == ownerPid &&
                ownerVpage < proc.pcb.pageTable.size()) {
                PageEntry& entry = proc.pcb.pageTable[ownerVpage];
                if (entry.isDirty) {
                    // Save the page contents to the simulated disk.
                    std::vector<uint8_t> buf;
                    mmu_.readPhysPage(ppage, buf);
                    disk_[{ownerPid, ownerVpage}] = std::move(buf);
                }
                // Page is no longer in physical memory.
                entry.isValid  = false;
                entry.physPage = PageEntry::UNMAPPED;
                break;
            }
        }
        physPageOwner_.erase(ownerIt);
    }

    mmu_.removeLRUEntry(ppage);
    // physUsed[ppage] deliberately stays true; caller reuses this frame directly.
    return ppage;
}

// ─────────────────────────────────────────────────────────────────────────────
// Process cleanup
// ─────────────────────────────────────────────────────────────────────────────
void OS::cleanupProcess_(Process& p) {
    // Free every physical page owned by this process.
    for (auto it = physPageOwner_.begin(); it != physPageOwner_.end(); ) {
        if (it->second.first == p.pcb.pid) {
            mmu_.freePhysPage(it->first);   // physUsed=false, removed from LRU
            it = physPageOwner_.erase(it);
        } else {
            ++it;
        }
    }

    // Discard any disk pages belonging to this process.
    for (auto it = disk_.begin(); it != disk_.end(); )
        if (it->first.first == p.pcb.pid) it = disk_.erase(it); else ++it;

    p.pcb.pageTable.clear();
    p.pcb.workingSetPages.clear();
    p.pcb.heapAllocations.clear();
    p.pcb.state = ProcState::Terminated;

    // Release any locks this process held.
    for (int i = 0; i < NUM_LOCKS; ++i) {
        if (locks_[i].held && locks_[i].ownerPid == p.pcb.pid) {
            if (!locks_[i].waitQueue.empty()) {
                size_t nextIdx = locks_[i].waitQueue.front();
                locks_[i].waitQueue.pop_front();
                locks_[i].ownerPid = processes_[nextIdx].pcb.pid;
                enqueueReady_(nextIdx);
            } else {
                locks_[i].held     = false;
                locks_[i].ownerPid = 0;
            }
        }
    }
}

ProcState OS::getProcessState(uint32_t pid) const {
    for (const auto& p : processes_)
        if (p.pcb.pid == pid) return p.pcb.state;
    throw std::runtime_error("OS::getProcessState: PID not found");
}

void OS::enqueueReady_(size_t procIndex) {
    uint32_t prio = processes_[procIndex].pcb.priority;
    if (prio < 1)  prio = 1;
    if (prio > 32) prio = 32;
    processes_[procIndex].pcb.state = ProcState::Ready;
    ready_[prio].push_back(procIndex);
}

int OS::pickNextReady_() {
    for (uint32_t prio = 1; prio <= 32; ++prio)
        if (!ready_[prio].empty()) {
            size_t idx = ready_[prio].front();
            ready_[prio].pop_front();
            return static_cast<int>(idx);
        }
    return -1;
}

void OS::tickSleepers_() {
    size_t n = sleeping_.size();
    for (size_t i = 0; i < n; ++i) {
        size_t idx = sleeping_.front();
        sleeping_.pop_front();
        Process& p = processes_[idx];
        if (p.pcb.state != ProcState::WaitingSleep) continue;
        if (p.pcb.sleepCounter > 0) p.pcb.sleepCounter--;
        if (p.pcb.sleepCounter == 0) enqueueReady_(idx);
        else sleeping_.push_back(idx);
    }
}

void OS::reportStats() const {
    std::cout << "\n=== Process Statistics ===\n";
    for (const auto& p : processes_) {
        std::cout << "PID: "               << p.pcb.pid
                  << "  Priority: "        << p.pcb.priority
                  << "  State: "           << static_cast<int>(p.pcb.state)
                  << "  Cycles: "          << p.pcb.cycles
                  << "  ContextSwitches: " << p.pcb.contextSwitches
                  << "  SleepCounter: "    << p.pcb.sleepCounter
                  << "  HeapAllocs: "      << p.pcb.heapAllocations.size()
                  << '\n';
    }
}

// =============================================================================
// Module 5 — Heap Allocation Handlers  (updated for Module 6 PageEntry)
// =============================================================================

void OS::handleAlloc_(size_t procIndex) {
    Process& p = processes_[procIndex];

    uint32_t sizeReg  = p.pcb.trapRegA;
    uint32_t addrReg  = p.pcb.trapRegB;
    int64_t  reqBytes = p.pcb.regs[sizeReg];

    if (reqBytes <= 0) {
        std::cerr << "OS: Alloc called with non-positive size by PID " << p.pcb.pid << "\n";
        p.pcb.regs[addrReg] = 0;
        enqueueReady_(procIndex);
        return;
    }

    uint32_t pagesNeeded   = (static_cast<uint32_t>(reqBytes) + MMU::PAGE_SIZE - 1) / MMU::PAGE_SIZE;
    uint32_t heapFirstPage = p.pcb.heapStart / MMU::PAGE_SIZE;
    uint32_t heapLastPage  = (p.pcb.heapEnd - 1) / MMU::PAGE_SIZE;
    uint32_t totalPages    = heapLastPage - heapFirstPage + 1;

    if (pagesNeeded > totalPages) {
        std::cerr << "OS: Alloc request exceeds heap size for PID " << p.pcb.pid << "\n";
        p.pcb.regs[addrReg] = 0;
        enqueueReady_(procIndex);
        return;
    }

    if (heapLastPage >= p.pcb.pageTable.size())
        p.pcb.pageTable.resize(heapLastPage + 1, PageEntry{});

    // Find a contiguous run of completely-free (not in RAM, not on disk) heap pages.
    uint32_t runStart = 0, runLen = 0;
    bool found = false;
    for (uint32_t vp = heapFirstPage; vp <= heapLastPage; ++vp) {
        const PageEntry& e = p.pcb.pageTable[vp];
        bool inUse = e.isValid ||
                     (e.physPage != PageEntry::UNMAPPED) ||
                     (disk_.count({p.pcb.pid, vp}) > 0);
        if (!inUse) {
            if (runLen == 0) runStart = vp;
            if (++runLen == pagesNeeded) { found = true; break; }
        } else {
            runLen = 0;
        }
    }

    if (!found) {
        std::cerr << "OS: Alloc failed — no contiguous free heap pages for PID " << p.pcb.pid << "\n";
        p.pcb.regs[addrReg] = 0;
        enqueueReady_(procIndex);
        return;
    }

    // Allocate physical pages, evicting LRU when necessary.
    for (uint32_t i = 0; i < pagesNeeded; ++i) {
        uint32_t vp = runStart + i;
        uint32_t pp;
        try {
            pp = mmu_.allocPhysPage();
        } catch (...) {
            pp = evictOnePage_();
            mmu_.zeroPhysPage(pp);
        }
        p.pcb.pageTable[vp].physPage = pp;
        p.pcb.pageTable[vp].isValid  = true;
        p.pcb.pageTable[vp].isDirty  = false;
        physPageOwner_[pp] = {p.pcb.pid, vp};
        mmu_.touchPhysPage(pp);
        p.pcb.workingSetPages.push_back(vp);
    }

    uint32_t allocVAddr = runStart * MMU::PAGE_SIZE;
    p.pcb.heapAllocations[allocVAddr] = pagesNeeded;
    p.pcb.regs[addrReg] = static_cast<int64_t>(allocVAddr);
    enqueueReady_(procIndex);
}

void OS::handleFreeMemory_(size_t procIndex) {
    Process& p = processes_[procIndex];

    uint32_t vaddr = static_cast<uint32_t>(p.pcb.regs[p.pcb.trapRegA]);
    auto it = p.pcb.heapAllocations.find(vaddr);

    if (it == p.pcb.heapAllocations.end()) {
        std::cerr << "OS: FreeMemory called with unknown address 0x"
                  << std::hex << vaddr << std::dec
                  << " by PID " << p.pcb.pid << "\n";
        enqueueReady_(procIndex);
        return;
    }

    uint32_t pagesFreed = it->second;
    uint32_t firstVPage = vaddr / MMU::PAGE_SIZE;

    for (uint32_t i = 0; i < pagesFreed; ++i) {
        uint32_t vp = firstVPage + i;
        if (vp >= p.pcb.pageTable.size()) continue;

        PageEntry& entry = p.pcb.pageTable[vp];
        if (entry.isValid && entry.physPage != PageEntry::UNMAPPED) {
            mmu_.freePhysPage(entry.physPage);       // physUsed=false, LRU removed
            physPageOwner_.erase(entry.physPage);
        }
        disk_.erase({p.pcb.pid, vp});  // remove any swapped version
        entry = PageEntry{};            // fully unmapped
    }

    p.pcb.heapAllocations.erase(it);
    enqueueReady_(procIndex);
}