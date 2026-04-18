#include "CPU.H"
#include "trap.h"
#include "ENUMOPCODE.H"
#include <iostream>
#include <stdexcept>
#include <string>

CPU::CPU(MMU& mmu) : mmu(mmu) { regs_.fill(0); }

void CPU::SetIP(uint32_t ip) { ip_ = ip; }
void CPU::SetSP(uint32_t sp) { sp_ = sp; }

int64_t CPU::getReg(uint32_t r) const { checkReg(r); return regs_[r]; }

Trap CPU::Run(uint64_t maxsteps) {
    running_ = true;
    for (uint64_t steps = 0; steps < maxsteps; ++steps) {
        Trap t = step();
        if (t != Trap::None) return t;
    }
    return Trap::QuantumExpired;
}

Trap CPU::step() {
    uint32_t rawOpcode = 0, opA = 0, opB = 0;
    fetch(rawOpcode, opA, opB);
    return execute(static_cast<Opcode>(rawOpcode), opA, opB);
}

void CPU::checkReg(uint32_t r) const {
    if (r < 1 || r > 10) throw std::out_of_range("Invalid register index");
}

void CPU::fetch(uint32_t& rawOpcode, uint32_t& opA, uint32_t& opB) {
    rawOpcode = mmu.read32(ip_);
    opA       = mmu.read32(ip_ + 4);
    opB       = mmu.read32(ip_ + 8);
    ip_ += 12;
}

