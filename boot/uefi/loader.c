/*
 * UEFI x86_64 引导加载器：从 FAT ESP 读取 kernel.elf (ET_EXEC)，
 * 映射 PT_LOAD 段，ExitBootServices 后跳入内核入口。
 *
 * 依赖：gnu-efi（Debian/Ubuntu: apt install gnu-efi）
 */
#include <efi.h>
#include <efilib.h>

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS64 2
#define EI_CLASS 4

#define ET_EXEC 2
#define EM_X86_64 62

#define PT_LOAD 1

typedef struct {
	UINT8 e_ident[16];
	UINT16 e_type;
	UINT16 e_machine;
	UINT32 e_version;
	UINT64 e_entry;
	UINT64 e_phoff;
	UINT64 e_shoff;
	UINT32 e_flags;
	UINT16 e_ehsize;
	UINT16 e_phentsize;
	UINT16 e_phnum;
	UINT16 e_shentsize;
	UINT16 e_shnum;
	UINT16 e_shstrndx;
} Elf64_Ehdr;

typedef struct {
	UINT32 p_type;
	UINT32 p_flags;
	UINT64 p_offset;
	UINT64 p_vaddr;
	UINT64 p_paddr;
	UINT64 p_filesz;
	UINT64 p_memsz;
	UINT64 p_align;
} Elf64_Phdr;

extern void uefi_handoff_kernel(UINTN StackPointer, void (*KernelEntry)(void));

#ifdef BOOT_DEBUG
#define LOG_D(...) Print(__VA_ARGS__)
#else
#define LOG_D(...) ((void)0)
#endif

/* 错误与 Release 下最少提示始终输出到固件控制台 */
#define LOG_E(...) Print(__VA_ARGS__)

static EFI_STATUS open_kernel_file(EFI_FILE *root, EFI_FILE **out)
{
	/* 协议成员函数按 UEFI 的 MS x64 ABI，须通过 uefi_call_wrapper */
	return uefi_call_wrapper(root->Open, 5, root, out,
				 L"EFI\\BOOT\\kernel.elf",
				 (UINT64)EFI_FILE_MODE_READ, (UINT64)0);
}

static EFI_STATUS read_entire_file(EFI_FILE *f, VOID **buf_out, UINTN *size_out)
{
	EFI_STATUS st;
	UINTN info_sz = sizeof(EFI_FILE_INFO) + 256 * sizeof(CHAR16);
	EFI_FILE_INFO *info;
	UINTN read_sz;
	VOID *buf;

	st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, info_sz,
			       (VOID **)&info);
	if (EFI_ERROR(st))
		return st;
	st = uefi_call_wrapper(f->GetInfo, 4, f, &gEfiFileInfoGuid,
			       &info_sz, info);
	if (EFI_ERROR(st)) {
		FreePool(info);
		return st;
	}
	read_sz = (UINTN)info->FileSize;
	FreePool(info);

	st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, read_sz,
			       &buf);
	if (EFI_ERROR(st))
		return st;

	UINTN left = read_sz;
	UINT8 *p = buf;
	while (left > 0) {
		UINTN chunk = left;
		st = uefi_call_wrapper(f->Read, 3, f, &chunk, p);
		if (EFI_ERROR(st)) {
			FreePool(buf);
			return st;
		}
		if (chunk == 0)
			break;
		p += chunk;
		left -= chunk;
	}
	*buf_out = buf;
	*size_out = read_sz;
	return EFI_SUCCESS;
}

