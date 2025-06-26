/* testmain.c - Minimal test to validate _MoveVBR and _AtomicAnd */

#include <proto/exec.h>

extern void _MoveVBR(void);
extern void _AtomicAnd(void);

ULONG test_val = 0xFFFFFFFF;

int main(void) {
    struct Library *SysBase = *((struct Library **)4);
    
    // Test VBR relocation (should be harmless unless already relocated)
    _MoveVBR();
    
    // Print value before
    Printf("Before AtomicAnd: %08lx\n", test_val);

    // Setup stack for _AtomicAnd(A0 = &test_val, D0 = mask)
    ULONG mask = 0x0000FFFF;
    __asm__ __volatile__ (
        "move.l %[ptr], %%a0\n\t"
        "move.l %[msk], %%d0\n\t"
        "jsr _AtomicAnd\n"
        :
        : [ptr] "r" (&test_val), [msk] "d" (mask)
        : "a0", "d0"
    );

    // Print value after
    Printf("After AtomicAnd: %08lx\n", test_val);

    return 0;
}