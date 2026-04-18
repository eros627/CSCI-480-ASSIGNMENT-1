#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "MMU.H"
#include "PCB.h"
#include "PROGRAM.H"
#include "OS.h"

static int passed = 0;
static int failed = 0;

void check(bool condition, const std::string& msg) {
    if (!condition) {
        throw std::runtime_error(msg);
    }
}

void run_test(const std::string& name, void (*fn)()) {
    try {
        fn();
        std::cout << "[PASS] " << name << '\n';
        passed++;
    } catch (const std::exception& ex) {
        std::cout << "[FAIL] " << name << " -> " << ex.what() << '\n';
        failed++;
    } catch (...) {
        std::cout << "[FAIL] " << name << " -> unknown error\n";
        failed++;
    }
}

void write_file(const std::string& path, const std::string& contents) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Could not create file: " + path);
    }
    out << contents;
}

void test_program_load() {
    write_file("hello.asm",
        "movi r1, #5\n"
        "printr r1\n"
        "exit\n"
    );

    Program prog;
    prog.loadFromFile("hello.asm");

    check(!prog.bytes().empty(), "assembled program is empty");
    check(prog.sizeBytes() % 12 == 0, "program size is not multiple of 12");
}

void test_mmu_basic() {
    MMU mmu(4096);

    PCB pcb;
    pcb.codeBase = 0x1000;
    pcb.codeSize = 256;
    pcb.stackTop = 0x2000;
    pcb.stackMax = 256;
    pcb.pageTable.assign(64, PCB::UNMAPPED);

    uint32_t vpage = pcb.codeBase / MMU::PAGE_SIZE;
    pcb.pageTable[vpage] = mmu.allocPhysPage();

    mmu.setPageTable(&pcb.pageTable);
    mmu.setBounds(pcb.codeBase, pcb.codeSize, 0, 0, 0, 0, pcb.stackTop, pcb.stackMax);

    mmu.write8(0x1000, 0xAB);
    check(mmu.read8(0x1000) == 0xAB, "MMU read/write mismatch");
}

void test_mmu_fault() {
    MMU mmu(4096);

    PCB pcb;
    pcb.codeBase = 0x1000;
    pcb.codeSize = 256;
    pcb.stackTop = 0x2000;
    pcb.stackMax = 256;
    pcb.pageTable.assign(64, PCB::UNMAPPED);

    mmu.setPageTable(&pcb.pageTable);
    mmu.setBounds(pcb.codeBase, pcb.codeSize, 0, 0, 0, 0, pcb.stackTop, pcb.stackMax);

    bool threw = false;
    try {
        mmu.read8(0x1000);
    } catch (...) {
        threw = true;
    }

    check(threw, "expected unmapped page fault");
}

void test_stack_program() {
    write_file("stack_test.asm",
        "movi r1, #42\n"
        "pushr r1\n"
        "popr r2\n"
        "printr r2\n"
        "exit\n"
    );

    OS os(64 * 1024);
    os.createProcessFromAsm("stack_test.asm", 1);
    os.run();
}

void test_single_process() {
    write_file("single.asm",
        "movi r1, #99\n"
        "printr r1\n"
        "exit\n"
    );

    OS os(64 * 1024);
    os.createProcessFromAsm("single.asm", 1);
    os.run();
}

void test_two_processes() {
    write_file("proc_a.asm",
        "movi r1, #111\n"
        "printr r1\n"
        "exit\n"
    );

    write_file("proc_b.asm",
        "movi r1, #222\n"
        "printr r1\n"
        "exit\n"
    );

    OS os(64 * 1024);
    os.createProcessFromAsm("proc_a.asm", 1);
    os.createProcessFromAsm("proc_b.asm", 1);
    os.run();
}

void test_cross_page_program() {
    std::ofstream out("cross.asm");
    if (!out) {
        throw std::runtime_error("Could not create cross.asm");
    }

    for (int i = 1; i <= 30; i++) {
        out << "movi r1, #" << i << "\n";
        out << "printr r1\n";
    }
    out << "exit\n";
    out.close();

    OS os(64 * 1024);
    os.createProcessFromAsm("cross.asm", 1);
    os.run();
}

