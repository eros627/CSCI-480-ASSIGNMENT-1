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
};

#endif