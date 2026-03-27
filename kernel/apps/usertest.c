void usertest_main(void) {
    const char* msg = "Hello from user mode (Ring 3)!\n";

    // Syscall 0: write
    __asm__ volatile (
        "mov $0, %%eax\n"
        "mov %0, %%ebx\n"
        "int $0x80\n"
        : : "r"(msg) : "eax", "ebx"
    );

    // Syscall 4: exit/yield
    __asm__ volatile (
        "mov $4, %%eax\n"
        "int $0x80\n"
        : : : "eax"
    );

    while(1); // Should not be reached
}