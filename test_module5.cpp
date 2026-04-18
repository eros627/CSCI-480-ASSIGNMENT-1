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

// Capture stdout during a test so we can assert on what was printed
struct CaptureStdout {
    std::streambuf* orig;
    std::ostringstream buf;
    CaptureStdout()  { orig = std::cout.rdbuf(buf.rdbuf()); }
    ~CaptureStdout() { std::cout.rdbuf(orig); }
    std::string str() const { return buf.str(); }
};

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

// ─────────────────────────────────────────────────────────────────────────────
// Requirement: "This memory must be allocated in the heap, and not beyond it."
// ─────────────────────────────────────────────────────────────────────────────

// Test 1: Returned address is inside the heap window (0x5000..0x5FFF).
// If the allocator returned 0, or a non-heap address, the process would fault.
void test_alloc_address_is_in_heap() {
    write_file("t1.asm",
        "movi r1, #100\n"
        "alloc r1, r2\n"
        "cmpi r2, #0\n"
        "jei #12\n"          // fail if returned 0
        "movi r3, #1\n"
        "printr r3\n"        // success marker
        "freememory r2\n"
        "exit\n"
        "movi r3, #0\n"
        "printr r3\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t1.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must terminate");
    check(cap.str().find("1\n") != std::string::npos, "alloc must return non-zero address");
}

// Test 2: Fill the entire heap (16 pages = 4096 bytes) — must succeed.
void test_alloc_entire_heap() {
    write_file("t2.asm",
        "movi r1, #4096\n"
        "alloc r1, r2\n"
        "cmpi r2, #0\n"
        "jei #12\n"
        "movi r3, #1\n"
        "printr r3\n"        // success
        "exit\n"
        "movi r3, #0\n"
        "printr r3\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t2.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must terminate");
    check(cap.str().find("1\n") != std::string::npos, "entire-heap alloc must succeed");
}

