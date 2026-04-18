#include "CPU.H"
#include "trap.h"
#include "ENUMOPCODE.H"
#include <iostream>
#include <stdexcept>
#include <string>

CPU::CPU(MMU& mmu) : mmu(mmu) {
    regs_.fill(0);
}

void CPU::SetIP(uint32_t ip) {
    ip_ = ip;
}

void CPU::SetSP(uint32_t sp) {
    sp_ = sp;
}

int64_t CPU::getReg(uint32_t r) const {
    checkReg(r);
    return regs_[r];
}

Trap CPU::Run(uint64_t maxsteps) {
    running_ = true;

    for (uint64_t steps = 0; steps < maxsteps; ++steps) {
        Trap t = step();
        if (t != Trap::None) {
            return t;
        }
    }

    return Trap::QuantumExpired;
}

Trap CPU::step() {
    uint32_t rawOpcode = 0;
    uint32_t opA = 0;
    uint32_t opB = 0;

    fetch(rawOpcode, opA, opB);

    Opcode opcode = static_cast<Opcode>(rawOpcode);
    return execute(opcode, opA, opB);
}

void CPU::checkReg(uint32_t r) const {
    if (r < 1 || r > 10) {
        throw std::out_of_range("Invalid register index");
    }
}

void CPU::fetch(uint32_t& rawOpcode, uint32_t& opA, uint32_t& opB) {
    rawOpcode = mmu.read32(ip_);
    opA = mmu.read32(ip_ + 4);
    opB = mmu.read32(ip_ + 8);

    ip_ += 12;
}

