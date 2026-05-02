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
    if (!condition) throw std::runtime_error(msg);
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
    if (!out) throw std::runtime_error("Could not create file: " + path);
    out << contents;
}

// ─── Original tests (unchanged logic, updated for PageEntry) ─────────────────

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
    pcb.codeBase = 0x1000; pcb.codeSize = 256;
    pcb.stackTop = 0x2000; pcb.stackMax = 256;
    // page table uses PageEntry, not raw uint32_t
    pcb.pageTable.assign(64, PageEntry{});

    uint32_t vpage = pcb.codeBase / MMU::PAGE_SIZE;
    uint32_t pp    = mmu.allocPhysPage();
    pcb.pageTable[vpage].physPage = pp;
    pcb.pageTable[vpage].isValid  = true;

    mmu.setPageTable(&pcb.pageTable);
    mmu.setBounds(pcb.codeBase, pcb.codeSize, 0, 0, 0, 0, pcb.stackTop, pcb.stackMax);

    mmu.write8(0x1000, 0xAB);
    check(mmu.read8(0x1000) == 0xAB, "MMU read/write mismatch");
}

void test_mmu_fault() {
    MMU mmu(4096);

    PCB pcb;
    pcb.codeBase = 0x1000; pcb.codeSize = 256;
    pcb.stackTop = 0x2000; pcb.stackMax = 256;
    pcb.pageTable.assign(64, PageEntry{});  // all entries invalid

    mmu.setPageTable(&pcb.pageTable);
    mmu.setBounds(pcb.codeBase, pcb.codeSize, 0, 0, 0, 0, pcb.stackTop, pcb.stackMax);

    bool threw = false;
    try { mmu.read8(0x1000); } catch (...) { threw = true; }
    check(threw, "expected page fault on unmapped page");
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
    if (!out) throw std::runtime_error("Could not create cross.asm");
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
    write_file("low.asm",  "movi r1, #1\nprintr r1\nexit\n");
    write_file("high.asm", "movi r1, #2\nprintr r1\nexit\n");
    OS os(64 * 1024);
    os.createProcessFromAsm("low.asm",  10);
    os.createProcessFromAsm("high.asm",  1);
    os.run();
    check(os.getProcessState(1) == ProcState::Terminated, "low priority should terminate");
    check(os.getProcessState(2) == ProcState::Terminated, "high priority should terminate");
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
    check(os.getProcessState(1) == ProcState::Terminated, "stats test process should terminate");
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

// ─── Module 5 ─────────────────────────────────────────────────────────────────

void test_heap_alloc_basic() {
    write_file("heap_alloc_basic.asm",
        "movi r1, #100\n"
        "alloc r1, r2\n"
        "cmpi r2, #0\n"
        "jei #24\n"
        "movi r3, #42\n"
        "movrm r2, r3\n"
        "movmr r4, r2\n"
        "printr r4\n"
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
        "movi r1, #8192\n"
        "alloc r1, r2\n"
        "printr r2\n"
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
        "movi r1, #256\n"
        "alloc r1, r2\n"
        "movi r3, #99\n"
        "movrm r2, r3\n"
        "movmr r4, r2\n"
        "printr r4\n"
        "freememory r2\n"
        "alloc r1, r5\n"
        "cmpi r5, #0\n"
        "jei #12\n"
        "printr r5\n"
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

// ─── Module 6: Virtual Memory Tests ──────────────────────────────────────────

// Test 1: Lazy stack allocation
// Stack pages start invalid; first push triggers a page fault that the OS
// handles transparently.  Process must complete correctly.
void test_vm_lazy_stack() {
    write_file("vm_lazy_stack.asm",
        "movi r1, #42\n"
        "pushr r1\n"       // triggers stack page fault
        "popr r2\n"
        "printr r2\n"      // should print 42
        "exit\n"
    );
    // 8 pages: 2 shared + 1 code + stack/heap lazily added
    OS os(8 * MMU::PAGE_SIZE);
    uint32_t pid = os.createProcessFromAsm("vm_lazy_stack.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated,
          "vm_lazy_stack: process should terminate after lazy stack fault");
}

// Test 2: Multiple page faults — heap allocs across multiple pages
// Verifies the OS handles many page faults in one process without crashing.
void test_vm_multi_fault() {
    write_file("vm_multi_fault.asm",
        "movi r1, #256\n"
        "alloc r1, r2\n"      // heap page A
        "movi r3, #77\n"
        "movrm r2, r3\n"
        "movi r1, #256\n"
        "alloc r1, r4\n"      // heap page B
        "movi r3, #88\n"
        "movrm r4, r3\n"
        "movmr r5, r2\n"      // read back A
        "printr r5\n"         // 77
        "movmr r6, r4\n"      // read back B
        "printr r6\n"         // 88
        "freememory r2\n"
        "freememory r4\n"
        "exit\n"
    );
    OS os(12 * MMU::PAGE_SIZE);
    uint32_t pid = os.createProcessFromAsm("vm_multi_fault.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated,
          "vm_multi_fault: process should terminate cleanly");
}

// Test 3: Two processes with tight physical memory
// Both must complete correctly even when sharing a constrained physical pool.
void test_vm_two_process_tight() {
    write_file("vm_tight_a.asm",
        "movi r1, #1\n"
        "movi r2, #3\n"
        "sleep r2\n"
        "printr r1\n"
        "exit\n"
    );
    write_file("vm_tight_b.asm",
        "movi r1, #2\n"
        "printr r1\n"
        "exit\n"
    );
    // Tight: 2 shared + 1 code/process + lazy stack per process
    OS os(8 * MMU::PAGE_SIZE);
    uint32_t pidA = os.createProcessFromAsm("vm_tight_a.asm", 1);
    uint32_t pidB = os.createProcessFromAsm("vm_tight_b.asm", 1);
    os.run();
    check(os.getProcessState(pidA) == ProcState::Terminated,
          "vm_two_process_tight: process A should terminate");
    check(os.getProcessState(pidB) == ProcState::Terminated,
          "vm_two_process_tight: process B should terminate");
}

// Test 4: Dirty-page writeback + swap-in correctness
//
// Memory budget: 5 physical pages total
//   pp0, pp1  = shared (pinned, never evicted)
//   pp2       = process A code
//   pp3       = process B code
//   pp4       = one free page
//
// Scenario:
//   Process A  allocs heap, writes 55, then sleeps.
//   Process B  does three heap allocs that exceed the free pool,
//              forcing eviction of A's pages (including the dirty heap page).
//   Process A  wakes, reads back the heap value — must get 55.
//
// This verifies: dirty pages are written to disk_ on eviction and correctly
// restored on page-fault swap-in.
void test_vm_dirty_writeback() {
    write_file("vm_writer.asm",
        "movi r1, #256\n"
        "alloc r1, r2\n"    // allocate one heap page (will become dirty)
        "movi r3, #55\n"
        "movrm r2, r3\n"    // write 55 -> page is now dirty
        "movi r4, #10\n"
        "sleep r4\n"        // yield; filler will run and evict our pages
        "movmr r5, r2\n"    // read back — must trigger swap-in, must equal 55
        "printr r5\n"
        "exit\n"
    );
    write_file("vm_filler.asm",
        // Three allocs force eviction of A's code page and heap page
        "movi r1, #256\n"
        "alloc r1, r2\n"
        "movi r1, #256\n"
        "alloc r1, r3\n"
        "movi r1, #256\n"
        "alloc r1, r4\n"
        "exit\n"
    );
    // 5 pages total: tight enough to force eviction when filler allocs
    OS os(5 * MMU::PAGE_SIZE);
    uint32_t pidA = os.createProcessFromAsm("vm_writer.asm",  1);
    uint32_t pidB = os.createProcessFromAsm("vm_filler.asm",  1);
    os.run();
    check(os.getProcessState(pidA) == ProcState::Terminated,
          "vm_dirty_writeback: writer should terminate (data restored from disk)");
    check(os.getProcessState(pidB) == ProcState::Terminated,
          "vm_dirty_writeback: filler should terminate");
}

// ─── main ────────────────────────────────────────────────────────────────────

int main() {
    run_test("program_load",              test_program_load);
    run_test("mmu_basic",                 test_mmu_basic);
    run_test("mmu_fault",                 test_mmu_fault);
    run_test("single_process",            test_single_process);
    run_test("stack_program",             test_stack_program);
    run_test("two_processes",             test_two_processes);
    run_test("cross_page_program",        test_cross_page_program);
    run_test("unmapped_data_fault",       test_unmapped_data_fault);
    run_test("process_terminates_cleanly",test_process_terminates_cleanly);
    run_test("sleep_switching",           test_sleep_switching);
    run_test("priority_scheduling",       test_priority_scheduling);
    run_test("stats_reporting",           test_stats_reporting);
    run_test("shared_memory",             test_shared_memory);
    run_test("locks",                     test_locks);
    run_test("events",                    test_events);
    run_test("heap_alloc_basic",          test_heap_alloc_basic);
    run_test("heap_alloc_fail",           test_heap_alloc_fail);
    run_test("heap_realloc",              test_heap_realloc);
    // Module 6 — Virtual Memory
    run_test("vm_lazy_stack",             test_vm_lazy_stack);
    run_test("vm_multi_fault",            test_vm_multi_fault);
    run_test("vm_two_process_tight",      test_vm_two_process_tight);
    run_test("vm_dirty_writeback",        test_vm_dirty_writeback);

    std::cout << "\nPassed: " << passed << '\n';
    std::cout << "Failed: " << failed << '\n';
    return failed == 0 ? 0 : 1;
}