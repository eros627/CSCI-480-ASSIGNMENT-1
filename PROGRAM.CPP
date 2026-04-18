#include "PROGRAM.H"
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
 
// ---------- Public API ----------
 
void Program::loadFromFile(const std::string& path) {
    bytes_.clear();
 
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Could not open program file: " + path);
    }
 
    std::string line;
    while (std::getline(in, line)) {
        line = stripComment(line);
        line = trim(line);
 
        if (line.empty()) {
            continue;
        }
 
        assembleLine(line);
    }
 
    if (bytes_.empty()) {
        throw std::runtime_error("Program file assembled to empty output: " + path);
    }
}
 
const std::vector<uint8_t>& Program::bytes() const {
    return bytes_;
}
 
uint32_t Program::sizeBytes() const {
    return static_cast<uint32_t>(bytes_.size());
}
 
void Program::loadIntoMemory(MMU& mmu, uint32_t baseAddr) const {
    for (uint32_t i = 0; i < bytes_.size(); ++i) {
        mmu.write8(baseAddr + i, bytes_[i]);
    }
}
 
// ---------- Assembler ----------
 
void Program::assembleLine(const std::string& line) {
    std::vector<std::string> toks = tokenize(line);
    if (toks.empty()) {
        return;
    }
 
    std::string mnem = toLower(toks[0]);
    Opcode op = parseMnemonic(mnem);
 
    uint32_t opA = 0;
    uint32_t opB = 0;
 
    auto needTok = [&](size_t idx, const char* what) {
        if (toks.size() <= idx) {
            throw std::runtime_error(std::string("Missing ") + what + " in line: " + line);
        }
    };
 
    switch (op) {
        // 0-operand instructions
        case Opcode::Exit:
        case Opcode::Ret:
            break;
 
        // 1 register operand
        case Opcode::Incr:
        case Opcode::Printr:
        case Opcode::Printm:
        case Opcode::Printcr:
        case Opcode::Printcm:
        case Opcode::Pushr:
        case Opcode::Popr:
        case Opcode::Popm:
        case Opcode::Jmp:
        case Opcode::Jlt:
        case Opcode::Jgt:
        case Opcode::Je:
        case Opcode::Call:
        case Opcode::Callm:
        case Opcode::Sleep:
        case Opcode::Input:
        case Opcode::Inputc:
        case Opcode::AcquireLock:
        case Opcode::ReleaseLock:
        case Opcode::SignalEvent:
        case Opcode::WaitEvent:
            needTok(1, "operand");
            opA = parseRegister(toLower(toks[1]));
            break;
 
        // 1 immediate operand (using # prefix, e.g. jmpi #5)
        case Opcode::Jmpi:
        case Opcode::Jmpa:
        case Opcode::Jlti:
        case Opcode::Jlta:
        case Opcode::Jgti:
        case Opcode::Jgta:
        case Opcode::Jei:
        case Opcode::Jea:
        case Opcode::Pushi:
        case Opcode::Setpriorityi:
            needTok(1, "immediate");
            opA = parseImmediate(toks[1]);
            break;
 
        // 1 immediate operand (raw integer, e.g. acquirelocki 0)
        case Opcode::AcquireLockI:
        case Opcode::ReleaseLockI:
        case Opcode::SignalEventI:
        case Opcode::WaitEventI:
            needTok(1, "immediate");
            opA = static_cast<uint32_t>(std::stoi(toks[1]));
            break;
 
        // register + immediate
        case Opcode::Movi:
        case Opcode::Addi:
        case Opcode::Cmpi:
        case Opcode::Setpriority:
            needTok(1, "register");
            needTok(2, "immediate");
            opA = parseRegister(toLower(toks[1]));
            opB = parseImmediate(toks[2]);
            break;
 
        // register + register
        case Opcode::Movr:
        case Opcode::Movmr:
        case Opcode::Movrm:
        case Opcode::Movmm:
        case Opcode::Addr:
        case Opcode::Cmpr:
        case Opcode::MapSharedMem:
            needTok(1, "register A");
            needTok(2, "register B");
            opA = parseRegister(toLower(toks[1]));
            opB = parseRegister(toLower(toks[2]));
            break;
 
        default:
            throw std::runtime_error("Assembler: opcode not handled in line: " + line);
    }
 
    emit32(static_cast<uint32_t>(op));
    emit32(opA);
    emit32(opB);
}
 
void Program::emit32(uint32_t w) {
    bytes_.push_back(static_cast<uint8_t>((w >> 0) & 0xFF));
    bytes_.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
    bytes_.push_back(static_cast<uint8_t>((w >> 16) & 0xFF));
    bytes_.push_back(static_cast<uint8_t>((w >> 24) & 0xFF));
}
 
// ---------- Parsing helpers ----------
 
