#include "OS.h"
#include "Assembler.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

// Constructor
OS::OS()
    : mmu_(64 * 1024), cpu_(mmu_)   // 64 KB physical memory
{
}

// Create a process from an assembly file
int OS::createProcessFromAsm(const std::string& asmFile, int pid) {
    // Assemble the source file into a Program object
    Program prog = Assembler::assembleFile(asmFile);

    // Create process
    Process p;
    p.pid = pid;
    p.state = ProcessState::READY;

    // -----------------------------
    // Define this process memory layout
    // -----------------------------
    p.pcb.codeBase  = 0x1000;
    p.pcb.codeSize  = static_cast<uint32_t>(prog.bytes().size());

    p.pcb.dataBase  = 0x4000;
    p.pcb.dataSize  = 0x1000;   // 4 KB data segment

    p.pcb.heapStart = 0x5000;
    p.pcb.heapEnd   = 0x6000;   // 4 KB heap

    p.pcb.stackTop  = 0xF000;
    p.pcb.stackMax  = 0x1000;   // 4 KB stack

    // Initial CPU register state
    p.pcb.IP = p.pcb.codeBase;
    p.pcb.SP = p.pcb.stackTop;

    // Load program into this process address space
    loadProcessImage_(p, prog);

    // Add to process table / ready queue
    processes_.push_back(p);
    readyQueue_.push_back(static_cast<int>(processes_.size() - 1));

    return pid;
}

// Load process image into paged virtual memory
void OS::loadProcessImage_(Process& p, const Program& prog) {
    // Determine highest virtual page needed
    uint32_t codeStart     = p.pcb.codeBase;
    uint32_t codeEnd       = p.pcb.codeBase + p.pcb.codeSize - 1;
    uint32_t firstCodePage = codeStart / MMU::PAGE_SIZE;
    uint32_t lastCodePage  = codeEnd / MMU::PAGE_SIZE;

    uint32_t stackLowAddr  = p.pcb.stackTop - p.pcb.stackMax;
    uint32_t stackHighAddr = p.pcb.stackTop - 1;
    uint32_t stackLowPage  = stackLowAddr / MMU::PAGE_SIZE;
    uint32_t stackHighPage = stackHighAddr / MMU::PAGE_SIZE;

    uint32_t maxVPage = std::max(lastCodePage, stackHighPage);

    // Initialize page table with all pages unmapped
    p.pcb.pageTable.assign(maxVPage + 1, PCB::UNMAPPED);
    p.pcb.workingSetPages.clear();

    // Activate this process's memory context for loading
    mmu_.setPageTable(&p.pcb.pageTable);
    mmu_.setBounds(
        p.pcb.codeBase,  p.pcb.codeSize,
        p.pcb.dataBase,  p.pcb.dataSize,
        p.pcb.heapStart, p.pcb.heapEnd,
        p.pcb.stackTop,  p.pcb.stackMax
    );

    // Allocate physical pages for code segment
    for (uint32_t vp = firstCodePage; vp <= lastCodePage; ++vp) {
        uint32_t pp = mmu_.allocPhysPage();
        p.pcb.pageTable[vp] = pp;
        p.pcb.workingSetPages.push_back(pp);
    }

    // Allocate at least one stack page
    for (uint32_t vp = stackLowPage; vp <= stackHighPage; ++vp) {
        if (p.pcb.pageTable[vp] == PCB::UNMAPPED) {
            uint32_t pp = mmu_.allocPhysPage();
            p.pcb.pageTable[vp] = pp;
            p.pcb.workingSetPages.push_back(pp);
        }
    }

    // Copy program bytes into virtual memory
    const std::vector<uint8_t>& codeBytes = prog.bytes();
    for (uint32_t i = 0; i < codeBytes.size(); ++i) {
        mmu_.write8(p.pcb.codeBase + i, codeBytes[i]);
    }
}

// Switch CPU/MMU to this process
void OS::contextSwitchTo_(Process& p) {
    mmu_.setPageTable(&p.pcb.pageTable);
    mmu_.setBounds(
        p.pcb.codeBase,  p.pcb.codeSize,
        p.pcb.dataBase,  p.pcb.dataSize,
        p.pcb.heapStart, p.pcb.heapEnd,
        p.pcb.stackTop,  p.pcb.stackMax
    );

    cpu_.loadFrom(p.pcb);
    p.state = ProcessState::RUNNING;
}

// Save CPU state back into PCB
void OS::contextSwitchOut_(Process& p) {
    cpu_.saveTo(p.pcb);

    if (p.state == ProcessState::RUNNING) {
        p.state = ProcessState::READY;
    }
}

// Very simple scheduler: round-robin over ready queue
void OS::run() {
    while (!readyQueue_.empty()) {
        int procIndex = readyQueue_.front();
        readyQueue_.erase(readyQueue_.begin());

        Process& p = processes_[procIndex];

        if (p.state == ProcessState::TERMINATED) {
            continue;
        }

        contextSwitchTo_(p);

        try {
            cpu_.run();   // or cpu_.Run(); depending on your class
            p.state = ProcessState::TERMINATED;
        }
        catch (const std::exception& ex) {
            std::cerr << "Process " << p.pid << " faulted: " << ex.what() << "\n";
            p.state = ProcessState::TERMINATED;
        }

        contextSwitchOut_(p);

        if (p.state == ProcessState::READY) {
            readyQueue_.push_back(procIndex);
        }
    }
}

// Free all physical pages owned by a terminated process
void OS::cleanupProcess_(Process& p) {
    for (uint32_t pp : p.pcb.workingSetPages) {
        mmu_.freePhysPage(pp);
    }

    p.pcb.workingSetPages.clear();
    p.pcb.pageTable.clear();
    p.state = ProcessState::TERMINATED;
}