Trap CPU::execute(Opcode opcode, uint32_t opA, uint32_t opB) {
    switch (opcode) {
        case Opcode::Exit:   running_ = false; return Trap::Exit;
        case Opcode::Sleep:  checkReg(opA);    return Trap::Sleep;

        // ── Module 5 ─────────────────────────────────────────────────────────
        case Opcode::Alloc:
            checkReg(opA); checkReg(opB);
            trapRegA_ = opA;  // rx: byte count
            trapRegB_ = opB;  // ry: result address
            return Trap::Alloc;

        case Opcode::FreeMemory:
            checkReg(opA);
            trapRegA_ = opA;  // rx: address to free
            return Trap::FreeMemory;

        // ── Assignment 4 ──────────────────────────────────────────────────────
        case Opcode::MapSharedMem:
            checkReg(opA); checkReg(opB);
            regs_[15] = opB;
            return Trap::MapSharedMem;

        case Opcode::AcquireLock:
            checkReg(opA);
            return Trap::AcquireLock;

        case Opcode::AcquireLockI:
            regs_[1] = opA;
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

        // ── Data movement ────────────────────────────────────────────────────
        case Opcode::Movi:
            checkReg(opA);
            regs_[opA] = static_cast<int64_t>(static_cast<int32_t>(opB));
            return Trap::None;

        case Opcode::Movr:
            checkReg(opA); checkReg(opB);
            regs_[opA] = regs_[opB];
            return Trap::None;

        case Opcode::Movmr: {
            checkReg(opA); checkReg(opB);
            regs_[opA] = static_cast<int32_t>(mmu.read32(static_cast<uint32_t>(regs_[opB])));
            return Trap::None;
        }

        case Opcode::Movrm: {
            checkReg(opA); checkReg(opB);
            mmu.write32(static_cast<uint32_t>(regs_[opA]),
                        static_cast<uint32_t>(regs_[opB] & 0xFFFFFFFF));
            return Trap::None;
        }

        case Opcode::Movmm: {
            checkReg(opA); checkReg(opB);
            mmu.write32(static_cast<uint32_t>(regs_[opA]),
                        mmu.read32(static_cast<uint32_t>(regs_[opB])));
            return Trap::None;
        }

        // ── Arithmetic ───────────────────────────────────────────────────────
        case Opcode::Incr:
            checkReg(opA); regs_[opA] += 1; return Trap::None;

        case Opcode::Addi:
            checkReg(opA);
            regs_[opA] += static_cast<int64_t>(static_cast<int32_t>(opB));
            return Trap::None;

        case Opcode::Addr:
            checkReg(opA); checkReg(opB);
            regs_[opA] += regs_[opB];
            return Trap::None;

        // ── Output ───────────────────────────────────────────────────────────
        case Opcode::Printr:
            checkReg(opA);
            std::cout << regs_[opA] << '\n';
            return Trap::None;

        case Opcode::Printm: {
            checkReg(opA);
            std::cout << static_cast<int32_t>(mmu.read32(static_cast<uint32_t>(regs_[opA]))) << '\n';
            return Trap::None;
        }

        case Opcode::Printcr:
            checkReg(opA);
            std::cout << static_cast<char>(regs_[opA] & 0xFF) << '\n';
            return Trap::None;

        case Opcode::Printcm: {
            checkReg(opA);
            std::cout << static_cast<char>(mmu.read8(static_cast<uint32_t>(regs_[opA]))) << '\n';
            return Trap::None;
        }

        // ── Control flow ─────────────────────────────────────────────────────
        case Opcode::Jmp:
            checkReg(opA);
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            return Trap::None;
        case Opcode::Jmpi:
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            return Trap::None;
        case Opcode::Jmpa:
            ip_ = opA; return Trap::None;

        case Opcode::Cmpi: {
            checkReg(opA);
            int64_t imm = static_cast<int32_t>(opB);
            zeroFlag_ = (regs_[opA] == imm);
            signFlag_ = (regs_[opA] <  imm);
            return Trap::None;
        }
        case Opcode::Cmpr: {
            checkReg(opA); checkReg(opB);
            zeroFlag_ = (regs_[opA] == regs_[opB]);
            signFlag_ = (regs_[opA] <  regs_[opB]);
            return Trap::None;
        }

        case Opcode::Jlt:
            checkReg(opA);
            if (signFlag_) ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            return Trap::None;
        case Opcode::Jlti:
            if (signFlag_) ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            return Trap::None;
        case Opcode::Jlta:
            if (signFlag_) ip_ = opA;
            return Trap::None;

        case Opcode::Jgt:
            checkReg(opA);
            if (!signFlag_ && !zeroFlag_) ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            return Trap::None;
        case Opcode::Jgti:
            if (!signFlag_ && !zeroFlag_) ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            return Trap::None;
        case Opcode::Jgta:
            if (!signFlag_ && !zeroFlag_) ip_ = opA;
            return Trap::None;

        case Opcode::Je:
            checkReg(opA);
            if (zeroFlag_) ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            return Trap::None;
        case Opcode::Jei:
            if (zeroFlag_) ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + static_cast<int32_t>(opA));
            return Trap::None;
        case Opcode::Jea:
            if (zeroFlag_) ip_ = opA;
            return Trap::None;

        case Opcode::Call:
            checkReg(opA);
            sp_ -= 4; mmu.write32(sp_, ip_);
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) + regs_[opA]);
            return Trap::None;
        case Opcode::Callm: {
            checkReg(opA);
            sp_ -= 4; mmu.write32(sp_, ip_);
            ip_ = static_cast<uint32_t>(static_cast<int64_t>(ip_) +
                  static_cast<int32_t>(mmu.read32(static_cast<uint32_t>(regs_[opA]))));
            return Trap::None;
        }
        case Opcode::Ret:
            ip_ = mmu.read32(sp_); sp_ += 4; return Trap::None;

        // ── Stack ────────────────────────────────────────────────────────────
        case Opcode::Pushi:
            sp_ -= 4; mmu.write32(sp_, opA); return Trap::None;
        case Opcode::Pushr:
            checkReg(opA);
            sp_ -= 4; mmu.write32(sp_, static_cast<uint32_t>(regs_[opA] & 0xFFFFFFFF));
            return Trap::None;
        case Opcode::Popr:
            checkReg(opA);
            regs_[opA] = static_cast<int32_t>(mmu.read32(sp_)); sp_ += 4;
            return Trap::None;
        case Opcode::Popm: {
            checkReg(opA);
            uint32_t v = mmu.read32(sp_); sp_ += 4;
            mmu.write32(static_cast<uint32_t>(regs_[opA]), v);
            return Trap::None;
        }

        // ── Input ────────────────────────────────────────────────────────────
        case Opcode::Input:   checkReg(opA); return Trap::Input;
        case Opcode::Inputc:  checkReg(opA); return Trap::Inputc;
        case Opcode::Setpriority:
        case Opcode::Setpriorityi:
            return Trap::None;

        default:
            throw std::runtime_error(
                "Unknown opcode: " + std::to_string(static_cast<uint32_t>(opcode)));
    }
}

void CPU::saveTo(PCB& out) const {
    out.regs     = regs_;
    out.ip       = ip_;
    out.sp       = sp_;
    out.zeroFlag = zeroFlag_;
    out.signFlag = signFlag_;
    out.trapRegA = trapRegA_;
    out.trapRegB = trapRegB_;
}

void CPU::loadFrom(const PCB& in) {
    regs_     = in.regs;
    ip_       = in.ip;
    sp_       = in.sp;
    zeroFlag_ = in.zeroFlag;
    signFlag_ = in.signFlag;
    trapRegA_ = in.trapRegA;
    trapRegB_ = in.trapRegB;
}