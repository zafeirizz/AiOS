ASM    = nasm
CC     = i686-linux-gnu-gcc
LD     = i686-linux-gnu-ld
QEMU   = qemu-system-x86_64

CFLAGS  = -ffreestanding -O2 -Wall -Wextra -m32 -Iinclude -MMD -MP
LDFLAGS = -T linker.ld -nostdlib

VPATH = kernel:kernel/core:kernel/gui:kernel/apps:kernel/fs:kernel/net:kernel/shell:kernel/editor:libc

OBJS = \
    build/entry.o        \
    build/isr.o          \
    build/kernel.o       \
    build/gdt.o          \
    build/idt.o          \
    build/irq.o          \
    build/isr_handler.o  \
    build/syscall.o      \
    build/timer.o        \
    build/paging.o       \
    build/process.o      \
    build/scheduler.o    \
    build/heap.o         \
    build/fb.o           \
    build/vga.o          \
    build/keyboard.o     \
    build/mouse.o        \
    build/pci.o          \
    build/rtl8139.o      \
    build/e1000.o        \
    build/ethernet.o     \
    build/net_util.o     \
    build/arp.o          \
    build/ip.o           \
    build/icmp.o         \
    build/udp.o          \
    build/tcp.o          \
    build/dns.o          \
    build/http.o         \
    build/html.o         \
    build/font8x16.o     \
    build/gfx.o          \
    build/event.o        \
    build/wm.o           \
    build/ui.o           \
    build/terminal_app.o \
    build/fileman.o      \
    build/fileviewer.o   \
    build/settings.o     \
    build/diskmgr.o      \
    build/notepad.o      \
    build/browser.o      \
    build/calculator.o   \
    build/usertest.o     \
    build/user_test.o    \
    build/shell.o        \
    build/editor.o       \
    build/ata.o          \
    build/fat32.o        \
    build/myfs.o         \
    build/disk.o         \
    build/zfsx.o         \
    build/vfs.o          \
    build/string.o

-include $(OBJS:.o=.d)

.PHONY: all run run-debug run-e1000 disk clean

all: myos.iso

myos.iso: $(OBJS) | build
	$(LD) $(LDFLAGS) $(OBJS) -o build/kernel.bin
	mkdir -p iso/boot/grub
	cp build/kernel.bin   iso/boot/kernel.bin
	cp boot/grub/grub.cfg iso/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso iso

# RTL8139 (default)
run: myos.iso
	$(QEMU) -cdrom myos.iso -vga std -nic model=rtl8139 \
	    $(if $(wildcard disk.img),-drive file=disk.img,format=raw,if=ide) \
	    -m 64M -no-reboot

# e1000 (Intel NIC)
run-e1000: myos.iso
	$(QEMU) -cdrom myos.iso -vga std -nic model=e1000 \
	    $(if $(wildcard disk.img),-drive file=disk.img,format=raw,if=ide) \
	    -m 64M -no-reboot

run-debug: myos.iso
	$(QEMU) -cdrom myos.iso -vga std -nic model=rtl8139 \
	    $(if $(wildcard disk.img),-drive file=disk.img,format=raw,if=ide) \
	    -m 64M -no-reboot -d int 2>log.txt

disk:
	dd if=/dev/zero of=disk.img bs=512 count=65536

build:
	mkdir -p build

build/entry.o: kernel/entry.asm | build
	$(ASM) -f elf32 $< -o $@

build/isr.o: kernel/core/isr.asm | build
	$(ASM) -f elf32 $< -o $@

build/kernel.o:      kernel/kernel.c          | build
	$(CC) $(CFLAGS) -c $< -o $@

build/gdt.o:         kernel/core/gdt.c        | build
	$(CC) $(CFLAGS) -c $< -o $@
build/idt.o:         kernel/core/idt.c        | build
	$(CC) $(CFLAGS) -c $< -o $@
build/irq.o:         kernel/core/irq.c        | build
	$(CC) $(CFLAGS) -c $< -o $@
build/isr_handler.o: kernel/core/isr_handler.c| build
	$(CC) $(CFLAGS) -c $< -o $@
build/syscall.o:     kernel/core/syscall.c    | build
	$(CC) $(CFLAGS) -c $< -o $@
build/process.o:     kernel/core/process.c    | build
	$(CC) $(CFLAGS) -c $< -o $@
build/scheduler.o:   kernel/core/scheduler.c  | build
	$(CC) $(CFLAGS) -c $< -o $@
build/timer.o:       kernel/core/timer.c      | build
	$(CC) $(CFLAGS) -c $< -o $@
build/paging.o:      kernel/core/paging.c     | build
	$(CC) $(CFLAGS) -c $< -o $@
build/heap.o:        kernel/core/heap.c       | build
	$(CC) $(CFLAGS) -c $< -o $@
