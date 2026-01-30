#include "MMU.H"

uint8_t MMU::read8(uint32_t addr) const 
{
    return physMem.read8(addr);
}

void MMU::write8(uint32_t addr, uint8_t val) 
{
    physMem.write8(addr, val);
}

uint32_t MMU::read32(uint32_t addr) const // Little-endian read
{
    uint32_t val = 0;
    val |= static_cast<uint32_t>(physMem.read8(addr)) << 0;
    val |= static_cast<uint32_t>(physMem.read8(addr + 1)) << 8;
    val |= static_cast<uint32_t>(physMem.read8(addr + 2)) << 16;
    val |= static_cast<uint32_t>(physMem.read8(addr + 3)) << 24;
    return val;
}

void MMU::write32(uint32_t addr, uint32_t val) // Little-endian write
{
    physMem.write8(addr, static_cast<uint8_t>((val >> 0) & 0xFF));
    physMem.write8(addr + 1, static_cast<uint8_t>((val >> 8) & 0xFF));
    physMem.write8(addr + 2, static_cast<uint8_t>((val >> 16) & 0xFF));
    physMem.write8(addr + 3, static_cast<uint8_t>((val >> 24) & 0xFF));
}
