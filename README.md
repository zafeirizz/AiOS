# myOS – A 32-bit Hobby Operating System

**myOS** is a from-scratch 32-bit operating system built for learning and experimentation. It includes a custom kernel, graphical user interface (GUI), networking stack, multiple filesystems, and basic applications.

![GUI Screenshot](https://via.placeholder.com/800x400?text=GUI+Screenshot+Coming)

---

## 🚀 Features

### Boot
- Multiboot2 compliant
- Boots via GRUB

### Memory Management
- Paging (identity-mapped first 4 MB)
- Simple bump allocator (temporary)

### Interrupts
- IDT with IRQ handling
- PIC remapping
- PIT timer at 100 Hz

### Input
- PS/2 Keyboard (scancodes, modifiers, circular queue)
- PS/2 Mouse support

### Storage
- ATA PIO driver
- Filesystems:
  - MyFS (custom, contiguous)
  - FAT32 (read-only)
  - ZFSX (experimental)

### Network Stack
- RTL8139 PCI Ethernet driver
- ARP, IP, ICMP (ping)
- UDP, basic TCP
- DNS resolver
- HTTP client (basic)

### GUI
- 32-bit framebuffer graphics
- Window manager (overlapping windows, events)
- UI toolkit (buttons, lists, text input)
- Applications:
  - Terminal emulator
  - File manager
  - Text editor

### Shell
- Interactive CLI
- Filesystem, networking, and system commands

### User Mode (Planned)
- Syscall interface
- Userspace programs

---

## 📁 Project Structure
```
.
├── boot/           # GRUB configuration
├── include/        # Public headers
├── kernel/
│   ├── apps/       # GUI applications
│   ├── core/       # GDT, IDT, paging, scheduler
│   ├── drivers/    # Hardware drivers
│   ├── fs/         # Filesystems & VFS
│   ├── gui/        # Graphics & window manager
│   ├── libc/       # Kernel utilities
│   ├── net/        # Network stack
│   └── shell/      # CLI implementation
├── tools/          # Build tools/scripts
├── linker.ld       # Linker script
├── Makefile        # Build system
└── README.md
```
---

## ⚙️ Build Instructions

### Prerequisites
- i686-elf-gcc (recommended) or multilib GCC
- nasm
- grub-mkrescue
- xorriso
- qemu-system-x86

### Install (Debian/Ubuntu)
sudo apt install build-essential nasm grub-pc-bin xorriso qemu-system-x86

### Build
make clean
make

Output:
myos.iso

---

## ▶️ Running

### QEMU (Recommended)
qemu-system-x86_64 -cdrom myos.iso -vga vmware -device rtl8139,netdev=net0 -netdev user,id=net0 -m 256 -device VGA,vgamem_mb=64

---

### VirtualBox

Create VM:
- Type: Other
- Version: Other/Unknown (32-bit)

Settings:
- Chipset: ICH9 (optional)
- Graphics: VBoxVGA
- Video Memory: 128MB

Attach myos.iso and boot

If GUI fails:
insmod vbe
insmod video
set gfxpayload=1024x768x32

---

## 💻 Shell Commands

help — List commands  
clear — Clear screen  
ticks — Show timer ticks  
uptime — System uptime  
mem — Heap usage  
diskinfo — ATA disk status  
mkfs <lba> <n> — Create MyFS  
mount <lba> — Mount filesystem  
ls — List files  
cat <file> — Read file  
write <f> <txt> — Write file  
rm <file> — Delete file  
netinfo — Network info  
arp — ARP cache  
ping <ip> — Ping host  
dns <host> — Resolve domain  
http get <url> — Fetch webpage  

---

## 🖥️ GUI Applications

Terminal — Run shell commands  
File Manager — Browse files  
Text Editor — Basic editing  

Launch GUI:
startgui

---

## 🌐 Network Configuration

Default IP: 10.0.2.15

Change:
netcfg <ip> <gateway> <netmask>

Example:
netcfg 192.168.1.100 192.168.1.1 255.255.255.0

Test:
ping 8.8.8.8  
dns google.com  
http get example.com  

---

## ⚠️ Known Limitations

- Single ATA master drive only
- MyFS has no fragmentation support
- Minimal TCP (SYN/FIN only)
- GUI performance not optimized
- Network stack incomplete

---

## 🤝 Contributing

Open issues or submit pull requests. Follow code style and ensure builds pass.

---

## 📄 License

MIT License

---

## 🙏 Acknowledgements

OSDev community  
GRUB project  
JamesM, Bran, and others  

---

## 🧠 Final Note

myOS is a work in progress — enjoy exploring!
