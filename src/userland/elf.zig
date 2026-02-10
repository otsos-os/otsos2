// /*
//  * Copyright (c) 2026, otsos team
//  *
//  * Redistribution and use in source and binary forms, with or without
//  * modification, are permitted provided that the following conditions are met:
//  *
//  * 1. Redistributions of source code must retain the above copyright notice,
//  * this list of conditions and the following disclaimer.
//  *
//  * 2. Redistributions in binary form must reproduce the above copyright notice,
//  *    this list of conditions and the following disclaimer in the documentation
//  *    and/or other materials provided with the distribution.
//  *
//  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  * POSSIBILITY OF SUCH DAMAGE.
//  */

const panic_mod = @import("panic");
pub const panic = panic_mod.panic;

const PAGE_SIZE: u64 = 4096;

const PTE_PRESENT: u64 = 0x1;
const PTE_RW: u64 = 0x2;
const PTE_USER: u64 = 0x4;
const PTE_NX: u64 = (@as(u64, 1) << 63);

const ELF_MAGIC: u32 = 0x464c457f;

const ELFCLASS64: u8 = 2;
const ELFDATA2LSB: u8 = 1;

const ET_EXEC: u16 = 2;
const EM_X86_64: u16 = 62;

const PT_LOAD: u32 = 1;

const PF_X: u32 = 0x1;

pub const elf64_ehdr_t = extern struct {
    e_ident: [16]u8,
    e_type: u16,
    e_machine: u16,
    e_version: u32,
    e_entry: u64,
    e_phoff: u64,
    e_shoff: u64,
    e_flags: u32,
    e_ehsize: u16,
    e_phentsize: u16,
    e_phnum: u16,
    e_shentsize: u16,
    e_shnum: u16,
    e_shstrndx: u16,
};

pub const elf64_phdr_t = extern struct {
    p_type: u32,
    p_flags: u32,
    p_offset: u64,
    p_vaddr: u64,
    p_paddr: u64,
    p_filesz: u64,
    p_memsz: u64,
    p_align: u64,
};

pub const elf64_shdr_t = extern struct {
    sh_name: u32,
    sh_type: u32,
    sh_flags: u64,
    sh_addr: u64,
    sh_offset: u64,
    sh_size: u64,
    sh_link: u32,
    sh_info: u32,
    sh_addralign: u64,
    sh_entsize: u64,
};

pub const elf_result_t = enum(c_int) {
    ELF_OK = 0,
    ELF_ERR_INVALID_MAGIC,
    ELF_ERR_INVALID_CLASS,
    ELF_ERR_INVALID_ENDIAN,
    ELF_ERR_INVALID_TYPE,
    ELF_ERR_INVALID_MACHINE,
    ELF_ERR_NO_SEGMENTS,
};

pub const elf_info_t = extern struct {
    header: *align(1) elf64_ehdr_t,
    phdrs: [*]align(1) elf64_phdr_t,
    entry_point: u64,
    load_addr_min: u64,
    load_addr_max: u64,
};

extern fn com1_printf(fmt: [*:0]const u8, ...) void;
extern fn kmalloc_aligned(size: usize, alignment: usize) ?*anyopaque;
extern fn memset(s: *anyopaque, c: c_int, n: usize) *anyopaque;
extern fn mmu_map_page(vaddr: u64, paddr: u64, flags: u64) void;
extern fn mmu_virt_to_phys(vaddr: u64) u64;
extern fn mmu_get_pte_flags(vaddr: u64) u64;

inline fn u64_to_usize(value: u64) usize {
    return @intCast(value);
}

inline fn u64_to_ptr(value: u64) *anyopaque {
    return @ptrFromInt(u64_to_usize(value));
}

inline fn data_as_bytes(data: *anyopaque) [*]u8 {
    return @ptrCast(data);
}

pub export fn elf_strerror(result: elf_result_t) [*:0]const u8 {
    return switch (result) {
        .ELF_OK => "OK",
        .ELF_ERR_INVALID_MAGIC => "Invalid ELF magic",
        .ELF_ERR_INVALID_CLASS => "Invalid ELF class (not 64-bit)",
        .ELF_ERR_INVALID_ENDIAN => "Invalid endianness (not little-endian)",
        .ELF_ERR_INVALID_TYPE => "Invalid ELF type (not executable)",
        .ELF_ERR_INVALID_MACHINE => "Invalid machine (not x86_64)",
        .ELF_ERR_NO_SEGMENTS => "No loadable segments",
    };
}

pub export fn elf_validate(data: *anyopaque, size: u64) elf_result_t {
    if (size < @sizeOf(elf64_ehdr_t)) {
        return .ELF_ERR_INVALID_MAGIC;
    }

    const ehdr: *align(1) const elf64_ehdr_t = @ptrCast(data);

    const magic: u32 = @as(u32, ehdr.e_ident[0]) |
        (@as(u32, ehdr.e_ident[1]) << 8) |
        (@as(u32, ehdr.e_ident[2]) << 16) |
        (@as(u32, ehdr.e_ident[3]) << 24);

    if (magic != ELF_MAGIC) {
        return .ELF_ERR_INVALID_MAGIC;
    }

    if (ehdr.e_ident[4] != ELFCLASS64) {
        return .ELF_ERR_INVALID_CLASS;
    }

    if (ehdr.e_ident[5] != ELFDATA2LSB) {
        return .ELF_ERR_INVALID_ENDIAN;
    }

    if (ehdr.e_type != ET_EXEC) {
        return .ELF_ERR_INVALID_TYPE;
    }

    if (ehdr.e_machine != EM_X86_64) {
        return .ELF_ERR_INVALID_MACHINE;
    }

    if (ehdr.e_phnum == 0) {
        return .ELF_ERR_NO_SEGMENTS;
    }

    return .ELF_OK;
}

