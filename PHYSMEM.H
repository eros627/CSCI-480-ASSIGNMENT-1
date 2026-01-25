#ifndef PHYSMEM_H
#define PHYSMEM_H

#include <cstdint>
#include <vector>
#include <stdexcept>

using namespace std;

class PhysMem{
public:
    explicit PhysMem(size_t sizebytes); // Constructor

    uint8_t read8(uint32_t addr) const; // Read 8-bit value
    void write8(uint32_t addr, uint8_t val); // Write 8-bit value

    private:

    vector<uint8_t> mem; // Physical memory storage
    void bounds_check(uint32_t addr) const; // Check address bounds
    
    
}

#endif // PHYSMEM_H