Trap CPU::execute(Opcode opcode, uint32_t opA, uint32_t opB) {
    switch (opcode) {
        case Opcode::Exit:
            running_ = false;
            return Trap::Exit;

        case Opcode::Sleep:
            checkReg(opA);
            return Trap::Sleep;

        case Opcode::Movi:
            checkReg(opA);
            regs_[opA] = static_cast<int64_t>(static_cast<int32_t>(opB));
            return Trap::None;

        case Opcode::Movr:
            checkReg(opA);
            checkReg(opB);
            regs_[opA] = regs_[opB];
            return Trap::None;

        case Opcode::Movmr: {
            checkReg(opA);
            checkReg(opB);
            uint32_t addr = static_cast<uint32_t>(regs_[opB]);
            regs_[opA] = static_cast<int32_t>(mmu.read32(addr));
            return Trap::None;
        }

        case Opcode::Movrm: {
            checkReg(opA);
            checkReg(opB);
            uint32_t addr = static_cast<uint32_t>(regs_[opA]);
            mmu.write32(addr, static_cast<uint32_t>(regs_[opB] & 0xFFFFFFFF));
            return Trap::None;
        }

        case Opcode::Movmm: {
            checkReg(opA);
            checkReg(opB);
            uint32_t addrDest = static_cast<uint32_t>(regs_[opA]);
            uint32_t addrSrc  = static_cast<uint32_t>(regs_[opB]);
            uint32_t val = mmu.read32(addrSrc);
            mmu.write32(addrDest, val);
            return Trap::None;
        }

        case Opcode::Incr:
            checkReg(opA);
            regs_[opA] += 1;
            return Trap::None;

        case Opcode::Addi:
            checkReg(opA);
            regs_[opA] += static_cast<int64_t>(static_cast<int32_t>(opB));
            return Trap::None;

        case Opcode::Addr:
            checkReg(opA);
            checkReg(opB);
            regs_[opA] += regs_[opB];
            return Trap::None;

        case Opcode::Printr:
            checkReg(opA);
            std::cout << regs_[opA] << '\n';
            return Trap::None;

        case Opcode::Printm: {
            checkReg(opA);
            uint32_t addr = static_cast<uint32_t>(regs_[opA]);
            uint32_t val = mmu.read32(addr);
            std::cout << static_cast<int32_t>(val) << '\n';
            return Trap::None;
        }

        case Opcode::Printcr:
            checkReg(opA);
            std::cout << static_cast<char>(regs_[opA] & 0xFF) << '\n';
            return Trap::None;

        case Opcode::Printcm: {
            checkReg(opA);
            uint32_t addr = static_cast<uint32_t>(regs_[opA]);
            uint8_t val = mmu.read8(addr);
            std::cout << static_cast<char>(val) << '\n';
            return Trap::None;
        }

        case Opcode::Jmp:
            checkReg(opA);
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            return Trap::None;

        case Opcode::Jmpi:
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            return Trap::None;

        case Opcode::Jmpa:
            ip_ = opA;
            return Trap::None;

        case Opcode::Cmpi: {
            checkReg(opA);
            int64_t regVal = regs_[opA];
            int64_t imm = static_cast<int32_t>(opB);
            zeroFlag_ = (regVal == imm);
            signFlag_ = (regVal < imm);
            return Trap::None;
        }

        case Opcode::Cmpr: {
            checkReg(opA);
            checkReg(opB);
            int64_t regAVal = regs_[opA];
            int64_t regBVal = regs_[opB];
            zeroFlag_ = (regAVal == regBVal);
            signFlag_ = (regAVal < regBVal);
            return Trap::None;
        }

        case Opcode::Jlt:
            checkReg(opA);
            if (signFlag_) {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            }
            return Trap::None;

        case Opcode::Jlti:
            if (signFlag_) {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            }
            return Trap::None;

        case Opcode::Jlta:
            if (signFlag_) {
                ip_ = opA;
            }
            return Trap::None;

        case Opcode::Jgt:
            checkReg(opA);
            if (!signFlag_ && !zeroFlag_) {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            }
            return Trap::None;

        case Opcode::Jgti:
            if (!signFlag_ && !zeroFlag_) {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            }
            return Trap::None;

        case Opcode::Jgta:
            if (!signFlag_ && !zeroFlag_) {
                ip_ = opA;
            }
            return Trap::None;

        case Opcode::Je:
            checkReg(opA);
            if (zeroFlag_) {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            }
            return Trap::None;

        case Opcode::Jei:
            if (zeroFlag_) {
                ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            }
            return Trap::None;

        case Opcode::Jea:
            if (zeroFlag_) {
                ip_ = opA;
            }
            return Trap::None;

        case Opcode::Call:
            checkReg(opA);
            sp_ -= 4;
            mmu.write32(sp_, ip_);
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            return Trap::None;

        case Opcode::Callm: {
            checkReg(opA);
            sp_ -= 4;
            mmu.write32(sp_, ip_);

            uint32_t addr = static_cast<uint32_t>(regs_[opA]);
            int32_t offset = static_cast<int32_t>(mmu.read32(addr));
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + offset);
            return Trap::None;
        }

        case Opcode::Ret:
            ip_ = mmu.read32(sp_);
            sp_ += 4;
            return Trap::None;

        case Opcode::Pushi:
            sp_ -= 4;
            mmu.write32(sp_, opA);
            return Trap::None;

        case Opcode::Pushr:
            checkReg(opA);
            sp_ -= 4;
            mmu.write32(sp_, static_cast<uint32_t>(regs_[opA] & 0xFFFFFFFF));
            return Trap::None;

        case Opcode::Popr:
            checkReg(opA);
            regs_[opA] = static_cast<int32_t>(mmu.read32(sp_));
            sp_ += 4;
            return Trap::None;

        case Opcode::Popm: {
            checkReg(opA);
            uint32_t v = mmu.read32(sp_);
            sp_ += 4;

            uint32_t addr = static_cast<uint32_t>(regs_[opA]);
            mmu.write32(addr, v);
            return Trap::None;
        }

        case Opcode::Input:
            checkReg(opA);
            return Trap::Input;

        case Opcode::Inputc:
            checkReg(opA);
            return Trap::Inputc;

        case Opcode::Setpriority:
        case Opcode::Setpriorityi:
            return Trap::None;

        default:
            throw std::runtime_error(
                "Unknown opcode: " + std::to_string(static_cast<uint32_t>(opcode))
            );
        case Opcode::MapSharedMem:
            checkReg(opA);
            checkReg(opB);
            // opA = rx (region id), opB = ry (return address register)
            // Save opB into a scratch register so OS knows where to write result
            regs_[15] = opB;  // use r15 as a scratch to pass ry index to OS
            return Trap::MapSharedMem;

        case Opcode::AcquireLock:
            checkReg(opA);
            // regs_[opA] holds lock number — OS will read it from saved PCB
            return Trap::AcquireLock;

        case Opcode::AcquireLockI:
            regs_[1] = opA;  // store immediate lock# into r1 so OS can read it uniformly
            return Trap::AcquireLock;

        case Opcode::ReleaseLock:
            checkReg(opA);
            return Trap::ReleaseLock;

        case Opcode::ReleaseLockI:
            regs_[1] = opA;
            return Trap::ReleaseLock;

        case Opcode::SignalEvent:
            checkReg(opA);
            return Trap::SignalEvent;

        case Opcode::SignalEventI:
            regs_[1] = opA;
            return Trap::SignalEvent;

        case Opcode::WaitEvent:
            checkReg(opA);
            return Trap::WaitEvent;

        case Opcode::WaitEventI:
            regs_[1] = opA;
            return Trap::WaitEvent;
    }
}

void CPU::saveTo(PCB& out) const {
    out.regs = regs_;
    out.ip = ip_;
    out.sp = sp_;
    out.zeroFlag = zeroFlag_;
    out.signFlag = signFlag_;
}

void CPU::loadFrom(const PCB& in) {
    regs_ = in.regs;
    ip_ = in.ip;
    sp_ = in.sp;
    zeroFlag_ = in.zeroFlag;
    signFlag_ = in.signFlag;
}