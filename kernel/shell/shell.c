#include <stdint.h>
#include "../../include/shell.h"
#include "../../include/shell_gui.h"
#include "../../include/vga.h"
#include "../../include/keyboard.h"
#include "../../include/heap.h"
#include "../../include/timer.h"
#include "../../include/string.h"
#include "../../include/ata.h"
#include "../../include/myfs.h"
#include "../../include/fat32.h"
#include "../../include/zfsx.h"
#include "../../include/vfs.h"
#include "../../include/editor.h"
#include "../../include/arp.h"
#include "../../include/ip.h"
#include "../../include/icmp.h"
#include "../../include/udp.h"
#include "../../include/rtl8139.h"
#include "../../include/ethernet.h"
#include "../../include/net.h"
#include "../../include/dns.h"

#define BUFSIZE 256

static char buf[BUFSIZE];
static int  bi  = 0;
static char cwd[256] = "/root";

/* ── History ──────────────────────────────────────────── */
#define HIST_MAX 16
static char  hist[HIST_MAX][BUFSIZE];
static int   hist_count = 0;
static int   hist_idx   = -1;

static void hist_push(const char* cmd) {
    if (!cmd[0]) return;
    /* Shift down */
    if (hist_count < HIST_MAX) hist_count++;
    for (int i = hist_count-1; i > 0; i--)
        strcpy(hist[i], hist[i-1]);
    strncpy(hist[0], cmd, BUFSIZE-1);
    hist_idx = -1;
}

/* ── Output ───────────────────────────────────────────── */
static shell_putchar_fn out_fn = (void*)0;

static void sputchar(char c) {
    if (out_fn) out_fn(c);
    else        terminal_putchar(c);
}
static void swrite(const char* s)           { while (*s) sputchar(*s++); }
static void swriteline(const char* s)       { swrite(s); sputchar('\n'); }

void shell_init_gui(shell_putchar_fn fn) {
    out_fn = fn;
    swrite("AiOS shell -- type 'help'\n");
    swrite("AiOS> ");
}

static void swrite_dec(uint32_t v) {
    if (!v) { sputchar('0'); return; }
    char tmp[12]; int i=11; tmp[i]=0;
    while (v) { tmp[--i]='0'+(v%10); v/=10; }
    swrite(&tmp[i]);
}

static void swrite_size(uint32_t bytes) {
    if (bytes >= 1024*1024) {
        swrite_dec(bytes/1024/1024); swrite(" MB");
    } else if (bytes >= 1024) {
        swrite_dec(bytes/1024); swrite(" KB");
    } else {
        swrite_dec(bytes); swrite(" B");
    }
}

static void print_ip(uint32_t ip) {
    swrite_dec(ip&0xFF);       sputchar('.');
    swrite_dec((ip>>8)&0xFF);  sputchar('.');
    swrite_dec((ip>>16)&0xFF); sputchar('.');
    swrite_dec((ip>>24)&0xFF);
}

static void print_mac(const uint8_t* mac) {
    const char* hex = "0123456789ABCDEF";
    for (int i=0;i<6;i++){
        sputchar(hex[(mac[i]>>4)&0xF]);
        sputchar(hex[mac[i]&0xF]);
        if (i<5) sputchar(':');
    }
}

/* ── Prompt ───────────────────────────────────────────── */
static void prompt(void) {
    swrite(cwd);
    swrite("> ");
}

/* ── FS helpers ───────────────────────────────────────── */
static void list_cb_myfs(const char* name, uint32_t size) {
    swrite("  [MyFS ] ");
    swrite(name);
    swrite("  ("); swrite_size(size); swriteline(")");
}
static void list_cb_fat32(const char* name, uint32_t size) {
    (void)size;
    swrite("  [FAT32] ");
    swriteline(name);
}
static void list_cb_zfsx(const char* name, uint32_t size) {
    (void)size;
    swrite("  [ZFSX ] ");
    swriteline(name);
}

/* Try to read a file from any mounted FS */
static int read_any(const char* name, uint8_t* buf, int bufsz) {
    int n = myfs_read(name, buf, (uint32_t)bufsz-1);
    if (n >= 0) return n;
    if (fat32_is_mounted()) {
        n = fat32_read_file(name, buf, (uint32_t)bufsz-1);
        if (n >= 0) return n;
    }
    if (zfsx_is_mounted()) {
        n = zfsx_read_file(name, buf, (uint32_t)bufsz-1);
        if (n >= 0) return n;
    }
    return -1;
}

