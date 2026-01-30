#include <iostream>
#include "MMU.H"
#include "CPU.H"
#include "PROGRAM.H"

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
