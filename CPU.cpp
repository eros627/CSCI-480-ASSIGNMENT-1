#include "CPU.H"
#include <iostream>
#include <stdexcept>

CPU:: CPU(MMU & mmu) : mmu(mmu) 
{
    regs_.fill(0); // Initialize all registers to 0
}

void CPU::SetIP(uint32_t ip) 
{
    ip_ = ip; // Set the instruction pointer
}
void CPU::SetSP(uint32_t sp) 
{
    sp_ = sp;
}

int64_t CPU::getReg(uint32_t r) const 
{
    checkReg(r);
    return regs_[r];
}

void CPU :: Run(uint64_t maxsteps) // Execute instructions
{
    running_ = true;

    for (uint64_t steps = 0; steps < maxsteps && running_; ++steps) 
    {
        step();
    }

    if (running_) {
        throw std::runtime_error("CPU halted: maxSteps reached (possible infinite loop)");
    }
}

void CPU :: step() //  Execute a single instruction
{
    uint32_t rawOpcode = 0, opA = 0, opB = 0;
    fetch(rawOpcode, opA, opB);

    Opcode opcode = static_cast<Opcode>(rawOpcode);
    execute(opcode, opA, opB);
}

void CPU :: checkReg(uint32_t r) const 
{
    if (r < 1 || r > 10) {
        throw std::out_of_range("Invalid register index");
    }
}

void CPU :: fetch(uint32_t& rawOpcode, uint32_t& opA, uint32_t& opB) 
{
    rawOpcode = mmu.read32(ip_);// Fetch opcode
    opA = mmu.read32(ip_ + 4);// Fetch operand A
    opB = mmu.read32(ip_ + 8);// Fetch operand B
    
    ip_ += 12; // Move IP to next instruction
}