/* Try to delete from any mounted FS */
static int delete_any(const char* name) {
    if (myfs_delete(name) == 0) return 0;
    if (fat32_is_mounted() && fat32_delete(name) == 0) return 0;
    if (zfsx_is_mounted()) {
        int id = zfsx_find_in_dir(ZFSX_ROOT_ID, name);
        if (id >= 0 && zfsx_delete(id) == 0) return 0;
    }
    return -1;
}

/* ── Commands ─────────────────────────────────────────── */

static void cmd_help(void) {
    swriteline("AiOS Shell Commands");
    swriteline("─────────────────────────────────────────");
    swriteline("Files (works across MyFS / FAT32 / ZFSX):");
    swriteline("  ls             list files (all mounted FSes)");
    swriteline("  ls <fs>        ls myfs | fat32 | zfsx");
    swriteline("  cat <file>     print file contents");
    swriteline("  write <f> <t>  write text to file (MyFS)");
    swriteline("  touch <file>   create empty file (MyFS)");
    swriteline("  rm <file>      delete file (any FS)");
    swriteline("  cp <src> <dst> copy file (MyFS)");
    swriteline("  mv <src> <dst> move/rename file (MyFS)");
    swriteline("  mkdir <name>   create dir (ZFSX or MyFS)");
    swriteline("  stat <file>    file info");
    swriteline("  pwd / cd <dir> working directory");
    swriteline("  df             disk free (all filesystems)");
    swriteline("─────────────────────────────────────────");
    swriteline("Disk / FS:");
    swriteline("  diskinfo       ATA disk status");
    swriteline("  mkfs <lba> <n> format MyFS");
    swriteline("  mount <lba>    mount MyFS");
    swriteline("  zmkfs [lba]    format ZFSX");
    swriteline("  zmount <lba>   mount ZFSX");
    swriteline("  zls            list ZFSX root");
    swriteline("  ztouch <name>  create ZFSX file");
    swriteline("─────────────────────────────────────────");
    swriteline("Network:");
    swriteline("  netinfo        IP / MAC / gateway");
    swriteline("  arp            ARP cache");
    swriteline("  ping <ip>      ICMP ping");
    swriteline("  dns <host>     DNS lookup");
    swriteline("  netstat        simulated connections");
    swriteline("─────────────────────────────────────────");
    swriteline("System:");
    swriteline("  mem            heap usage");
    swriteline("  uptime         seconds since boot");
    swriteline("  ticks          raw timer ticks");
    swriteline("  ps             process list");
    swriteline("  uname          kernel info");
    swriteline("  clear          clear terminal");
    swriteline("─────────────────────────────────────────");
    swriteline("Apps:");
    swriteline("  run notepad    open notepad");
    swriteline("  run fileman    open file manager");
    swriteline("  run browser    open browser");
    swriteline("  run calc       open calculator");
    swriteline("  run settings   open settings");
    swriteline("  run diskmgr    open disk manager");
}

static void cmd_clear(void) {
    if (!out_fn) terminal_clear();
    /* In GUI terminal, print newlines to scroll */
    else { for (int i=0;i<25;i++) sputchar('\n'); }
}

static void cmd_ls(const char* arg) {
    int did = 0;
    if (!arg[0] || strcmp(arg,"myfs")==0) {
        swriteline("[MyFS]");
        myfs_list(list_cb_myfs);
        did = 1;
    }
    if (!arg[0] || strcmp(arg,"fat32")==0) {
        if (fat32_is_mounted()) {
            swriteline("[FAT32]");
            fat32_list(list_cb_fat32);
            did = 1;
        } else if (strcmp(arg,"fat32")==0) {
            swriteline("FAT32 not mounted.");
        }
    }
    if (!arg[0] || strcmp(arg,"zfsx")==0) {
        if (zfsx_is_mounted()) {
            swriteline("[ZFSX]");
            zfsx_list_root(list_cb_zfsx);
            did = 1;
        } else if (strcmp(arg,"zfsx")==0) {
            swriteline("ZFSX not mounted.");
        }
    }
    if (!did) swriteline("(no filesystems mounted)");
}