void test_unmapped_data_fault() {
    write_file("bad_data.asm",
        "movi r1, #16384\n"
        "movi r2, #77\n"
        "movrm r1, r2\n"
        "exit\n"
    );

    OS os(64 * 1024);
    os.createProcessFromAsm("bad_data.asm", 1);
    os.run();

    check(os.getProcessState(1) == ProcState::Terminated,
          "process should be terminated after unmapped data fault");
}

void test_process_terminates_cleanly() {
    write_file("clean_exit.asm",
        "movi r1, #5\n"
        "printr r1\n"
        "exit\n"
    );

    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("clean_exit.asm", 1);
    os.run();

    check(os.getProcessState(pid) == ProcState::Terminated,
          "process should be terminated after normal exit");
}

void test_sleep_switching() {
    write_file("sleep_test.asm",
        "movi r1, #2\n"
        "sleep r1\n"
        "movi r2, #7\n"
        "printr r2\n"
        "exit\n"
    );

    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("sleep_test.asm", 1);
    os.run();

    check(os.getProcessState(pid) == ProcState::Terminated,
          "sleeping process should eventually wake and terminate");
}

void test_priority_scheduling() {
    write_file("low.asm",
        "movi r1, #1\n"
        "printr r1\n"
        "exit\n"
    );

    write_file("high.asm",
        "movi r1, #2\n"
        "printr r1\n"
        "exit\n"
    );

    OS os(64 * 1024);

    // lower numeric value = higher priority
    os.createProcessFromAsm("low.asm", 10);
    os.createProcessFromAsm("high.asm", 1);

    os.run();

    check(os.getProcessState(1) == ProcState::Terminated,
          "low priority process should terminate");
    check(os.getProcessState(2) == ProcState::Terminated,
          "high priority process should terminate");
}

void test_stats_reporting() {
    write_file("stats.asm",
        "movi r1, #3\n"
        "printr r1\n"
        "exit\n"
    );

    OS os(64 * 1024);
    os.createProcessFromAsm("stats.asm", 1);
    os.run();
    os.reportStats();

    check(os.getProcessState(1) == ProcState::Terminated,
          "stats test process should terminate");
}

void test_shared_memory() {
    write_file("proc_writer.asm",
        "movi r1, #0\n"
        "mapsharedmem r1, r2\n"
        "movi r3, #42\n"
        "movrm r2, r3\n"
        "exit\n"
    );

    write_file("proc_reader.asm",
        "movi r1, #5\n"
        "sleep r1\n"
        "movi r1, #0\n"
        "mapsharedmem r1, r2\n"
        "movmr r3, r2\n"
        "printr r3\n"
        "exit\n"
    );

    OS os(64 * 1024);
    os.createProcessFromAsm("proc_writer.asm", 1);
    os.createProcessFromAsm("proc_reader.asm", 1);
    os.run();

    check(os.getProcessState(1) == ProcState::Terminated, "writer should terminate");
    check(os.getProcessState(2) == ProcState::Terminated, "reader should terminate");
}

void test_locks() {
    write_file("lock_test_a.asm",
        "acquirelocki 0\n"
        "movi r1, #0\n"
        "mapsharedmem r1, r2\n"
        "movmr r3, r2\n"
        "addi r3, #1\n"
        "movrm r2, r3\n"
        "printr r3\n"
        "releaselocki 0\n"
        "exit\n"
    );

    write_file("lock_test_b.asm",
        "acquirelocki 0\n"
        "movi r1, #0\n"
        "mapsharedmem r1, r2\n"
        "movmr r3, r2\n"
        "addi r3, #1\n"
        "movrm r2, r3\n"
        "printr r3\n"
        "releaselocki 0\n"
        "exit\n"
    );

    OS os(64 * 1024);
    os.createProcessFromAsm("lock_test_a.asm", 1);
    os.createProcessFromAsm("lock_test_b.asm", 1);
    os.run();

    check(os.getProcessState(1) == ProcState::Terminated, "lock_a should terminate");
    check(os.getProcessState(2) == ProcState::Terminated, "lock_b should terminate");
}