void CPU :: execute(Opcode opcode, uint32_t opA, uint32_t opB) 
{
    switch (opcode) 
    {
        case Opcode::Exit:
            running_ = false; // Stop execution
            return;
        
        case Opcode::Sleep:
            // Sleep operation not implemented yet
            break;
        case Opcode:: Movi:
            checkReg(opA);
            regs_[opA] = static_cast<int64_t>(opB); // Move immediate
            break;
        case Opcode::Movr:
            checkReg(opA);
            checkReg(opB);
            regs_[opA] = regs_[opB]; // Move register
            break;
        case Opcode::Movmr: 
        {
            checkReg(opA); // destination rx
            checkReg(opB); // address register ry
            uint32_t addr = static_cast<uint32_t>(regs_[opB]);   // address in ry
            regs_[opA] = static_cast<int32_t>(mmu.read32(addr)); // load 32-bit into rx (sign-extend)
            break;
        }
        case Opcode::Movrm: 
        {
            checkReg(opA); // address register rx
            checkReg(opB); // source register ry
            uint32_t addr = static_cast<uint32_t>(regs_[opA]);   // address in rx
            mmu.write32(addr, static_cast<uint32_t>(regs_[opB] & 0xFFFFFFFF));
            break;
        }
        case Opcode :: Movmm: 
        {
            checkReg(opA); // destination address register rx
            checkReg(opB); // source address register ry

            uint32_t addrDest = static_cast<uint32_t>(regs_[opA]); // destination address in rx
            uint32_t addrSrc = static_cast<uint32_t>(regs_[opB]);  // source address in ry
            uint32_t val = mmu.read32(addrSrc);
            mmu.write32(addrDest, val);
            break;
        }
        case Opcode::Incr:
            checkReg(opA);
            regs_[opA] += 1; // Increment register
            break;
        case Opcode::Addi:
            checkReg(opA);
            regs_[opA] += static_cast<int64_t>(opB); // Add immediate
            break;
        case Opcode::Addr:
            checkReg(opA);
            checkReg(opB);
            regs_[opA] += regs_[opB]; // Add register
            break;
        case Opcode::Printr:
            checkReg(opA);
            std::cout << regs_[opA] << std::endl; // Print register
            break;
        case Opcode::Printm:
        {
            checkReg(opA);  // address register rx
            uint32_t addr = static_cast<uint32_t>(regs_[opA]); // address
            uint32_t val = mmu.read32(addr);
            std::cout << static_cast<int32_t>(val) << std::endl; // Print memory value (sign-extended)
            break;
        }
        case Opcode::Printcr:
            checkReg(opA);
            std::cout << static_cast<char>(regs_[opA] & 0xFF) << std::endl; // Print char from register
            break;
        case Opcode::Printcm:
        {
            checkReg(opA);  // address register rx
            uint32_t addr = static_cast<uint32_t>(regs_[opA]); // address
            uint8_t val = mmu.read8(addr);
            std::cout << static_cast<char>(val) << std::endl; // Print char from memory
            break;
        }
        //control flow opcode cases below
        case Opcode::Jmp:
            checkReg(opA);
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            break;
        case Opcode::Jmpi:
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            break;
        case Opcode::Jmpa:
            ip_ = opA; // Absolute jump
            break;
        case Opcode::Cmpi:
        {
            checkReg(opA);
            int64_t regVal = regs_[opA];
            int64_t imm = static_cast<int32_t>(opB); // sign-extend
            zeroFlag_ = (regVal == imm);
            signFlag_ = (regVal < imm);
            break;
        }
        case Opcode::Cmpr:
            checkReg(opA);
            checkReg(opB);
            {
                int64_t regAVal = regs_[opA];
                int64_t regBVal = regs_[opB];
                zeroFlag_ = (regAVal == regBVal);
                signFlag_ = (regAVal < regBVal);
            }
            break;
        case Opcode::Jlt:
            checkReg(opA);
            if (signFlag_) 
            {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            }
            break;

        case Opcode::Jlti: 
        {
            if (signFlag_) {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            }
            break;
        }
        case Opcode::Jlta:
            if (signFlag_) 
            {
                ip_ = opA; // Absolute jump
            }
            break;
        case Opcode::Jgt:
            checkReg(opA);
            if (!signFlag_ && !zeroFlag_) 
            {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            }
            break;
        case Opcode::Jgti:
            checkReg(opA);
            if (!signFlag_ && !zeroFlag_) 
            {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            }
            break;
        case Opcode::Jgta:
            checkReg(opA);
            if (!signFlag_ && !zeroFlag_) 
            {
                ip_ = opA; // Absolute jump
            }
            break;
        case Opcode::Je:
            checkReg(opA);
            if (zeroFlag_) 
            {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            }
            break;
        case Opcode::Jei:
            checkReg(opA);
            if (zeroFlag_) 
            {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            }
            break;
        case Opcode::Jea:
            checkReg(opA);
            if (zeroFlag_) 
            {
                ip_ = opA; // Absolute jump
            }
            break;
        case Opcode::Call:
            checkReg(opA);
            sp_ -= 4;
            mmu.write32(sp_, ip_); // Push return address
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            break;
        case Opcode::Callm:
        {
            checkReg(opA);
            sp_ -= 4;
            mmu.write32(sp_, ip_); // return address 

            uint32_t addr = static_cast<uint32_t>(regs_[opA]); // address held in rx
            int32_t offset = static_cast<int32_t>(mmu.read32(addr)); // [rx]
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + offset);
            break;
        }
        case Opcode::Ret:
        {
            ip_ = mmu.read32(sp_); // Pop return address
            sp_ += 4;
            break;
        }
        //stack opcode cases below
        case Opcode::Pushi:
            sp_ -= 4;
            mmu.write32(sp_, opA);
            break;
        case Opcode::Pushr:
            checkReg(opA);
            sp_ -= 4;
            mmu.write32(sp_, static_cast<uint32_t>(regs_[opA] & 0xFFFFFFFF));
            break;
        case Opcode::Popr:
            checkReg(opA);
            regs_[opA] = static_cast<int32_t>(mmu.read32(sp_));
            sp_ += 4;
            break;
        case Opcode::Popm:
        {
            checkReg(opA);
            uint32_t v = mmu.read32(sp_);
            sp_ += 4;

            uint32_t addr = static_cast<uint32_t>(regs_[opA]); // address held in rx
            mmu.write32(addr, v);
            break;
        }
        //input opcode cases below
        case Opcode::Input:
        {
            checkReg(opA);
            int64_t inputVal;
            std::cin >> inputVal;
            regs_[opA] = static_cast<int64_t>(inputVal);
            break;
        }
        case Opcode::Inputc:
        {
            checkReg(opA);
            char ch;
            std::cin.get(ch);               // reads next character including whitespace
            if (ch == '\n') std::cin.get(ch); // skip newline if present
            regs_[opA] = static_cast<uint8_t>(ch);
            break;
        }
        case Opcode::Setpriority:           
            // Priority setting not implemented
            break;
        case Opcode::Setpriorityi:
            // Priority setting not implemented
            break;  
        
        default:
            throw std::runtime_error("Unknown opcode: " + std::to_string(static_cast<uint32_t>(opcode)));
    }

}

