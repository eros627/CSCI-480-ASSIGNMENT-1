#ifndef PROGRAM_H
#define PROGRAM_H

#include <cstdint>
#include <string>
#include <vector>

#include "MMU.H"
#include "ENUMOPCODE.H"

class Program {
public:
    // Load and assemble a program file into instruction bytes
    void loadFromFile(const std::string& path);

    // Get the encoded bytes (multiple of 12)
    const std::vector<uint8_t>& bytes() const { return bytes_; }

    // Convenience: write program bytes into memory at base address
    void loadIntoMemory(MMU& mmu, uint32_t baseAddr) const;

    // Helpful for CPU setup
    uint32_t sizeBytes() const { return static_cast<uint32_t>(bytes_.size()); }

private:
    std::vector<uint8_t> bytes_;

private:
    // Assemble one non-empty line into 12 bytes
    void assembleLine(const std::string& line);

    // Encoding helper: append a 32-bit word in little-endian
    void emit32(uint32_t w);

    // Parsing helpers
    static std::string stripComment(const std::string& s);
    static std::string trim(const std::string& s);
    static std::string toLower(std::string s);
    static std::vector<std::string> tokenize(const std::string& s);

    static uint32_t parseRegister(const std::string& tok);   // "r1" -> 1
    static uint32_t parseImmediate(const std::string& tok);  // "#12" or "@a"
    static Opcode parseMnemonic(const std::string& mnem);    // "movi" -> Opcode::Movi
};

#endif