void test_events() {
    write_file("event_waiter.asm",
        "waiteventI 0\n"
        "movi r1, #99\n"
        "printr r1\n"
        "exit\n"
    );

    write_file("event_signaler.asm",
        "movi r1, #5\n"
        "sleep r1\n"
        "signaleventI 0\n"
        "exit\n"
    );

    OS os(64 * 1024);
    os.createProcessFromAsm("event_waiter.asm", 1);
    os.createProcessFromAsm("event_signaler.asm", 1);
    os.run();

    check(os.getProcessState(1) == ProcState::Terminated, "waiter should terminate");
    check(os.getProcessState(2) == ProcState::Terminated, "signaler should terminate");
}

// ─── Module 5: Heap Allocation Tests ─────────────────────────────────────────

void test_heap_alloc_basic() {
    write_file("heap_alloc_basic.asm",
        "movi r1, #100\n"     // request 100 bytes (rounds up to 1 page)
        "alloc r1, r2\n"      // r2 = heap address
        "cmpi r2, #0\n"
        "jei #24\n"           // jump to failure if r2==0
        "movi r3, #42\n"
        "movrm r2, r3\n"      // mem[r2] = 42
        "movmr r4, r2\n"      // r4 = mem[r2]
        "printr r4\n"         // should print 42
        "freememory r2\n"
        "exit\n"
        "movi r5, #0\n"
        "printr r5\n"
        "exit\n"
    );
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("heap_alloc_basic.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated,
          "heap_alloc_basic: process should terminate cleanly");
}

void test_heap_alloc_fail() {
    write_file("heap_alloc_fail.asm",
        "movi r1, #8192\n"   // 8 KB > 4 KB heap — must fail
        "alloc r1, r2\n"
        "printr r2\n"        // should print 0
        "exit\n"
    );
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("heap_alloc_fail.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated,
          "heap_alloc_fail: process should terminate cleanly");
}

void test_heap_realloc() {
    write_file("heap_realloc.asm",
        "movi r1, #256\n"     // exactly 1 page
        "alloc r1, r2\n"
        "movi r3, #99\n"
        "movrm r2, r3\n"
        "movmr r4, r2\n"
        "printr r4\n"         // should print 99
        "freememory r2\n"
        "alloc r1, r5\n"      // re-alloc — freed page should be reused
        "cmpi r5, #0\n"
        "jei #12\n"
        "printr r5\n"         // print non-zero address
        "exit\n"
        "movi r6, #0\n"
        "printr r6\n"
        "exit\n"
    );
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("heap_realloc.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated,
          "heap_realloc: process should terminate cleanly");
}


int main() {
    run_test("program_load", test_program_load);
    run_test("mmu_basic", test_mmu_basic);
    run_test("mmu_fault", test_mmu_fault);
    run_test("single_process", test_single_process);
    run_test("stack_program", test_stack_program);
    run_test("two_processes", test_two_processes);
    run_test("cross_page_program", test_cross_page_program);
    run_test("unmapped_data_fault", test_unmapped_data_fault);
    run_test("process_terminates_cleanly", test_process_terminates_cleanly);
    run_test("sleep_switching", test_sleep_switching);
    run_test("priority_scheduling", test_priority_scheduling);
    run_test("stats_reporting", test_stats_reporting);
    run_test("shared_memory",test_shared_memory);
    run_test("locks",test_locks);
    run_test("events",test_events);
    run_test("heap_alloc_basic",  test_heap_alloc_basic);
    run_test("heap_alloc_fail",   test_heap_alloc_fail);
    run_test("heap_realloc",      test_heap_realloc);

    std::cout << "\nPassed: " << passed << '\n';
    std::cout << "Failed: " << failed << '\n';

    return failed == 0 ? 0 : 1;
}