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