static void cmd_cat(const char* arg) {
    if (!arg[0]) { swriteline("Usage: cat <file>"); return; }
    static uint8_t tbuf[4096];
    int n = read_any(arg, tbuf, sizeof(tbuf));
    if (n < 0) { swriteline("File not found"); return; }
    tbuf[n] = '\0';
    swrite((char*)tbuf);
    if (n > 0 && tbuf[n-1] != '\n') sputchar('\n');
}

static void cmd_write(const char* arg) {
    char fname[64]; const char* p = arg; int i=0;
    while (*p && *p!=' ' && i<63) fname[i++]=*p++;
    fname[i]='\0';
    while (*p==' ') p++;
    if (!fname[0]||!p[0]) { swriteline("Usage: write <file> <text>"); return; }
    if (myfs_write(fname,(const uint8_t*)p,(uint32_t)strlen(p))>=0)
        swriteline("Written.");
    else swriteline("Write failed.");
}

static void cmd_touch(const char* arg) {
    if (!arg[0]) { swriteline("Usage: touch <file>"); return; }
    myfs_write(arg,(const uint8_t*)"",0);
    swrite("Created: "); swriteline(arg);
}

static void cmd_rm(const char* arg) {
    if (!arg[0]) { swriteline("Usage: rm <file>"); return; }
    if (delete_any(arg)==0) { swrite("Deleted: "); swriteline(arg); }
    else swriteline("File not found or delete failed.");
}

static void cmd_cp(const char* arg) {
    /* cp <src> <dst> */
    char src[64], dst[64];
    const char* p = arg; int i=0;
    while (*p && *p!=' ' && i<63) {
        src[i++] = *p++;
    }
    src[i] = '\0';
    while (*p==' ') p++;
    i=0;
    while (*p && i<63) {
        dst[i++] = *p++;
    }
    dst[i] = '\0';
    if (!src[0]||!dst[0]) { swriteline("Usage: cp <src> <dst>"); return; }
    static uint8_t cbuf[4096];
    int n = read_any(src, cbuf, sizeof(cbuf));
    if (n<0) { swriteline("Source not found."); return; }
    if (myfs_write(dst, cbuf, (uint32_t)n)>=0) {
        swrite("Copied "); swrite(src); swrite(" -> "); swriteline(dst);
    } else swriteline("Copy failed.");
}

static void cmd_mv(const char* arg) {
    /* mv <src> <dst>: cp then rm */
    char src[64], dst[64];
    const char* p = arg; int i=0;
    while (*p && *p!=' ' && i<63) {
        src[i++] = *p++;
    }
    src[i] = '\0';
    while (*p==' ') p++;
    i=0;
    while (*p && i<63) {
        dst[i++] = *p++;
    }
    dst[i] = '\0';
    if (!src[0]||!dst[0]) { swriteline("Usage: mv <src> <dst>"); return; }
    static uint8_t mbuf[4096];
    int n = read_any(src, mbuf, sizeof(mbuf));
    if (n<0) { swriteline("Source not found."); return; }
    if (myfs_write(dst, mbuf, (uint32_t)n)<0) { swriteline("Move failed."); return; }
    delete_any(src);
    swrite("Moved "); swrite(src); swrite(" -> "); swriteline(dst);
}

static void cmd_stat(const char* arg) {
    if (!arg[0]) { swriteline("Usage: stat <file>"); return; }
    static uint8_t sbuf[1];
    int n = myfs_read(arg, sbuf, 1);
    if (n >= 0) {
        swrite("File: "); swriteline(arg);
        swriteline("FS:   MyFS");
        return;
    }
    if (fat32_is_mounted()) {
        n = fat32_read_file(arg, sbuf, 1);
        if (n>=0) { swrite("File: "); swriteline(arg); swriteline("FS:   FAT32"); return; }
    }
    if (zfsx_is_mounted()) {
        n = zfsx_read_file(arg, sbuf, 1);
        if (n>=0) { swrite("File: "); swriteline(arg); swriteline("FS:   ZFSX"); return; }
    }
    swriteline("File not found.");
}

static void cmd_df(void) {
    swriteline("Filesystem   Size    Used   FS-Type");
    swrite("disk0        512MB   ");
    swrite_size(heap_used());
    swriteline("  MyFS");
    if (fat32_is_mounted()) swriteline("disk0p1      128MB   --      FAT32");
    if (zfsx_is_mounted())  swriteline("disk0p2       10MB   --      ZFSX");
}

static void cmd_pwd(void) { swrite(cwd); sputchar('\n'); }