build/fb.o:          kernel/core/fb_simple.c  | build
	$(CC) $(CFLAGS) -c $< -o $@

build/vga.o:         kernel/drivers/vga.c     | build
	$(CC) $(CFLAGS) -c $< -o $@
build/keyboard.o:    kernel/drivers/keyboard.c| build
	$(CC) $(CFLAGS) -c $< -o $@
build/mouse.o:       kernel/drivers/mouse.c   | build
	$(CC) $(CFLAGS) -c $< -o $@
build/pci.o:         kernel/drivers/pci.c     | build
	$(CC) $(CFLAGS) -c $< -o $@
build/rtl8139.o:     kernel/drivers/rtl8139.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/e1000.o:       kernel/drivers/e1000.c   | build
	$(CC) $(CFLAGS) -c $< -o $@

build/ethernet.o:    kernel/net/ethernet.c    | build
	$(CC) $(CFLAGS) -c $< -o $@
build/net_util.o:    kernel/net/net_util.c    | build
	$(CC) $(CFLAGS) -c $< -o $@
build/arp.o:         kernel/net/arp.c         | build
	$(CC) $(CFLAGS) -c $< -o $@
build/ip.o:          kernel/net/ip.c          | build
	$(CC) $(CFLAGS) -c $< -o $@
build/icmp.o:        kernel/net/icmp.c        | build
	$(CC) $(CFLAGS) -c $< -o $@
build/udp.o:         kernel/net/udp.c         | build
	$(CC) $(CFLAGS) -c $< -o $@
build/tcp.o:         kernel/net/tcp.c         | build
	$(CC) $(CFLAGS) -c $< -o $@
build/dns.o:         kernel/net/dns.c         | build
	$(CC) $(CFLAGS) -c $< -o $@
build/http.o:        kernel/net/http.c        | build
	$(CC) $(CFLAGS) -c $< -o $@
build/html.o:        kernel/net/html.c        | build
	$(CC) $(CFLAGS) -c $< -o $@

build/font8x16.o:    kernel/gui/font8x16.c    | build
	$(CC) $(CFLAGS) -c $< -o $@
build/gfx.o:         kernel/gui/gfx.c         | build
	$(CC) $(CFLAGS) -c $< -o $@
build/event.o:       kernel/gui/event.c       | build
	$(CC) $(CFLAGS) -c $< -o $@
build/wm.o:          kernel/gui/wm.c          | build
	$(CC) $(CFLAGS) -c $< -o $@
build/ui.o:          kernel/gui/ui.c          | build
	$(CC) $(CFLAGS) -c $< -o $@
build/terminal_app.o:kernel/gui/terminal_app.c| build
	$(CC) $(CFLAGS) -c $< -o $@

build/fileman.o:     kernel/apps/fileman.c    | build
	$(CC) $(CFLAGS) -c $< -o $@
build/fileviewer.o:  kernel/apps/fileviewer.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/settings.o:    kernel/apps/settings.c   | build
	$(CC) $(CFLAGS) -c $< -o $@
build/diskmgr.o:     kernel/apps/diskmgr.c    | build
	$(CC) $(CFLAGS) -c $< -o $@
build/notepad.o:     kernel/apps/notepad.c    | build
	$(CC) $(CFLAGS) -c $< -o $@
build/browser.o:     kernel/apps/browser.c    | build
	$(CC) $(CFLAGS) -c $< -o $@
build/calculator.o:  kernel/apps/calculator.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/usertest.o:    kernel/apps/usertest.c   | build
	$(CC) $(CFLAGS) -c $< -o $@
build/user_test.o:   kernel/apps/user_test.c  | build
	$(CC) $(CFLAGS) -c $< -o $@

build/shell.o:       kernel/shell/shell.c     | build
	$(CC) $(CFLAGS) -c $< -o $@

build/editor.o:      kernel/editor/editor.c   | build
	$(CC) $(CFLAGS) -c $< -o $@

build/ata.o:         kernel/fs/ata.c          | build
	$(CC) $(CFLAGS) -c $< -o $@
build/fat32.o:       kernel/fs/fat32.c        | build
	$(CC) $(CFLAGS) -c $< -o $@
build/myfs.o:        kernel/fs/myfs.c         | build
	$(CC) $(CFLAGS) -c $< -o $@
build/disk.o:        kernel/fs/disk.c         | build
	$(CC) $(CFLAGS) -c $< -o $@
build/zfsx.o:        kernel/fs/zfsx.c         | build
	$(CC) $(CFLAGS) -c $< -o $@
build/vfs.o:         kernel/fs/vfs.c          | build
	$(CC) $(CFLAGS) -c $< -o $@

build/string.o:      libc/string.c            | build
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/ myos.iso iso/boot/kernel.bin
	@echo "Cleaned."