pub export fn elf_parse(data: *anyopaque, size: u64, info: *elf_info_t) elf_result_t {
    const result = elf_validate(data, size);
    if (result != .ELF_OK) {
        return result;
    }

    const ehdr: *align(1) elf64_ehdr_t = @ptrCast(data);
    const base = data_as_bytes(data);
    const phdrs: [*]align(1) elf64_phdr_t = @ptrCast(base + u64_to_usize(ehdr.e_phoff));

    info.header = ehdr;
    info.phdrs = phdrs;
    info.entry_point = ehdr.e_entry;
    info.load_addr_min = ~@as(u64, 0);
    info.load_addr_max = 0;

    var i: u16 = 0;
    while (i < ehdr.e_phnum) : (i += 1) {
        const phdr = &phdrs[i];

        if (phdr.p_type != PT_LOAD) {
            continue;
        }

        if (phdr.p_vaddr < info.load_addr_min) {
            info.load_addr_min = phdr.p_vaddr;
        }

        const end_addr = phdr.p_vaddr + phdr.p_memsz;
        if (end_addr > info.load_addr_max) {
            info.load_addr_max = end_addr;
        }
    }

    return .ELF_OK;
}

pub export fn elf_load(data: *anyopaque, size: u64) u64 {
    var info: elf_info_t = undefined;
    const result = elf_parse(data, size, &info);

    if (result != .ELF_OK) {
        com1_printf("[ELF] Error: %s\n", elf_strerror(result));
        return 0;
    }

    com1_printf(
        "[ELF] Loading ELF: entry=%p, segments=%d\n",
        u64_to_ptr(info.entry_point),
        @as(c_int, @intCast(info.header.e_phnum)),
    );
    com1_printf(
        "[ELF] Load range: %p - %p\n",
        u64_to_ptr(info.load_addr_min),
        u64_to_ptr(info.load_addr_max),
    );

    var i: u16 = 0;
    while (i < info.header.e_phnum) : (i += 1) {
        const phdr = &info.phdrs[i];

        if (phdr.p_type != PT_LOAD) {
            continue;
        }

        const vaddr = phdr.p_vaddr;
        const filesz = phdr.p_filesz;
        const memsz = phdr.p_memsz;
        const offset = phdr.p_offset;

        com1_printf(
            "[ELF] Loading segment: vaddr=%p filesz=%d memsz=%d flags=%x\n",
            u64_to_ptr(vaddr),
            @as(c_int, @intCast(filesz)),
            @as(c_int, @intCast(memsz)),
            phdr.p_flags,
        );

        var page_flags: u64 = PTE_PRESENT | PTE_USER | PTE_RW;
        if ((phdr.p_flags & PF_X) == 0) {
            page_flags |= PTE_NX;
        }

        const page_mask: u64 = PAGE_SIZE - 1;
        const page_start = vaddr & ~page_mask;
        const page_end = (vaddr + memsz + PAGE_SIZE - 1) & ~page_mask;

        var page = page_start;
        while (page < page_end) : (page += PAGE_SIZE) {
            const existing_phys = mmu_virt_to_phys(page);
            if (existing_phys != 0) {
                const existing_flags = mmu_get_pte_flags(page);
                if ((existing_flags & PTE_USER) != 0) {
                    var combined_flags = existing_flags | page_flags | PTE_USER | PTE_PRESENT;
                    const exec_ok = ((existing_flags & PTE_NX) == 0) or ((phdr.p_flags & PF_X) != 0);
                    if (exec_ok) {
                        combined_flags &= ~PTE_NX;
                    }
                    mmu_map_page(page, existing_phys, combined_flags);
                    continue;
                }
            }

            const phys_page = kmalloc_aligned(u64_to_usize(PAGE_SIZE), u64_to_usize(PAGE_SIZE));
            if (phys_page == null) {
                com1_printf("[ELF] Error: Failed to allocate page at %p\n", u64_to_ptr(page));
                return 0;
            }
            _ = memset(phys_page.?, 0, u64_to_usize(PAGE_SIZE));

            mmu_map_page(page, @as(u64, @intFromPtr(phys_page.?)), page_flags);
        }

        const base = data_as_bytes(data);
        const src = base + u64_to_usize(offset);
        const dst: [*]u8 = @ptrFromInt(u64_to_usize(vaddr));

        var j: u64 = 0;
        while (j < filesz) : (j += 1) {
            dst[u64_to_usize(j)] = src[u64_to_usize(j)];
        }

        j = filesz;
        while (j < memsz) : (j += 1) {
            dst[u64_to_usize(j)] = 0;
        }

        page = page_start;
        while (page < page_end) : (page += PAGE_SIZE) {
            const phys = mmu_virt_to_phys(page);
            if (phys == 0) {
                continue;
            }
            const existing_flags = mmu_get_pte_flags(page);

            var combined_flags = existing_flags | (page_flags & (PTE_PRESENT | PTE_USER | PTE_RW));
            const exec_ok = ((existing_flags & PTE_NX) == 0) or ((phdr.p_flags & PF_X) != 0);
            if (exec_ok) {
                combined_flags &= ~PTE_NX;
            } else {
                combined_flags |= PTE_NX;
            }
            mmu_map_page(page, phys, combined_flags);
        }
    }

    com1_printf("[ELF] Load complete, entry point: %p\n", u64_to_ptr(info.entry_point));
    return info.entry_point;
}
