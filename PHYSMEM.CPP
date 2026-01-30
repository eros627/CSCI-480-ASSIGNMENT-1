#include "PHYSMEM.H"

using namespace std;

PhysMem :: PhysMem(size_t sizebytes) : mem(sizebytes, 0) // Initialize physical memory with the specified size, filled with zeros
{
}

void PhysMem::bounds_check(uint32_t addr) const // Check address bounds
{
    if (addr >= mem.size()) 
    {
        throw out_of_range("Address out of bounds");
    }
}

uint8_t PhysMem::read8(uint32_t addr) const
{
    bounds_check(addr); // Ensure address is within bounds
    return mem[addr]; // Return the 8-bit value at the specified address
}

void PhysMem::write8(uint32_t addr, uint8_t val)
{
    bounds_check(addr); // Ensure address is within bounds
    mem[addr] = val; // Write the 8-bit value to the specified address
}

