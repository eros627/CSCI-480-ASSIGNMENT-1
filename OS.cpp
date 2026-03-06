#include <iostream>
#include "MMU.H"
#include "CPU.H"
#include "PROGRAM.H"
#include "OS.h"
#include "process.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " program.asm\n";
        return 1;
    }
   // std::cerr << "OS: Loading program file: " << argv[1] << "\n";
    MMU mmu(64 * 1024);
    CPU cpu(mmu);

    Program prog;
    prog.loadFromFile(argv[1]);

    //std::cerr << "OS: Program size (bytes): " << prog.sizeBytes() << "\n";

    uint32_t base = 0x1000;
    uint32_t stackTop = 0x9000;

    prog.loadIntoMemory(mmu, base);

    cpu.SetIP(base);
    cpu.SetSP(stackTop);

    cpu.Run();
    return 0;

    
}

uint32_t OS::createProcessFromAsm(const std::string& path, uint32_t priority) {
    Program prog;
    prog.loadFromFile(path);

    Process p;
    p.name = path;
    p.pcb.pid = nextPid_++;
    p.pcb.priority = priority;
    p.pcb.timeQuantum = 50;
    p.pcb.state = ProcState::New;

    // segment layout
    p.pcb.codeBase = CODE_BASE;
    p.pcb.codeSize = static_cast<uint32_t>(prog.sizeBytes()); // you may add sizeBytes()

    p.pcb.dataBase = DATA_BASE;
    p.pcb.dataSize = 0; // if not using yet

    p.pcb.heapStart = HEAP_START;
    p.pcb.heapEnd   = HEAP_START; // empty heap for now

    p.pcb.stackTop = STACK_TOP;
    p.pcb.stackMax = STACK_MAX;

    loadProcessImage_(p, prog);

    // init CPU state in PCB
    p.pcb.ip = p.pcb.codeBase;
    p.pcb.sp = p.pcb.stackTop;
    p.pcb.regs.fill(0);

    p.pcb.state = ProcState::Ready;

    procs_.push_back(std::move(p));
    enqueueReady_(procs_.back().pcb.pid);
    return procs_.back().pcb.pid;
}

void OS::loadProcessImage_(Process& p, const Program& prog) 
{

    p.pcb.pageTable.clear();

    // temporarily point MMU to THIS process page table
    mmu_.setPageTable(&p.pcb.pageTable);
    mmu_.setBounds(p.pcb.codeBase, p.pcb.codeSize,
                   p.pcb.dataBase, p.pcb.dataSize,
                   p.pcb.heapStart, p.pcb.heapEnd,
                   p.pcb.stackTop, p.pcb.stackMax);
}

void OS::run() 
{
    while (true) 
    {
        tickSleepers_();

        int pid = pickNextReadyPid_();
        if (pid < 0) {
            // no ready processes; if all terminated -> done
            bool anyAlive = false;
            for (auto& p : procs_) if (p.pcb.state != ProcState::Terminated) anyAlive = true;
            if (!anyAlive) break;
            // else idle tick
            continue;
        }

        Process& p = byPid_(pid);
        contextSwitchTo_(p);

        // run for one slice
        p.pcb.state = ProcState::Running;

        uint32_t quantum = p.pcb.timeQuantum;
        while (quantum-- > 0) 
        {
            // step once
            // for now, you can check state changes via flags or throw exceptions
            Trap t = cpu_.step(); // execute one instruction and get trap info 
            p.pcb.cycles++;

            // if CPU executed Exit, mark terminated and break
            // if CPU executed Sleep/Input, break and set waiting state
            if (t.type == TrapType::Sleep) 
            {
                p.pcb.sleepCounter = t.value;     // 0 means indefinite (handle separately)
                p.pcb.state = ProcState::WaitingSleep;
                sleeping_.push_back(p.pcb.pid);
                break; // end slice now
            }
        }

        // preempt if still running after quantum:
        if (p.pcb.state == ProcState::Running) {
            p.pcb.state = ProcState::Ready;
            enqueueReady_(p.pcb.pid);
        }

        // save state back to PCB happens in contextSwitchOut
        cpu_.saveTo(p.pcb);
        p.pcb.contextSwitches++;
    }


    // report stats
}
void OS::tickSleepers_() 
{
    for (size_t i = 0; i < sleeping_.size(); ) 
    {
        Process& p = byPid_(sleeping_[i]);
        if (p.pcb.sleepCounter > 0) p.pcb.sleepCounter--;

        if (p.pcb.sleepCounter == 0) 
        {
            p.pcb.state = ProcState::Ready;
            enqueueReady_(p.pcb.pid);
            sleeping_.erase(sleeping_.begin() + i);
        } else 
        {
            ++i;
        }
    }
}
