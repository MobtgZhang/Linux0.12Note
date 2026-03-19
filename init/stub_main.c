/*
 * x86_64 最小内核桩：经 UEFI loader 加载后在 0x100000 运行。
 * 通过 COM1 (0x3F8) 输出，便于 qemu -serial stdio 观察。
 *
 * 构建类型（由 Makefile 传入 -DBOOT_DEBUG=1 或 -DBOOT_RELEASE=1）：
 *   Release — 最少串口输出
 *   Debug   — 详细 [KDBG] 日志，说明为何没有 bash /usr/bin
 */
typedef unsigned char u8;
typedef unsigned short u16;

static inline u8 inb(u16 port)
{
	u8 v;
	__asm__ volatile ("inb %w1, %0" : "=a"(v) : "Nd"(port));
	return v;
}

static inline void outb(u16 port, u8 val)
{
	__asm__ volatile ("outb %0, %w1" : : "a"(val), "Nd"(port));
}

static void serial_init(void)
{
	outb(0x3f8 + 1, 0x00);
	outb(0x3f8 + 3, 0x80);
	outb(0x3f8 + 0, 0x03);
	outb(0x3f8 + 1, 0x00);
	outb(0x3f8 + 3, 0x03);
	outb(0x3f8 + 2, 0xc7);
	outb(0x3f8 + 4, 0x0b);
}

static void serial_putc(char c)
{
	while ((inb(0x3f8 + 5) & 0x20) == 0)
		;
	outb(0x3f8, (u8)c);
}

static void serial_puts(const char *s)
{
	while (*s)
		serial_putc(*s++);
}

/* 供 head64.s 调用的入口：UEFI 下用桩实现，完整内核用 init/main.c 的 main() */
void main(void)
{
	serial_init();

#ifdef BOOT_DEBUG
	serial_puts("\r\n[KDBG] Linux 0.12 UEFI x86_64 — DEBUG kernel stub\r\n");
	serial_puts("[KDBG] Serial COM1 0x3F8 OK (qemu: -serial stdio)\r\n");
	serial_puts("[KDBG] 说明：当前仅为内核桩，未挂载根文件系统、无 init、无用户态。\r\n");
	serial_puts("[KDBG] 因此不会出现 bash、/usr/bin 或 shell 提示符；属正常现象。\r\n");
	serial_puts("[KDBG] 完整用户态需：根镜像 + init + libc/shell 等，与 0.12 原设计一致后再谈。\r\n");
	serial_puts("[KDBG] 构建：make debug；Release：make release（少日志）。\r\n\r\n");
#elif defined(BOOT_RELEASE)
	serial_puts("\r\nLinux 0.12 UEFI x86_64 (release stub)\r\n");
#else
	serial_puts("\r\nLinux 0.12 UEFI x86_64 (stub)\r\n");
#endif
}
