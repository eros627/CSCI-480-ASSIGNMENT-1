#include "OS.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

OS::OS(size_t physMemBytes)
    : mmu_(physMemBytes), cpu_(mmu_), ready_(33) {
}

uint32_t OS::createProcessFromAsm(const std::string& asmFile, uint32_t priority) {
    Program prog;
    prog.loadFromFile(asmFile);

    Process p;

    p.pcb.pid = nextPid_++;
    p.pcb.priority = priority;
    p.pcb.state = ProcState::Ready;

    // Virtual memory layout
    p.pcb.codeBase  = 0x1000;
    p.pcb.codeSize  = prog.sizeBytes();

    p.pcb.dataBase  = 0x4000;
    p.pcb.dataSize  = 0x1000;

    p.pcb.heapStart = 0x5000;
    p.pcb.heapEnd   = 0x6000;

    p.pcb.stackTop  = 0xF000;
    p.pcb.stackMax  = 0x1000;

    // Initial CPU snapshot
    p.pcb.ip = p.pcb.codeBase;
    p.pcb.sp = p.pcb.stackTop;

    loadProcessImage_(p, prog);

    processes_.push_back(p);
    enqueueReady_(processes_.size() - 1);

    return p.pcb.pid;
}

void OS::loadProcessImage_(Process& p, const Program& prog) {
    if (p.pcb.codeSize == 0) {
        throw std::runtime_error("Program has no code bytes");
    }

    uint32_t codeStart     = p.pcb.codeBase;
    uint32_t codeEnd       = p.pcb.codeBase + p.pcb.codeSize - 1;
    uint32_t firstCodePage = codeStart / MMU::PAGE_SIZE;
    uint32_t lastCodePage  = codeEnd / MMU::PAGE_SIZE;

    uint32_t stackLowAddr  = p.pcb.stackTop - p.pcb.stackMax;
    uint32_t stackHighAddr = p.pcb.stackTop - 1;
    uint32_t stackLowPage  = stackLowAddr / MMU::PAGE_SIZE;
    uint32_t stackHighPage = stackHighAddr / MMU::PAGE_SIZE;

    uint32_t maxVPage = std::max(lastCodePage, stackHighPage);

    p.pcb.pageTable.assign(maxVPage + 1, PCB::UNMAPPED);
    p.pcb.workingSetPages.clear();

    mmu_.setPageTable(&p.pcb.pageTable);
    mmu_.setBounds(
        p.pcb.codeBase,  p.pcb.codeSize,
        p.pcb.dataBase,  p.pcb.dataSize,
        p.pcb.heapStart, p.pcb.heapEnd,
        p.pcb.stackTop,  p.pcb.stackMax
    );

    // Map code pages
    for (uint32_t vp = firstCodePage; vp <= lastCodePage; ++vp) {
        uint32_t pp = mmu_.allocPhysPage();
        p.pcb.pageTable[vp] = pp;
        p.pcb.workingSetPages.push_back(pp);
    }

    // Map stack pages
    for (uint32_t vp = stackLowPage; vp <= stackHighPage; ++vp) {
        if (p.pcb.pageTable[vp] == PCB::UNMAPPED) {
            uint32_t pp = mmu_.allocPhysPage();
            p.pcb.pageTable[vp] = pp;
            p.pcb.workingSetPages.push_back(pp);
        }
    }

    // Copy assembled code into this process's virtual memory
    prog.loadIntoMemory(mmu_, p.pcb.codeBase);
}

void OS::contextSwitchTo_(Process& p) {
    mmu_.setPageTable(&p.pcb.pageTable);
    mmu_.setBounds(
        p.pcb.codeBase,  p.pcb.codeSize,
        p.pcb.dataBase,  p.pcb.dataSize,
        p.pcb.heapStart, p.pcb.heapEnd,
        p.pcb.stackTop,  p.pcb.stackMax
    );

    cpu_.loadFrom(p.pcb);
    p.pcb.state = ProcState::Running;
    p.pcb.contextSwitches++;
}

void OS::contextSwitchOut_(Process& p) {
    cpu_.saveTo(p.pcb);
}

void OS::run() {
    while (true) {
        tickSleepers_();

        int next = pickNextReady_();

        // Only stop when there is nothing ready AND nothing sleeping
        if (next < 0) {
            if (sleeping_.empty()) {
                break;
            }
            continue;
        }

        size_t procIndex = static_cast<size_t>(next);
        Process& p = processes_[procIndex];

        if (p.pcb.state == ProcState::Terminated) {
            continue;
        }

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

                    if (p.pcb.regs[1] <= 0) {
                        p.pcb.sleepCounter = 1;
                    } else {
                        p.pcb.sleepCounter = static_cast<uint32_t>(p.pcb.regs[1]);
                    }

                    sleeping_.push_back(procIndex);
                    break;

                case Trap::Input:
                case Trap::Inputc:
                    p.pcb.state = ProcState::WaitingIO;
                    enqueueReady_(procIndex);
                    break;

                case Trap::None:
                    enqueueReady_(procIndex);
                    break;
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "Process " << p.pcb.pid
                      << " faulted: " << ex.what() << "\n";
            contextSwitchOut_(p);
            cleanupProcess_(p);
        }
    }
}

void OS::cleanupProcess_(Process& p) {
    for (uint32_t pp : p.pcb.workingSetPages) {
        mmu_.freePhysPage(pp);
    }

    p.pcb.workingSetPages.clear();
    p.pcb.pageTable.clear();
    p.pcb.state = ProcState::Terminated;
}

ProcState OS::getProcessState(uint32_t pid) const {
    for (const auto& p : processes_) {
        if (p.pcb.pid == pid) {
            return p.pcb.state;
        }
    }
    throw std::runtime_error("OS::getProcessState: PID not found");
}

void OS::enqueueReady_(size_t procIndex) {
    uint32_t prio = processes_[procIndex].pcb.priority;

    if (prio < 1) prio = 1;
    if (prio > 32) prio = 32;

    processes_[procIndex].pcb.state = ProcState::Ready;
    ready_[prio].push_back(procIndex);
}

int OS::pickNextReady_() {
    for (uint32_t prio = 1; prio <= 32; ++prio) {
        if (!ready_[prio].empty()) {
            size_t idx = ready_[prio].front();
            ready_[prio].pop_front();
            return static_cast<int>(idx);
        }
    }
    return -1;
}

void OS::tickSleepers_() {
    size_t n = sleeping_.size();

    for (size_t i = 0; i < n; ++i) {
        size_t idx = sleeping_.front();
        sleeping_.pop_front();

        Process& p = processes_[idx];

        if (p.pcb.state != ProcState::WaitingSleep) {
            continue;
        }

        if (p.pcb.sleepCounter > 0) {
            p.pcb.sleepCounter--;
        }

        if (p.pcb.sleepCounter == 0) {
            enqueueReady_(idx);
        } else {
            sleeping_.push_back(idx);
        }
    }
}

void OS::reportStats() const 
{
    std::cout << "\n=== Process Statistics ===\n";
    for (const auto& p : processes_) 
    {
        std::cout << "PID: " << p.pcb.pid
                  << "  Priority: " << p.pcb.priority
                  << "  State: " << static_cast<int>(p.pcb.state)
                  << "  Cycles: " << p.pcb.cycles
                  << "  ContextSwitches: " << p.pcb.contextSwitches
                  << "  SleepCounter: " << p.pcb.sleepCounter
                  << '\n';
    }
}

