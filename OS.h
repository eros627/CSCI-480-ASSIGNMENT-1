#ifndef OS_H
#define OS_H

#include <vector>
#include <deque>
#include <cstdint>
#include "MMU.H"
#include "CPU.H"
#include "process.h"
#include "PROGRAM.H"

class OS {
public:
    OS(size_t physMemBytes);

    uint32_t createProcessFromAsm(const std::string& path, uint32_t priority);
    void run();

private:

    static constexpr uint32_t CODE_BASE  = 0x1000;
    static constexpr uint32_t DATA_BASE  = 0x4000;
    static constexpr uint32_t HEAP_START = 0x6000;
    static constexpr uint32_t STACK_TOP  = 0x9000;
    static constexpr uint32_t STACK_MAX  = 0x1000;

    MMU mmu_;
    CPU cpu_;
    uint32_t nextPid_ = 1;

    std::vector<Process> procs_;

    // ready queues by priority 1..32 (index 1..32)
    std::vector<std::deque<uint32_t>> ready_;

    // waiting sleepers store pid
    std::deque<uint32_t> sleeping_;

    void loadProcessImage_(Process& p, const Program& prog);
    void enqueueReady_(uint32_t pid);
    int pickNextReadyPid_();
    void contextSwitchTo_(Process& p);
    void tickSleepers_();
    
    Process& byPid_(uint32_t pid);
};

#endif // OS_H