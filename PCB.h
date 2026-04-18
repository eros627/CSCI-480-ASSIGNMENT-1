#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <limits>
#include "procstate.h"

struct PCB {
    // Identity
    uint32_t pid = 0;

    // State & scheduling
    ProcState state = ProcState::New;
    uint32_t priority = 16;          // 1..32 (lower = higher priority OR vice versa; pick one)
    uint32_t timeQuantum = 50;       // “instructions per slice” (simple)
    uint64_t cycles = 0;
    uint64_t contextSwitches = 0;
    uint32_t sleepCounter = 0;
    
    int waitingLockId  = -1;   // which lock this process is blocked on (-1 = none)
    int waitingEventId = -1;   // which event this process is blocked on (-1 = none)

    // CPU snapshot
    std::array<int64_t, 16> regs{};
    uint32_t ip = 0;
    uint32_t sp = 0;
    bool zeroFlag = false;
    bool signFlag = false;

    // Memory layout (virtual addresses)
    uint32_t codeBase = 0;
    uint32_t codeSize = 0;

    uint32_t dataBase = 0;
    uint32_t dataSize = 0;

    uint32_t heapStart = 0;
    uint32_t heapEnd   = 0;

    uint32_t stackTop  = 0;
    uint32_t stackMax  = 0;

    // Paging / isolation: vpage -> ppage map (UNMAPPED if none)
    static constexpr uint32_t UNMAPPED = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> pageTable;        // index by vpage
    std::vector<uint32_t> workingSetPages;
    
    // list of physical pages owned by process
};