static void cmd_cd(const char* arg) {
    if (!arg[0]) { strcpy(cwd, "/root"); return; }
    if (strcmp(arg,"..")==0) {
        char* p = cwd + strlen(cwd) - 1;
        while (p > cwd && *p != '/') p--;
        if (p > cwd) *p = '\0'; else strcpy(cwd, "/");
    } else if (arg[0]=='/') {
        strncpy(cwd, arg, 255);
    } else {
        if (strlen(cwd)+strlen(arg)+2 < 255) {
            strcat(cwd, "/");
            strcat(cwd, arg);
        }
    }
}

static void cmd_echo(const char* arg) { swrite(arg); sputchar('\n'); }

static void cmd_mkdir(const char* arg) {
    if (!arg[0]) { swriteline("Usage: mkdir <name>"); return; }
    if (zfsx_is_mounted()) {
        int id = zfsx_create(ZFSX_ROOT_ID, arg, ZFSX_TYPE_DIR);
        if (id>=0) { swrite("mkdir (ZFSX): "); swriteline(arg); return; }
    }
    /* MyFS doesn't have real dirs but create a placeholder file */
    myfs_write(arg, (const uint8_t*)"[dir]", 5);
    swrite("mkdir (MyFS placeholder): "); swriteline(arg);
}

static void cmd_ps(void) {
    swriteline("PID  NAME       STATE");
    swriteline("  1  kernel     running");
    swriteline("  2  wm         running");
    swriteline("  3  shell      running");
}

static void cmd_uname(void) {
    swriteline("AiOS 1.0 x86 i686 ffreestanding");
}

static void cmd_diskinfo(void) {
    if (ata_init()) swriteline("ATA: disk present");
    else            swriteline("ATA: not detected");
    swrite("MyFS : "); swriteline(myfs_is_mounted() ? "mounted" : "not mounted");
    swrite("FAT32: "); swriteline(fat32_is_mounted()? "mounted" : "not mounted");
    swrite("ZFSX : "); swriteline(zfsx_is_mounted() ? "mounted" : "not mounted");
}

/* ── Disk / FS ────────────────────────────────────────── */
static void cmd_mkfs(const char* arg) {
    uint32_t lba=0, blocks=0; const char* p=arg;
    while (*p>='0'&&*p<='9'){lba=lba*10+(*p-'0');p++;} while(*p==' ')p++;
    while (*p>='0'&&*p<='9'){blocks=blocks*10+(*p-'0');p++;}
    if (!lba||!blocks){swriteline("Usage: mkfs <lba> <blocks>");return;}
    if (myfs_format(lba,blocks)==0) swriteline("MyFS formatted.");
    else swriteline("mkfs failed.");
}

static void cmd_mount(const char* arg) {
    uint32_t lba=0; const char* p=arg;
    while(*p>='0'&&*p<='9'){lba=lba*10+(*p-'0');p++;}
    if (!lba){swriteline("Usage: mount <lba>");return;}
    if (myfs_mount(lba)==0) swriteline("MyFS mounted.");
    else swriteline("mount failed.");
}

static void cmd_zmkfs(const char* arg) {
    uint32_t lba=2048; const char* p=arg;
    if (*p) { lba=0; while(*p>='0'&&*p<='9'){lba=lba*10+(*p-'0');p++;} }
    if (!lba){swriteline("Invalid LBA");return;}
    swriteline("Formatting ZFSX...");
    if (zfsx_format(lba,20000)==0){swriteline("Mounting..."); zfsx_mount(lba);}
    else swriteline("ZFSX format failed.");
}

static void cmd_zmount(const char* arg) {
    uint32_t lba=0; const char* p=arg;
    while(*p>='0'&&*p<='9'){lba=lba*10+(*p-'0');p++;}
    if (!lba){swriteline("Usage: zmount <lba>");return;}
    if (zfsx_mount(lba)==0) swriteline("ZFSX mounted.");
    else swriteline("zmount failed.");
}

static void cmd_zls(void) { zfsx_ls(0); }

static void cmd_ztouch(const char* arg) {
    if (!arg[0]){swriteline("Usage: ztouch <name>");return;}
    int id = zfsx_create(0, arg, ZFSX_TYPE_FILE);
    if (id>=0){ const char* m="Hello ZFSX!"; zfsx_write(id,m,strlen(m)); swrite("Created: ");swriteline(arg);}
    else swriteline("ztouch failed.");
}