std::string Program::stripComment(const std::string& s) {
    size_t pos = s.find(';');
    if (pos == std::string::npos) {
        return s;
    }
    return s.substr(0, pos);
}
 
std::string Program::trim(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
 
    size_t j = s.size();
    while (j > i && std::isspace(static_cast<unsigned char>(s[j - 1]))) {
        --j;
    }
 
    return s.substr(i, j - i);
}
 
std::string Program::toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}
 
std::vector<std::string> Program::tokenize(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
 
    auto flush = [&]() {
        if (!cur.empty()) {
            out.push_back(cur);
            cur.clear();
        }
    };
 
    for (char ch : s) {
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == ',') {
            flush();
        } else {
            cur.push_back(ch);
        }
    }
 
    flush();
    return out;
}
 
uint32_t Program::parseRegister(const std::string& tok) {
    if (tok.size() < 2 || tok[0] != 'r') {
        throw std::runtime_error("Expected register like r1, got: " + tok);
    }
 
    int n = std::stoi(tok.substr(1));
    if (n < 1 || n > 10) {
        throw std::runtime_error("Register out of range r1..r10: " + tok);
    }
 
    return static_cast<uint32_t>(n);
}
 
uint32_t Program::parseImmediate(const std::string& tok) {
    if (tok.size() < 2) {
        throw std::runtime_error("Bad immediate: " + tok);
    }
 
    if (tok[0] == '#') {
        int64_t v = std::stoll(tok.substr(1));
        return static_cast<uint32_t>(static_cast<int32_t>(v));
    }
 
    if (tok[0] == '@') {
        if (tok.size() != 2) {
            throw std::runtime_error("Char immediate must look like @a : " + tok);
        }
        return static_cast<uint32_t>(static_cast<uint8_t>(tok[1]));
    }
 
    // Fallback: raw integer (e.g. used by acquirelocki, releaselocki, etc.)
    try {
        int64_t v = std::stoll(tok);
        return static_cast<uint32_t>(static_cast<int32_t>(v));
    } catch (...) {}
 
    throw std::runtime_error("Immediate must start with # or @, got: " + tok);
}
 
Opcode Program::parseMnemonic(const std::string& mnem) {
    static const std::unordered_map<std::string, Opcode> map = {
        {"incr", Opcode::Incr},
        {"addi", Opcode::Addi},
        {"addr", Opcode::Addr},
 
        {"pushr", Opcode::Pushr},
        {"pushi", Opcode::Pushi},
        {"popr",  Opcode::Popr},
        {"popm",  Opcode::Popm},
 
        {"movi",  Opcode::Movi},
        {"movr",  Opcode::Movr},
        {"movmr", Opcode::Movmr},
        {"movrm", Opcode::Movrm},
        {"movmm", Opcode::Movmm},
 
        {"printr",  Opcode::Printr},
        {"printm",  Opcode::Printm},
        {"printcr", Opcode::Printcr},
        {"printcm", Opcode::Printcm},
 
        {"jmp",  Opcode::Jmp},
        {"jmpi", Opcode::Jmpi},
        {"jmpa", Opcode::Jmpa},
 
        {"cmpi", Opcode::Cmpi},
        {"cmpr", Opcode::Cmpr},
 
        {"jlt",  Opcode::Jlt},
        {"jlti", Opcode::Jlti},
        {"jlta", Opcode::Jlta},
 
        {"jgt",  Opcode::Jgt},
        {"jgti", Opcode::Jgti},
        {"jgta", Opcode::Jgta},
 
        {"je",   Opcode::Je},
        {"jei",  Opcode::Jei},
        {"jea",  Opcode::Jea},
 
        {"call",  Opcode::Call},
        {"callm", Opcode::Callm},
        {"ret",   Opcode::Ret},
 
        {"exit",  Opcode::Exit},
        {"sleep", Opcode::Sleep},
 
        {"input",  Opcode::Input},
        {"inputc", Opcode::Inputc},
 
        {"setpriority",  Opcode::Setpriority},
        {"setpriorityi", Opcode::Setpriorityi},
 
        // Assignment 4 - Shared Memory
        {"mapsharedmem", Opcode::MapSharedMem},
 
        // Assignment 4 - Locks
        {"acquirelock",  Opcode::AcquireLock},
        {"acquirelocki", Opcode::AcquireLockI},
        {"releaselock",  Opcode::ReleaseLock},
        {"releaselocki", Opcode::ReleaseLockI},
 
        // Assignment 4 - Events
        {"signalevent",  Opcode::SignalEvent},
        {"signaleventi", Opcode::SignalEventI},
        {"waitevent",    Opcode::WaitEvent},
        {"waiteventi",   Opcode::WaitEventI},
    };
 
    auto it = map.find(mnem);
    if (it == map.end()) {
        throw std::runtime_error("Unknown mnemonic: " + mnem);
    }
 
    return it->second;
}