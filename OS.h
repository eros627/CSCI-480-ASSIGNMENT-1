#ifndef OS_H
#define OS_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "MMU.H"
#include "CPU.H"
#include "process.h"
#include "PROGRAM.H"
#include "procstate.h"

class OS {
public:
    explicit OS(size_t physMemBytes = 64 * 1024);

    uint32_t createProcessFromAsm(const std::string& asmFile, uint32_t priority);
    void run();
    ProcState getProcessState(uint32_t pid) const;
    void reportStats() const;

private:
    MMU mmu_;
    CPU cpu_;
    uint32_t nextPid_ = 1;

    std::vector<Process> processes_;

    // index 1..32 used
    std::vector<std::deque<size_t>> ready_;
    std::deque<size_t> sleeping_;

    void loadProcessImage_(Process& p, const Program& prog);
    void contextSwitchTo_(Process& p);
    void contextSwitchOut_(Process& p);
    void cleanupProcess_(Process& p);

    void enqueueReady_(size_t procIndex);
    int pickNextReady_();
    void tickSleepers_();

    // Shared memory
    static constexpr uint32_t NUM_SHARED_REGIONS = 2;
    static constexpr uint32_t SHARED_MEM_VBASE   = 0xE000; // virtual addr processes use
    std::vector<uint32_t> sharedPhysPages_;  // physical pages for shared regions

    // Locks
    static constexpr int NUM_LOCKS = 10;
    struct Lock 
    {
        bool held = false;
        uint32_t ownerPid = 0;
        std::deque<size_t> waitQueue;  // process indices waiting
    };
    std::array<Lock, NUM_LOCKS> locks_;

    // Events
    static constexpr int NUM_EVENTS = 10;
    struct Event 
    {
        bool signaled = false;
        std::deque<size_t> waitQueue;  // process indices waiting
    };
    std::array<Event, NUM_EVENTS> events_;
};

#endif