/* ── Network ──────────────────────────────────────────── */
static void cmd_netinfo(void) {
    swrite("IP      : "); print_ip(ip_our_ip);  sputchar('\n');
    swrite("Gateway : "); print_ip(ip_gateway); sputchar('\n');
    swrite("Netmask : "); print_ip(ip_netmask); sputchar('\n');
    const uint8_t* mac = rtl8139_mac();
    if (mac) { swrite("MAC     : "); print_mac(mac); sputchar('\n'); }
}

static void cmd_arp(void) { arp_print_table(); }

static void cmd_ping(const char* arg) {
    if (!arg[0]) { swriteline("Usage: ping <ip>"); return; }
    uint32_t dst=0; const char* p=arg;
    for (int i=0;i<4;i++){
        uint32_t oct=0;
        while(*p>='0'&&*p<='9'){oct=oct*10+(*p-'0');p++;}
        dst|=(oct<<(i*8));
        if(i<3&&*p=='.') p++;
    }
    if (!dst){ swriteline("Usage: ping <ip> (use IP address)"); return; }
    swrite("PING "); print_ip(dst); swriteline(" ...");
    for (int seq=1;seq<=4;seq++){
        uint32_t t0=timer_get_ticks();
        icmp_ping(dst,seq);
        int got=0;
        for (int w=0;w<200;w++){
            ethernet_poll();
            uint32_t from; uint16_t rs;
            if (icmp_got_reply(&from,&rs)){
                uint32_t ms=(timer_get_ticks()-t0)*10;
                swrite("  Reply from "); print_ip(from);
                swrite(" seq="); swrite_dec(rs);
                swrite(" time="); swrite_dec(ms); swriteline(" ms");
                got=1; break;
            }
            for(int d=0;d<500;d++) __asm__ volatile("pause");
        }
        if (!got){ swrite("  Timeout seq="); swrite_dec(seq); sputchar('\n'); }
    }
}

static void cmd_dns(const char* arg) {
    if (!arg[0]) { swriteline("Usage: dns <hostname>"); return; }
    swrite("Resolving: "); swriteline(arg);
    uint32_t ip = 0;
    if (dns_resolve(arg, &ip) == 0) {
        swrite("  -> "); print_ip(ip); sputchar('\n');
    } else {
        swriteline("  DNS resolution failed.");
    }
}

static void cmd_netstat(void) {
    swriteline("Proto  Local             Foreign           State");
    swrite("tcp    "); print_ip(ip_our_ip); swriteline(":80   0.0.0.0:*   LISTEN");
    swrite("udp    "); print_ip(ip_our_ip); swriteline(":53   0.0.0.0:*   OPEN");
}

/* ── run <app> ────────────────────────────────────────── */
static void cmd_run(const char* arg) {
    extern void notepad_launch(void);
    extern void fileman_launch(void);
    extern void browser_launch(void);
    extern void calculator_launch(void);
    extern void settings_launch(void);
    extern int  diskmgr_launch(void);

    char app[32]; const char* p=arg; int i=0;
    while(*p&&*p!=' '&&i<31) app[i++]=*p++;
    app[i]='\0';
    while(*p==' ') p++;

    if      (strcmp(app,"notepad") ==0) notepad_launch();
    else if (strcmp(app,"fileman") ==0 || strcmp(app,"fm")==0) fileman_launch();
    else if (strcmp(app,"browser") ==0) browser_launch();
    else if (strcmp(app,"calc")    ==0 || strcmp(app,"calculator")==0) calculator_launch();
    else if (strcmp(app,"settings")==0) settings_launch();
    else if (strcmp(app,"diskmgr") ==0) diskmgr_launch();
    else if (strcmp(app,"editor")  ==0) {
        if (!out_fn) {
            swriteline("Launching editor...");
            editor_open(*p ? p : (void*)0);
            swriteline("Exited editor.");
            extern void vga_enter_text_mode(void);
            vga_enter_text_mode();
        } else {
            swriteline("Use start menu to open editor in GUI mode.");
        }
    }
    else { swrite("Unknown app: "); swriteline(app); }
}

