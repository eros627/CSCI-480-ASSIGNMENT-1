#include<iostream> // for std::cout
#include "ENUMOPCODE.H" // Include the enumOpcode definition

using namespace std;

int main() {
cout << static_cast<uint32_t>(enumOpcode::Addi) << std::endl; // Output the integer value of the Addi opcode
    return 0;
}   