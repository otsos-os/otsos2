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

#ifndef ELF_H
#define ELF_H

#include <mlibc/mlibc.h>

#define ELF_MAGIC 0x464C457F
#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFDATA2LSB 1 // litle endian
#define ELFDATA2MSB 2 //big endian
#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4
#define EM_386 3 // i386
#define EM_X86_64 62 //amd64
#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_TLS 7
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

/* ELF64 Header */
typedef struct {
  u8 e_ident[16];  /* Magic number and other info */
  u16 e_type;      /* Object file type */
  u16 e_machine;   /* Architecture */
  u32 e_version;   /* Object file version */
  u64 e_entry;     /* Entry point virtual address */
  u64 e_phoff;     /* Program header table file offset */
  u64 e_shoff;     /* Section header table file offset */
  u32 e_flags;     /* Processor-specific flags */
  u16 e_ehsize;    /* ELF header size in bytes */
  u16 e_phentsize; /* Program header table entry size */
  u16 e_phnum;     /* Program header table entry count */
  u16 e_shentsize; /* Section header table entry size */
  u16 e_shnum;     /* Section header table entry count */
  u16 e_shstrndx;  /* Section header string table index */
} __attribute__((packed)) elf64_ehdr_t;

/* ELF64 Program Header */
typedef struct {
  u32 p_type;   /* Segment type */
  u32 p_flags;  /* Segment flags */
  u64 p_offset; /* Segment file offset */
  u64 p_vaddr;  /* Segment virtual address */
  u64 p_paddr;  /* Segment physical address */
  u64 p_filesz; /* Segment size in file */
  u64 p_memsz;  /* Segment size in memory */
  u64 p_align;  /* Segment alignment */
} __attribute__((packed)) elf64_phdr_t;

/* ELF64 Section Header */
typedef struct {
  u32 sh_name;      /* Section name (string table index) */
  u32 sh_type;      /* Section type */
  u64 sh_flags;     /* Section flags */
  u64 sh_addr;      /* Section virtual address at exec */
  u64 sh_offset;    /* Section file offset */
  u64 sh_size;      /* Section size in bytes */
  u32 sh_link;      /* Link to another section */
  u32 sh_info;      /* Additional section info */
  u64 sh_addralign; /* Section alignment */
  u64 sh_entsize;   /* Entry size if section holds table */
} __attribute__((packed)) elf64_shdr_t;

/* ELF validation result */
typedef enum {
  ELF_OK = 0,
  ELF_ERR_INVALID_MAGIC,
  ELF_ERR_INVALID_CLASS,
  ELF_ERR_INVALID_ENDIAN,
  ELF_ERR_INVALID_TYPE,
  ELF_ERR_INVALID_MACHINE,
  ELF_ERR_NO_SEGMENTS
} elf_result_t;

/* Parsed ELF info */
typedef struct {
  elf64_ehdr_t *header;
  elf64_phdr_t *phdrs;
  u64 entry_point;
  u64 load_addr_min;
  u64 load_addr_max;
} elf_info_t;
typedef struct {
  u64 entry;    // entry point
  u64 load_base;
  u64 phdr_vaddr;  //addres to program headers
  u64 phent;       //program header entry size
  u64 phnum;       //number of program headers
  u64 e_type;      //ET_EXEC or ET_DYN
  u64 interp_off;  //file offset PT_INTERP string 0 if none
  u64 interp_len;  //length of interp string incl NULL 0 if none
  u64 load_addr_max; // end of highest PT_LOAD segment
  u64 data_start;    // page-aligned start of writable PT_LOAD segment
  u64 data_end;      // page-aligned end of writable PT_LOAD segment
} elf_loadinfo_t;

#define ELF_INTERP_BASE 0x40000000ULL

/* Validate ELF file */
elf_result_t elf_validate(void *data, u64 size);

/* Parse ELF file and fill info structure */
elf_result_t elf_parse(void *data, u64 size, elf_info_t *info);

/* Load ELF segments into memory (returns entry point or 0 on error) */
u64 elf_load(void *data, u64 size);
u64 elf_load_full(void *data, u64 size, elf_loadinfo_t *out);
u64 elf_load_interp(void *data, u64 size, u64 load_base);

/* Get error message for result code */
const char *elf_strerror(elf_result_t result);

#endif
