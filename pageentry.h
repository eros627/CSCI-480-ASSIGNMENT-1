#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>

// ─── Page table entry ────────────────────────────────────────────────────────
// Each virtual page in a process's page table is described by one of these.
// physPage == UNMAPPED   →  never allocated
// physPage == UNMAPPED + !isValid →  swapped to disk
// physPage  < UNMAPPED + isValid  →  in physical memory
struct PageEntry {
    static constexpr uint32_t UNMAPPED = std::numeric_limits<uint32_t>::max();
    uint32_t physPage = UNMAPPED;  // which physical page frame holds this data
    bool     isValid  = false;     // is the page currently in physical memory?
    bool     isDirty  = false;     // has the page been written since last load?
};

// ─── Page fault exception ────────────────────────────────────────────────────
// Thrown by MMU::translate when a virtual page is not currently in physical
// memory (isValid == false).  The OS catches this, brings the page in, and
// re-queues the faulting process so it can retry the instruction.
class PageFaultException : public std::exception {
public:
    uint32_t vpage;
    explicit PageFaultException(uint32_t vp) : vpage(vp) {}
    const char* what() const noexcept override { return "Page fault"; }
};