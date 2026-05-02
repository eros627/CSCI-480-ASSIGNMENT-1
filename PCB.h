#pragma once
#include <cstdint>
#include <array>
#include <limits>
#include <unordered_map>
#include <vector>
#include "procstate.h"
#include "pageentry.h"   //PageEntry replaces raw uint32_t page table entries

struct PCB {
    // Identity
    uint32_t pid = 0;

    // State & scheduling
    ProcState state       = ProcState::New;
    uint32_t  priority    = 16;
    uint32_t  timeQuantum = 50;
    uint64_t  cycles          = 0;
    uint64_t  contextSwitches = 0;
    uint32_t  sleepCounter    = 0;

    // Assignment 4 blocking fields
    int waitingLockId  = -1;
    int waitingEventId = -1;

    // CPU snapshot
    std::array<int64_t, 16> regs{};
    uint32_t ip       = 0;
    uint32_t sp       = 0;
    bool zeroFlag     = false;
    bool signFlag     = false;

    // Module 5 trap operand registers
    uint32_t trapRegA = 0;
    uint32_t trapRegB = 0;

    // Virtual memory layout
    uint32_t codeBase = 0, codeSize = 0;
    uint32_t dataBase = 0, dataSize = 0;
    uint32_t heapStart = 0, heapEnd = 0;
    uint32_t stackTop  = 0, stackMax = 0;

    // page table now uses PageEntry (isValid + isDirty flags)
    // workingSetPages tracks virtual page numbers (not physical) so eviction
    // does not invalidate the list.
    std::vector<PageEntry> pageTable;
    std::vector<uint32_t>  workingSetPages;  // stores virtual page numbers

    // Heap allocation table (Module 5)
    std::unordered_map<uint32_t, uint32_t> heapAllocations;
};