/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <kernel/mmu.h>
#include <lib/com1.h>
#include <mlibc/memory.h>
#include <userland/elf.h>

const char *elf_strerror(elf_result_t result) {
  switch (result) {
  case ELF_OK:
    return "OK";
  case ELF_ERR_INVALID_MAGIC:
    return "Invalid ELF magic";
  case ELF_ERR_INVALID_CLASS:
    return "Invalid ELF class (not 64-bit)";
  case ELF_ERR_INVALID_ENDIAN:
    return "Invalid endianness (not little-endian)";
  case ELF_ERR_INVALID_TYPE:
    return "Invalid ELF type (not executable)";
  case ELF_ERR_INVALID_MACHINE:
    return "Invalid machine (not x86_64)";
  case ELF_ERR_NO_SEGMENTS:
    return "No loadable segments";
  default:
    return "Unknown error";
  }
}

elf_result_t elf_validate(void *data, u64 size) {
  if (size < sizeof(elf64_ehdr_t)) {
    return ELF_ERR_INVALID_MAGIC;
  }

  elf64_ehdr_t *ehdr = (elf64_ehdr_t *)data;

  /* Check magic number */
  if (*(u32 *)ehdr->e_ident != ELF_MAGIC) {
    return ELF_ERR_INVALID_MAGIC;
  }

  /* Check class (must be 64-bit) */
  if (ehdr->e_ident[4] != ELFCLASS64) {
    return ELF_ERR_INVALID_CLASS;
  }

  /* Check endianness (must be little-endian) */
  if (ehdr->e_ident[5] != ELFDATA2LSB) {
    return ELF_ERR_INVALID_ENDIAN;
  }

  /* Check type (must be executable) */
  if (ehdr->e_type != ET_EXEC) {
    return ELF_ERR_INVALID_TYPE;
  }

  /* Check machine (must be x86_64) */
  if (ehdr->e_machine != EM_X86_64) {
    return ELF_ERR_INVALID_MACHINE;
  }

  /* Check that we have program headers */
  if (ehdr->e_phnum == 0) {
    return ELF_ERR_NO_SEGMENTS;
  }

  return ELF_OK;
}

elf_result_t elf_parse(void *data, u64 size, elf_info_t *info) {
  elf_result_t result = elf_validate(data, size);
  if (result != ELF_OK) {
    return result;
  }

  elf64_ehdr_t *ehdr = (elf64_ehdr_t *)data;

  info->header = ehdr;
  info->phdrs = (elf64_phdr_t *)((u8 *)data + ehdr->e_phoff);
  info->entry_point = ehdr->e_entry;
  info->load_addr_min = (u64)-1;
  info->load_addr_max = 0;

  /* Find min/max load addresses */
  for (u16 i = 0; i < ehdr->e_phnum; i++) {
    elf64_phdr_t *phdr = &info->phdrs[i];

    if (phdr->p_type != PT_LOAD) {
      continue;
    }

    if (phdr->p_vaddr < info->load_addr_min) {
      info->load_addr_min = phdr->p_vaddr;
    }

    u64 end = phdr->p_vaddr + phdr->p_memsz;
    if (end > info->load_addr_max) {
      info->load_addr_max = end;
    }
  }

  return ELF_OK;
}

u64 elf_load(void *data, u64 size) {
  elf_info_t info;
  elf_result_t result = elf_parse(data, size, &info);

  if (result != ELF_OK) {
    com1_printf("[ELF] Error: %s\n", elf_strerror(result));
    return 0;
  }

  com1_printf("[ELF] Loading ELF: entry=%p, segments=%d\n",
              (void *)info.entry_point, info.header->e_phnum);
  com1_printf("[ELF] Load range: %p - %p\n", (void *)info.load_addr_min,
              (void *)info.load_addr_max);

  /* Load each PT_LOAD segment */
  for (u16 i = 0; i < info.header->e_phnum; i++) {
    elf64_phdr_t *phdr = &info.phdrs[i];

    if (phdr->p_type != PT_LOAD) {
      continue;
    }

    u64 vaddr = phdr->p_vaddr;
    u64 filesz = phdr->p_filesz;
    u64 memsz = phdr->p_memsz;
    u64 offset = phdr->p_offset;

    com1_printf("[ELF] Loading segment: vaddr=%p filesz=%d memsz=%d flags=%x\n",
                (void *)vaddr, (int)filesz, (int)memsz, phdr->p_flags);

    /* Determine page flags based on segment permissions */
    u64 page_flags = PTE_PRESENT | PTE_USER;
    if (phdr->p_flags & PF_W) {
      page_flags |= PTE_RW;
    }
    if (!(phdr->p_flags & PF_X)) {
      page_flags |= PTE_NX;
    }

    /* Map pages for this segment */
    u64 page_start = vaddr & ~(PAGE_SIZE - 1);
    u64 page_end = (vaddr + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (u64 page = page_start; page < page_end; page += PAGE_SIZE) {
      u64 existing_phys = mmu_virt_to_phys(page);
      if (existing_phys != 0) {
        u64 existing_flags = mmu_get_pte_flags(page);
        u64 combined_flags =
            (existing_flags | page_flags | PTE_USER | PTE_PRESENT);
        int exec = !(existing_flags & PTE_NX) || (phdr->p_flags & PF_X);
        if (exec) {
          combined_flags &= ~PTE_NX;
        }
        mmu_map_page(page, existing_phys, combined_flags);
        continue;
      }

      /* Allocate physical page */
      void *phys_page = kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
      if (!phys_page) {
        com1_printf("[ELF] Error: Failed to allocate page at %p\n",
                    (void *)page);
        return 0;
      }
      memset(phys_page, 0, PAGE_SIZE);

      /* Map virtual to physical */
      mmu_map_page(page, (u64)phys_page, page_flags);
    }

    /* Copy segment data */
    u8 *src = (u8 *)data + offset;
    u8 *dst = (u8 *)vaddr;

    /* Copy file contents */
    for (u64 j = 0; j < filesz; j++) {
      dst[j] = src[j];
    }

    /* Zero remaining bytes (BSS) */
    for (u64 j = filesz; j < memsz; j++) {
      dst[j] = 0;
    }
  }

  com1_printf("[ELF] Load complete, entry point: %p\n",
              (void *)info.entry_point);

  return info.entry_point;
}
