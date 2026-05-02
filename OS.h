#ifndef OS_H
#define OS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "MMU.H"
#include "CPU.H"
#include "process.h"
#include "PROGRAM.H"
#include "procstate.h"

class OS {
public:
    explicit OS(size_t physMemBytes = 64 * 1024);
    uint32_t  createProcessFromAsm(const std::string& asmFile, uint32_t priority);
    void      run();
    ProcState getProcessState(uint32_t pid) const;
    void      reportStats() const;

private:
    MMU mmu_;
    CPU cpu_;
    uint32_t nextPid_ = 1;
    std::vector<Process> processes_;
    std::vector<std::deque<size_t>> ready_;   // index 1..32
    std::deque<size_t> sleeping_;

    void loadProcessImage_(Process& p, const Program& prog);
    void contextSwitchTo_(Process& p);
    void contextSwitchOut_(Process& p);
    void cleanupProcess_(Process& p);
    void enqueueReady_(size_t procIndex);
    int  pickNextReady_();
    void tickSleepers_();

    // Shared memory (Assignment 4)
    static constexpr uint32_t NUM_SHARED_REGIONS = 2;
    static constexpr uint32_t SHARED_MEM_VBASE   = 0xE000;
    std::vector<uint32_t> sharedPhysPages_;

    // Locks (Assignment 4)
    static constexpr int NUM_LOCKS = 10;
    struct Lock {
        bool held = false;
        uint32_t ownerPid = 0;
        std::deque<size_t> waitQueue;
    };
    std::array<Lock, NUM_LOCKS> locks_;

    // Events (Assignment 4)
    static constexpr int NUM_EVENTS = 10;
    struct Event {
        bool signaled = false;
        std::deque<size_t> waitQueue;
    };
    std::array<Event, NUM_EVENTS> events_;

    // Heap allocation (Module 5)
    void handleAlloc_(size_t procIndex);
    void handleFreeMemory_(size_t procIndex);

    // ── Module 6: Virtual Memory ──────────────────────────────────────────────
    // Simulated disk: (pid, vpage) → saved page bytes
    std::map<std::pair<uint32_t,uint32_t>, std::vector<uint8_t>> disk_;

    // Reverse map: physical page → (pid, vpage) that currently owns it
    // Shared OS pages are NOT in this map (they are pinned, never evicted).
    std::unordered_map<uint32_t, std::pair<uint32_t,uint32_t>> physPageOwner_;

    // Called when CPU throws PageFaultException: brings vpage into RAM.
    void     handlePageFault_(size_t procIndex, uint32_t vpage);

    // Evict the LRU non-pinned physical page; save to disk_ if dirty.
    // Returns the reclaimed physical page number (physUsed stays true).
    uint32_t evictOnePage_();
};

#endif