static EFI_STATUS load_elf64_segments(VOID *kbuf, UINTN ksize)
{
	Elf64_Ehdr *e = kbuf;

	if (ksize < sizeof(Elf64_Ehdr))
		return EFI_LOAD_ERROR;

	if (e->e_ident[0] != ELFMAG0 || e->e_ident[1] != ELFMAG1 ||
	    e->e_ident[2] != ELFMAG2 || e->e_ident[3] != ELFMAG3)
		return EFI_LOAD_ERROR;
	if (e->e_ident[EI_CLASS] != ELFCLASS64)
		return EFI_LOAD_ERROR;
	if (e->e_machine != EM_X86_64 || e->e_type != ET_EXEC)
		return EFI_LOAD_ERROR;
	if (e->e_phentsize != sizeof(Elf64_Phdr))
		return EFI_LOAD_ERROR;
	if (ksize < e->e_phoff + (UINTN)e->e_phnum * sizeof(Elf64_Phdr))
		return EFI_LOAD_ERROR;

	Elf64_Phdr *ph = (Elf64_Phdr *)((UINT8 *)kbuf + e->e_phoff);

	for (UINTN i = 0; i < e->e_phnum; i++, ph++) {
		if (ph->p_type != PT_LOAD)
			continue;

		UINT64 dest = ph->p_paddr ? ph->p_paddr : ph->p_vaddr;
		UINT64 memsz = ph->p_memsz;
		UINT64 filesz = ph->p_filesz;

		if (memsz == 0)
			continue;
		if (ph->p_offset + filesz > ksize)
			return EFI_LOAD_ERROR;

		UINTN pages = (UINTN)((memsz + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE);
		EFI_PHYSICAL_ADDRESS phys = dest;

		EFI_STATUS st = uefi_call_wrapper(BS->AllocatePages, 4,
						   AllocateAddress, EfiLoaderData,
						   pages, &phys);
		if (EFI_ERROR(st))
			return st;

		VOID *dst = (VOID *)(UINTN)dest;
		CopyMem(dst, (UINT8 *)kbuf + ph->p_offset, (UINTN)filesz);
		if (memsz > filesz)
			ZeroMem((UINT8 *)dst + filesz, (UINTN)(memsz - filesz));

#ifdef BOOT_DEBUG
		LOG_D(L"[UEFI][D] PT_LOAD paddr=%lx vaddr=%lx filesz=%lx memsz=%lx\r\n",
		      dest, ph->p_vaddr, filesz, memsz);
#endif
	}
	return EFI_SUCCESS;
}

static EFI_STATUS exit_boot_services_retry(EFI_HANDLE img)
{
	UINTN map_sz = 0, map_key, desc_sz;
	UINT32 ver;
	EFI_MEMORY_DESCRIPTOR *map = NULL;
	EFI_STATUS st;
	UINTN i;

	for (i = 0; i < 5; i++) {
		map_sz = 0;
		st = uefi_call_wrapper(BS->GetMemoryMap, 5, &map_sz, NULL,
				       &map_key, &desc_sz, &ver);
		if (st != EFI_BUFFER_TOO_SMALL && EFI_ERROR(st))
			return st;
		map_sz += 2 * EFI_PAGE_SIZE;
		if (map)
			FreePool(map);
		st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
				       map_sz, (VOID **)&map);
		if (EFI_ERROR(st))
			return st;
		st = uefi_call_wrapper(BS->GetMemoryMap, 5, &map_sz, map,
				       &map_key, &desc_sz, &ver);
		if (EFI_ERROR(st))
			continue;
		st = uefi_call_wrapper(BS->ExitBootServices, 2, img, map_key);
		if (!EFI_ERROR(st)) {
			/* 成功后不得再调用 BootServices（含 FreePool） */
			return EFI_SUCCESS;
		}
	}
	if (map)
		FreePool(map);
	return EFI_LOAD_ERROR;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS st;
	InitializeLib(ImageHandle, SystemTable);

	LOG_D(L"[UEFI][D] Linux 0.12 UEFI x86_64 loader (DEBUG build)\r\n");
#ifndef BOOT_DEBUG
	Print(L"UEFI: boot\r\n");
#endif

	VOID *iface_li = NULL;
	st = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle,
			       &LoadedImageProtocol, &iface_li);
	if (EFI_ERROR(st)) {
		LOG_E(L"LoadedImageProtocol: %r\r\n", st);
		return st;
	}
	EFI_LOADED_IMAGE_PROTOCOL *loaded = iface_li;
	EFI_FILE *root = LibOpenRoot(loaded->DeviceHandle);
	if (!root) {
		LOG_E(L"LibOpenRoot failed\r\n");
		return EFI_LOAD_ERROR;
	}

	LOG_D(L"[UEFI][D] opened ESP root\r\n");

	EFI_FILE *kfile;
	st = open_kernel_file(root, &kfile);
	uefi_call_wrapper(root->Close, 1, root);
	if (EFI_ERROR(st)) {
		LOG_E(L"open kernel.elf: %r (path EFI/BOOT/kernel.elf)\r\n", st);
		return st;
	}

	VOID *kbuf;
	UINTN ksz;
	st = read_entire_file(kfile, &kbuf, &ksz);
	uefi_call_wrapper(kfile->Close, 1, kfile);
	if (EFI_ERROR(st)) {
		LOG_E(L"read kernel: %r\r\n", st);
		return st;
	}

	LOG_D(L"[UEFI][D] kernel.elf size=0%lx bytes\r\n", (unsigned long)ksz);

	st = load_elf64_segments(kbuf, ksz);
	if (EFI_ERROR(st)) {
		LOG_E(L"ELF load: %r\r\n", st);
		FreePool(kbuf);
		return st;
	}

	Elf64_Ehdr *eh = (Elf64_Ehdr *)kbuf;
	UINTN entry = (UINTN)eh->e_entry;
	LOG_D(L"[UEFI][D] ELF entry=%lx phnum=%u\r\n", entry, (unsigned)eh->e_phnum);
	FreePool(kbuf);

	EFI_PHYSICAL_ADDRESS stack_base;
	st = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
			       EfiLoaderData, 8, &stack_base);
	if (EFI_ERROR(st)) {
		LOG_E(L"stack pages: %r\r\n", st);
		return st;
	}
	UINTN stack_top = (UINTN)stack_base + 8 * EFI_PAGE_SIZE - 16;
	stack_top &= ~(UINTN)0xf;

	LOG_D(L"[UEFI][D] stack_top=%lx, calling ExitBootServices\r\n",
	      (unsigned long)stack_top);
#ifndef BOOT_DEBUG
	Print(L"UEFI: kernel\r\n");
#endif

	st = exit_boot_services_retry(ImageHandle);
	if (EFI_ERROR(st)) {
		LOG_E(L"ExitBootServices failed\r\n");
		return st;
	}

	uefi_handoff_kernel(stack_top, (void (*)(void))entry);
	/* not reached */
	for (;;)
		__asm__ volatile ("hlt");
	return EFI_LOAD_ERROR;
}