/* ── Dispatcher ───────────────────────────────────────── */
static void execute(char* line) {
    if (!line[0]) return;
    hist_push(line);

    char* arg = line;
    while (*arg && *arg != ' ') arg++;
    if (*arg) { *arg='\0'; arg++; while(*arg==' ') arg++; }

    if      (strcmp(line,"help")    ==0) cmd_help();
    else if (strcmp(line,"clear")   ==0) cmd_clear();
    else if (strcmp(line,"ls")      ==0) cmd_ls(arg);
    else if (strcmp(line,"cat")     ==0) cmd_cat(arg);
    else if (strcmp(line,"write")   ==0) cmd_write(arg);
    else if (strcmp(line,"touch")   ==0) cmd_touch(arg);
    else if (strcmp(line,"rm")      ==0) cmd_rm(arg);
    else if (strcmp(line,"cp")      ==0) cmd_cp(arg);
    else if (strcmp(line,"mv")      ==0) cmd_mv(arg);
    else if (strcmp(line,"mkdir")   ==0) cmd_mkdir(arg);
    else if (strcmp(line,"stat")    ==0) cmd_stat(arg);
    else if (strcmp(line,"df")      ==0) cmd_df();
    else if (strcmp(line,"pwd")     ==0) cmd_pwd();
    else if (strcmp(line,"cd")      ==0) cmd_cd(arg);
    else if (strcmp(line,"echo")    ==0) cmd_echo(arg);
    else if (strcmp(line,"ps")      ==0) cmd_ps();
    else if (strcmp(line,"uname")   ==0) cmd_uname();
    else if (strcmp(line,"uptime")  ==0) { swrite("Uptime: "); swrite_dec(timer_get_ticks()/100); swriteline(" s"); }
    else if (strcmp(line,"ticks")   ==0) { swrite("Ticks: "); swrite_dec(timer_get_ticks()); sputchar('\n'); }
    else if (strcmp(line,"mem")     ==0) { swrite("Heap used: "); swrite_size(heap_used()); sputchar('\n'); }
    else if (strcmp(line,"diskinfo")==0) cmd_diskinfo();
    else if (strcmp(line,"mkfs")    ==0) cmd_mkfs(arg);
    else if (strcmp(line,"mount")   ==0) cmd_mount(arg);
    else if (strcmp(line,"zmkfs")   ==0) cmd_zmkfs(arg);
    else if (strcmp(line,"zmount")  ==0) cmd_zmount(arg);
    else if (strcmp(line,"zls")     ==0) cmd_zls();
    else if (strcmp(line,"ztouch")  ==0) cmd_ztouch(arg);
    else if (strcmp(line,"zstat")   ==0) zfsx_stat();
    else if (strcmp(line,"netinfo") ==0) cmd_netinfo();
    else if (strcmp(line,"arp")     ==0) cmd_arp();
    else if (strcmp(line,"ping")    ==0) cmd_ping(arg);
    else if (strcmp(line,"dns")     ==0) cmd_dns(arg);
    else if (strcmp(line,"netstat") ==0) cmd_netstat();
    else if (strcmp(line,"run")     ==0) cmd_run(arg);
    else if (strcmp(line,"tty")     ==0) { swriteline("GUI terminal active."); }
    else { swrite("Unknown command: "); swrite(line); sputchar('\n'); }
}

/* ── Public API ───────────────────────────────────────── */
volatile int shell_mode_request = 0;

void shell_init(void) {
    swriteline("AiOS Shell -- type 'help'");
    prompt();
}

void shell_run_text_mode(void) {
    shell_mode_request = 0;
    out_fn = (void*)0;
    shell_init();
    while (1) {
        char c;
        while ((c = keyboard_poll()) != 0) {
            shell_handle_input(c);
            if (shell_mode_request == 2) { shell_mode_request=0; return; }
        }
        ethernet_poll();
        __asm__ volatile("hlt");
    }
}

void shell_handle_input(char c) {
    if (c==0||c==0x1B) return;
    if (c=='\n') {
        buf[bi]='\0'; sputchar('\n');
        if (bi>0) execute(buf);
        bi=0; hist_idx=-1;
        prompt(); return;
    }
    if (c=='\b') {
        if (bi>0){ bi--; sputchar('\b'); sputchar(' '); sputchar('\b'); }
        return;
    }
    /* Arrow up: history recall (scancode comes through as raw byte) */
    if ((uint8_t)c == 0x48 || (uint8_t)c == 0xE0) return; /* ignore raw scancodes */
    if (bi < BUFSIZE-1) { buf[bi++]=c; sputchar(c); }
}
