# AiOS – A 32-bit Hobby Operating System

**AiOS** is a from-scratch 32-bit operating system built for learning and experimentation. It includes a custom kernel, graphical user interface (GUI), networking stack, multiple filesystems, and basic applications.

[![Virtual-Box-awkdfiasf-27-03-2026-17-51-05.png](https://i.postimg.cc/HsYgnWSJ/Virtual-Box-awkdfiasf-27-03-2026-17-51-05.png)](https://postimg.cc/grT5Bpbd)

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

The AiOS shell provides a rich set of commands for filesystem, networking, and system interaction.

### 📁 Filesystem Commands
| Command | Description |
|--------|------------|
| ls | List files (all mounted filesystems) |
| ls myfs / fat32 / zfsx | List specific filesystem |
| cat <file> | Display file contents |
| write <file> <text> | Write text to file (MyFS) |
| touch <file> | Create empty file |
| rm <file> | Delete file |
| cp <src> <dst> | Copy file |
| mv <src> <dst> | Move/rename file |
| mkdir <name> | Create directory |
| stat <file> | File info |
| pwd | Print working directory |
| cd <dir> | Change directory |
| df | Disk usage |

---

### 💾 Disk / Filesystem Management
| Command | Description |
|--------|------------|
| diskinfo | Show disk + FS status |
| mkfs <lba> <n> | Format MyFS |
| mount <lba> | Mount MyFS |
| zmkfs [lba] | Format ZFSX |
| zmount <lba> | Mount ZFSX |
| zls | List ZFSX |
| ztouch <file> | Create file in ZFSX |

---

### 🌐 Networking
| Command | Description |
|--------|------------|
| netinfo | Show IP, MAC, gateway |
| arp | Show ARP table |
| ping <ip> | Send ICMP ping |
| dns <host> | Resolve hostname |
| netstat | Show connections |

---

### ⚙️ System Commands
| Command | Description |
|--------|------------|
| mem | Heap usage |
| uptime | System uptime |
| ticks | Timer ticks |
| ps | Process list |
| uname | System info |
| clear | Clear screen |
| echo <text> | Print text |

---

### 🖥️ Applications
| Command | Description |
|--------|------------|
| run notepad | Launch text editor |
| run fileman | Launch file manager |
| run browser | Launch browser |
| run calc | Launch calculator |
| run settings | Launch settings |
| run diskmgr | Launch disk manager |

---

## 🖥️ GUI Applications

Terminal — Run shell commands  
File Manager — Browse files  
Text Editor — Basic editing  
Calculator - Basic calculator
Notepad - Currently save not working
Browser - WIP

---
```
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
```

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

AiOS is a work in progress — enjoy exploring!