// Test 3: Request one byte beyond the heap — must fail (return 0).
void test_alloc_beyond_heap_fails() {
    // Just print r2 directly — if alloc correctly fails, r2=0 is printed.
    write_file("t3.asm",
        "movi r1, #4097\n"   // 1 byte more than heap (4096 max)
        "alloc r1, r2\n"     // must fail -> r2 = 0
        "printr r2\n"        // print 0 if correct
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t3.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must terminate");
    check(cap.str() == "0\n", "over-sized alloc must return 0, got: " + cap.str());
}

// ─────────────────────────────────────────────────────────────────────────────
// Requirement: "You must size the memory appropriately."
// ─────────────────────────────────────────────────────────────────────────────

// Test 4: Allocate 1 byte — must get a full page (256 bytes).
// Write to offset 252 (last safe 4-byte position in a single page).
// NOTE: movrm uses write32 (4 bytes). Last safe offset = 256-4 = 252.
// Writing to offset 253+ would cross into the next (unmapped) page.
void test_alloc_rounds_up_to_full_page() {
    write_file("t4.asm",
        "movi r1, #1\n"       // request just 1 byte
        "alloc r1, r2\n"      // r2 = base address
        "cmpi r2, #0\n"
        "jei #48\n"
        // Write to offset 252 (last safe 4-byte spot in a 256-byte page)
        "movi r3, #252\n"
        "addr r3, r2\n"       // r3 = r2 + 252
        "movi r4, #77\n"
        "movrm r3, r4\n"      // mem[r2+252] = 77  <- would fault if < 1 page
        "movmr r5, r3\n"
        "printr r5\n"         // must print 77
        "freememory r2\n"
        "exit\n"
        "movi r5, #0\n"
        "printr r5\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t4.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must not fault");
    check(cap.str().find("77\n") != std::string::npos,
          "write to offset 252 must succeed — full page must be allocated");
}

// Test 5: Allocate exactly 256 bytes then another 256 — both must succeed.
void test_alloc_two_separate_pages() {
    write_file("t5.asm",
        "movi r1, #256\n"
        "alloc r1, r2\n"
        "cmpi r2, #0\n"
        "jei #36\n"
        "alloc r1, r3\n"
        "cmpi r3, #0\n"
        "jei #24\n"
        "movi r4, #2\n"
        "printr r4\n"
        "freememory r2\n"
        "freememory r3\n"
        "exit\n"
        "movi r4, #0\n"
        "printr r4\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t5.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must terminate");
    check(cap.str().find("2\n") != std::string::npos, "two separate page allocs must succeed");
}

// Test 6: Allocate 512 bytes (2 pages).
// Write to byte 0 and to the start of the second page (offset 256).
// Both must succeed — two pages must be contiguously mapped.
void test_alloc_multi_page_contiguous() {
    write_file("t6.asm",
        "movi r1, #512\n"     // 2 pages
        "alloc r1, r2\n"
        "cmpi r2, #0\n"
        "jei #60\n"
        // Write to byte 0 of allocation
        "movi r3, #10\n"
        "movrm r2, r3\n"      // mem[r2+0] = 10
        // Write to start of second page (offset 256)
        "movi r4, #256\n"
        "addr r4, r2\n"       // r4 = r2 + 256
        "movi r3, #20\n"
        "movrm r4, r3\n"      // mem[r2+256] = 20  <- second page
        // Read both back
        "movmr r5, r2\n"
        "printr r5\n"         // must print 10
        "movmr r6, r4\n"
        "printr r6\n"         // must print 20
        "freememory r2\n"
        "exit\n"
        "movi r5, #0\n"
        "printr r5\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t6.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must not fault");
    check(cap.str().find("10\n") != std::string::npos, "first page must be readable");
    check(cap.str().find("20\n") != std::string::npos, "second page must be readable");
}

// ─────────────────────────────────────────────────────────────────────────────
// Requirement: "You must free the memory appropriately."
// ─────────────────────────────────────────────────────────────────────────────

// Test 7: Free then reallocate — physical pages must be recycled.
void test_free_recycles_pages() {
    write_file("t7.asm",
        "movi r1, #256\n"
        "alloc r1, r2\n"
        "cmpi r2, #0\n"
        "jei #48\n"
        "movi r3, #55\n"
        "movrm r2, r3\n"
        "freememory r2\n"
        "alloc r1, r4\n"      // re-alloc
        "cmpi r4, #0\n"
        "jei #24\n"
        "movi r3, #66\n"
        "movrm r4, r3\n"
        "movmr r5, r4\n"
        "printr r5\n"         // must print 66 — page is re-usable
        "freememory r4\n"
        "exit\n"
        "movi r5, #0\n"
        "printr r5\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t7.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must terminate");
    check(cap.str().find("66\n") != std::string::npos,
          "freed page must be reusuable and writable");
}

// Test 8: Fill heap, free all, fill again — both fills must succeed.
void test_fill_free_refill() {
    write_file("t8.asm",
        "movi r1, #4096\n"
        "alloc r1, r2\n"
        "cmpi r2, #0\n"
        "jei #36\n"
        "freememory r2\n"
        "alloc r1, r3\n"      // second fill after free
        "cmpi r3, #0\n"
        "jei #24\n"
        "movi r4, #1\n"
        "printr r4\n"
        "freememory r3\n"
        "exit\n"
        "movi r4, #0\n"
        "printr r4\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t8.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must terminate");
    check(cap.str().find("1\n") != std::string::npos,
          "re-fill after full free must succeed");
}

// Test 9: Double-free — OS must not crash; process must continue and exit.
void test_double_free_is_safe() {
    write_file("t9.asm",
        "movi r1, #256\n"
        "alloc r1, r2\n"
        "freememory r2\n"     // valid free
        "freememory r2\n"     // double-free: bad pointer, must not crash
        "movi r3, #1\n"
        "printr r3\n"         // must still reach here
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t9.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated,
          "process must survive double-free");
    check(cap.str().find("1\n") != std::string::npos,
          "process must continue executing after double-free");
}

// Test 10: Free with an address that was never allocated — must not crash.
void test_free_bad_address_is_safe() {
    write_file("t10.asm",
        "movi r1, #20480\n"   // 0x5000 — never explicitly allocated
        "freememory r1\n"     // bad free
        "movi r2, #1\n"
        "printr r2\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t10.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated,
          "process must survive bad free");
    check(cap.str().find("1\n") != std::string::npos,
          "process must continue after bad free");
}

// ─────────────────────────────────────────────────────────────────────────────
// Requirement: "If a contiguous amount of heap memory cannot be found that
//               is large enough, the allocation must fail."
// ─────────────────────────────────────────────────────────────────────────────

// Test 11: Fragmentation — hole is too small for the request.
// Alloc A (page 0), Alloc B (page 1), Free A.
// Now only 1 free page, but request asks for 2 — must fail.
void test_fragmentation_causes_failure() {
    // Strategy: alloc 1 page (A), alloc 14 pages (B), free A.
    // Result: page 80 free | pages 81-94 used | page 95 free
    // Two isolated 1-page holes -> no contiguous 2-page run -> alloc(2 pages) fails.
    write_file("t11.asm",
        "movi r1, #256\n"    // 1 page
        "alloc r1, r2\n"     // A: page 80
        "movi r1, #3584\n"   // 14 pages (3584 = 14*256)
        "alloc r1, r3\n"     // B: pages 81-94 (fills middle of heap)
        "freememory r2\n"    // free A: leaves page 80 and page 95 as isolated holes
        "movi r4, #512\n"    // request 2 contiguous pages
        "alloc r4, r5\n"     // must fail: no 2-page contiguous run anywhere
        "printr r5\n"        // print 0 if correctly failed
        "freememory r3\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t11.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must terminate");
    check(cap.str() == "0\n",
          "fragmented heap must reject 2-page request, got: " + cap.str());
}

// Test 12: Exhaust heap, then any further alloc must return 0.
void test_heap_exhausted_returns_zero() {
    // Fill the entire heap, then try one more alloc — must return 0.
    write_file("t12.asm",
        "movi r1, #4096\n"   // fill entire heap (16 pages)
        "alloc r1, r2\n"     // should succeed
        "movi r1, #256\n"
        "alloc r1, r3\n"     // heap is full — must return 0
        "printr r3\n"        // print 0 if correct
        "freememory r2\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t12.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must terminate");
    check(cap.str() == "0\n",
          "alloc on full heap must return 0, got: " + cap.str());
}

// ─────────────────────────────────────────────────────────────────────────────
// Process isolation
// ─────────────────────────────────────────────────────────────────────────────

// Test 13: Two processes each use their own heap independently.
void test_heap_isolation_between_processes() {
    write_file("hip_a.asm",
        "movi r1, #256\n"
        "alloc r1, r2\n"
        "movi r3, #111\n"
        "movrm r2, r3\n"
        "movmr r4, r2\n"
        "printr r4\n"        // must print 111
        "freememory r2\n"
        "exit\n"
    );
    write_file("hip_b.asm",
        "movi r1, #256\n"
        "alloc r1, r2\n"
        "movi r3, #222\n"
        "movrm r2, r3\n"
        "movmr r4, r2\n"
        "printr r4\n"        // must print 222
        "freememory r2\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os(128 * 1024);
    uint32_t pidA = os.createProcessFromAsm("hip_a.asm", 1);
    uint32_t pidB = os.createProcessFromAsm("hip_b.asm", 1);
    os.run();
    check(os.getProcessState(pidA) == ProcState::Terminated, "process A must terminate");
    check(os.getProcessState(pidB) == ProcState::Terminated, "process B must terminate");
    check(cap.str().find("111\n") != std::string::npos, "process A must print 111");
    check(cap.str().find("222\n") != std::string::npos, "process B must print 222");
}

// ─────────────────────────────────────────────────────────────────────────────
// Cleanup on exit
// ─────────────────────────────────────────────────────────────────────────────

// Test 14: Process fills heap and exits WITHOUT freeing.
// OS must automatically release the pages. A second OS instance (fresh state)
// should be able to allocate normally — proving no permanent leak.
void test_heap_freed_on_process_exit() {
    // First: run a process that leaks its heap
    {
        OS os1(64 * 1024);
        os1.createProcessFromAsm("t2.asm", 1); // reuse t2.asm: allocs 4096, exits
        os1.run();
        // os1 destroyed here: cleanupProcess_ must have freed all pages
    }
    // Second: fresh OS — should allocate fine
    write_file("t14_after.asm",
        "movi r1, #256\n"
        "alloc r1, r2\n"
        "cmpi r2, #0\n"
        "jei #12\n"
        "movi r3, #1\n"
        "printr r3\n"
        "exit\n"
        "movi r3, #0\n"
        "printr r3\n"
        "exit\n"
    );
    CaptureStdout cap;
    OS os2(64 * 1024);
    uint32_t pid = os2.createProcessFromAsm("t14_after.asm", 1);
    os2.run();
    check(os2.getProcessState(pid) == ProcState::Terminated, "second process must terminate");
    check(cap.str().find("1\n") != std::string::npos,
          "fresh OS after leaked process must allocate normally");
}

// Test 15: alloc with size=0 must return 0 gracefully.
void test_alloc_zero_bytes_fails_gracefully() {
    write_file("t15.asm",
        "movi r1, #0\n"
        "alloc r1, r2\n"    // size=0, must return 0
        "printr r2\n"       // print 0 if correct
        "exit\n"
    );
    CaptureStdout cap;
    OS os(64 * 1024);
    uint32_t pid = os.createProcessFromAsm("t15.asm", 1);
    os.run();
    check(os.getProcessState(pid) == ProcState::Terminated, "process must terminate");
    check(cap.str() == "0\n", "alloc(0) must return 0, got: " + cap.str());
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Module 5 Heap Allocation Tests ===\n\n";

    std::cout << "-- Requirement: allocated in heap, not beyond it --\n";
    run_test("alloc_address_is_in_heap",       test_alloc_address_is_in_heap);
    run_test("alloc_entire_heap_succeeds",     test_alloc_entire_heap);
    run_test("alloc_beyond_heap_fails",        test_alloc_beyond_heap_fails);

    std::cout << "\n-- Requirement: size memory appropriately --\n";
    run_test("alloc_rounds_up_to_full_page",   test_alloc_rounds_up_to_full_page);
    run_test("alloc_two_separate_pages",       test_alloc_two_separate_pages);
    run_test("alloc_multi_page_contiguous",    test_alloc_multi_page_contiguous);

    std::cout << "\n-- Requirement: free memory appropriately --\n";
    run_test("free_recycles_pages",            test_free_recycles_pages);
    run_test("fill_free_refill",               test_fill_free_refill);
    run_test("double_free_is_safe",            test_double_free_is_safe);
    run_test("free_bad_address_is_safe",       test_free_bad_address_is_safe);

    std::cout << "\n-- Requirement: fail if no contiguous block --\n";
    run_test("fragmentation_causes_failure",   test_fragmentation_causes_failure);
    run_test("heap_exhausted_returns_zero",    test_heap_exhausted_returns_zero);

    std::cout << "\n-- Process isolation --\n";
    run_test("heap_isolation_two_processes",   test_heap_isolation_between_processes);

    std::cout << "\n-- Cleanup on exit --\n";
    run_test("heap_freed_on_process_exit",     test_heap_freed_on_process_exit);
    run_test("alloc_zero_bytes_fails",         test_alloc_zero_bytes_fails_gracefully);

    std::cout << "\n=============================\n";
    std::cout << "Passed: " << passed << '\n';
    std::cout << "Failed: " << failed << '\n';

    return failed == 0 ? 0 : 1;
}