#!/usr/bin/env python3
import argparse
import os
import struct
import sys
from dataclasses import dataclass


ELF_HDR = "<16sHHIQQQIHHHHHH"
ELF_SHDR = "<IIQQQQIIQQ"
ELF_SYM = "<IBBHQQ"
ELF_RELA = "<QQq"

ET_REL = 1
EM_X86_64 = 62

SHT_SYMTAB = 2
SHT_RELA = 4
SHT_NOBITS = 8
SHT_REL = 9

SHF_WRITE = 0x1
SHF_ALLOC = 0x2
SHF_EXECINSTR = 0x4

SHN_UNDEF = 0

STB_LOCAL = 0
STB_GLOBAL = 1
STB_WEAK = 2
STT_SECTION = 3

R_X86_64_NONE = 0
R_X86_64_64 = 1
R_X86_64_PC32 = 2
R_X86_64_PLT32 = 4
R_X86_64_32 = 10
R_X86_64_32S = 11

KOFO_MAGIC = 0x4F464F4B
KOFO_VERSION = 1
KOFO_ARCH_X86_64 = 1
KOFO_ABI_KERNEL = 1
KOFO_STR_NONE = 0xFFFFFFFF

KOFO_SEC_TEXT = 1
KOFO_SEC_RODATA = 2
KOFO_SEC_DATA = 3
KOFO_SEC_BSS = 4

KOFO_SEC_F_ALLOC = 0x00000001
KOFO_SEC_F_READ = 0x00000002
KOFO_SEC_F_WRITE = 0x00000004
KOFO_SEC_F_EXEC = 0x00000008
KOFO_SEC_F_BSS = 0x00000010

KOFO_SYM_UNDEF = 0xFFFF
KOFO_SYM_F_GLOBAL = 0x00000001
KOFO_SYM_F_EXPORT = 0x00000002
KOFO_SYM_F_IMPORT = 0x00000004

KOFO_RELOC_ABS64 = 1
KOFO_RELOC_ABS32 = 2
KOFO_RELOC_REL32 = 3

KOFO_DRIVER_F_PSEUDO = 0x00000001

KOFO_HEADER = "<IHHHH" + ("I" * 27)
KOFO_SECTION = "<IIIIQII"
KOFO_SYMBOL = "<IHHQQ"
KOFO_IMPORT = "<IIII"
KOFO_RELOC = "<HHIQq"
KOFO_DRIVER = "<" + ("I" * 14)


class KofoPackError(Exception):
    pass


@dataclass
class ElfSection:
    index: int
    name: str
    sh_type: int
    flags: int
    off: int
    size: int
    link: int
    info: int
    align: int
    entsize: int


@dataclass
class ElfSymbol:
    index: int
    name: str
    bind: int
    sym_type: int
    section: int
    value: int
    size: int


@dataclass
class KofoSection:
    name: str
    sec_type: int
    flags: int
    align: int
    size: int
    data: bytes
    data_off: int = 0


@dataclass
class KofoSymbol:
    name: str
    section: int
    flags: int
    value: int
    size: int


@dataclass
class KofoReloc:
    section: int
    reloc_type: int
    symbol: int
    offset: int
    addend: int


@dataclass
class KofoDriver:
    name: str
    bus: str
    class_name: str
    driver_pass: int
    order: int
    flags: int
    probe: str | None
    attach: str | None
    detach: str | None


class StringTable:
    def __init__(self) -> None:
        self.data = bytearray(b"\0")
        self.offsets: dict[str, int] = {"": 0}

    def add(self, value: str | None) -> int:
        if value is None:
            return KOFO_STR_NONE
        if value in self.offsets:
            return self.offsets[value]
        off = len(self.data)
        self.data += value.encode("utf-8") + b"\0"
        self.offsets[value] = off
        return off


def align(value: int, boundary: int) -> int:
    if boundary <= 1:
        return value
    return (value + boundary - 1) & ~(boundary - 1)


def cstr(data: bytes, off: int) -> str:
    if off >= len(data):
        raise KofoPackError("string offset outside string table")
    end = data.find(b"\0", off)
    if end < 0:
        raise KofoPackError("unterminated string table entry")
    return data[off:end].decode("utf-8", "replace")


def read_range(data: bytes, off: int, size: int) -> bytes:
    end = off + size
    if off < 0 or size < 0 or end < off or end > len(data):
        raise KofoPackError("ELF section range outside file")
    return data[off:end]


def section_kind(section: ElfSection) -> tuple[int, int]:
    flags = KOFO_SEC_F_ALLOC | KOFO_SEC_F_READ
    if section.flags & SHF_EXECINSTR:
        return KOFO_SEC_TEXT, flags | KOFO_SEC_F_EXEC
    if section.sh_type == SHT_NOBITS:
        return KOFO_SEC_BSS, flags | KOFO_SEC_F_WRITE | KOFO_SEC_F_BSS
    if section.flags & SHF_WRITE:
        return KOFO_SEC_DATA, flags | KOFO_SEC_F_WRITE
    return KOFO_SEC_RODATA, flags


def parse_elf(path: str) -> tuple[bytes, list[ElfSection], list[ElfSymbol], int]:
    data = open(path, "rb").read()
    if len(data) < struct.calcsize(ELF_HDR):
        raise KofoPackError("input is too small for an ELF64 header")
    header = struct.unpack_from(ELF_HDR, data, 0)
    ident = header[0]
    if ident[:4] != b"\x7fELF" or ident[4] != 2 or ident[5] != 1:
        raise KofoPackError("input must be little-endian ELF64")
    if header[1] != ET_REL or header[2] != EM_X86_64:
        raise KofoPackError("input must be x86_64 ELF relocatable")
    e_shoff = header[6]
    e_shentsize = header[11]
    e_shnum = header[12]
    e_shstrndx = header[13]
    if e_shentsize != struct.calcsize(ELF_SHDR):
        raise KofoPackError("unsupported ELF section header size")
    if e_shnum == 0 or e_shstrndx >= e_shnum:
        raise KofoPackError("ELF section table is missing")
    if e_shoff + e_shnum * e_shentsize > len(data):
        raise KofoPackError("ELF section table outside file")

    raw_sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        raw_sections.append(struct.unpack_from(ELF_SHDR, data, off))

    shstr_raw = raw_sections[e_shstrndx]
    shstr = read_range(data, shstr_raw[4], shstr_raw[5])
    sections = []
    for i, raw in enumerate(raw_sections):
        name = cstr(shstr, raw[0]) if raw[0] != 0 else ""
        sections.append(ElfSection(
            index=i,
            name=name,
            sh_type=raw[1],
            flags=raw[2],
            off=raw[4],
            size=raw[5],
            link=raw[6],
            info=raw[7],
            align=raw[8] or 1,
            entsize=raw[9],
        ))

    symtab_index = -1
    for section in sections:
        if section.sh_type == SHT_SYMTAB:
            symtab_index = section.index
            break
    if symtab_index < 0:
        raise KofoPackError("ELF symtab is required")

    symtab = sections[symtab_index]
    if symtab.link >= len(sections):
        raise KofoPackError("ELF symtab has invalid string table link")
    strtab_sec = sections[symtab.link]
    strtab = read_range(data, strtab_sec.off, strtab_sec.size)
    sym_size = struct.calcsize(ELF_SYM)
    if symtab.entsize not in (0, sym_size):
        raise KofoPackError("unsupported ELF symbol entry size")
    if symtab.size % sym_size != 0:
        raise KofoPackError("ELF symtab size is not aligned")

    symbols = []
    sym_raw = read_range(data, symtab.off, symtab.size)
    for i in range(0, symtab.size, sym_size):
        st_name, st_info, _other, st_shndx, st_value, st_size = \
            struct.unpack_from(ELF_SYM, sym_raw, i)
        name = cstr(strtab, st_name) if st_name != 0 else ""
        if (st_info & 0xf) == STT_SECTION and st_shndx < len(sections):
            name = sections[st_shndx].name
        symbols.append(ElfSymbol(
            index=i // sym_size,
            name=name,
            bind=st_info >> 4,
            sym_type=st_info & 0xf,
            section=st_shndx,
            value=st_value,
            size=st_size,
        ))

    return data, sections, symbols, symtab_index


def build_sections(
    data: bytes,
    sections: list[ElfSection],
) -> tuple[list[KofoSection], dict[int, int]]:
    kofo_sections: list[KofoSection] = []
    section_map: dict[int, int] = {}
    for section in sections:
        if section.index == 0 or (section.flags & SHF_ALLOC) == 0:
            continue
        if section.align > 4096 or section.align & (section.align - 1):
            raise KofoPackError(f"{section.name}: unsupported alignment")
        sec_type, flags = section_kind(section)
        raw = b""
        if section.sh_type != SHT_NOBITS and section.size != 0:
            raw = read_range(data, section.off, section.size)
        section_map[section.index] = len(kofo_sections)
        kofo_sections.append(KofoSection(
            name=section.name,
            sec_type=sec_type,
            flags=flags,
            align=section.align,
            size=section.size,
            data=raw,
        ))
    if not kofo_sections:
        raise KofoPackError("ELF has no allocatable sections")
    return kofo_sections, section_map


def collect_raw_relocs(
    data: bytes,
    sections: list[ElfSection],
    section_map: dict[int, int],
) -> list[tuple[int, int, int, int, int]]:
    relocs = []
    rela_size = struct.calcsize(ELF_RELA)
    for section in sections:
        if section.sh_type not in (SHT_RELA, SHT_REL):
            continue
        if section.info not in section_map:
            continue
        if section.sh_type == SHT_REL and section.size != 0:
            raise KofoPackError(f"{section.name}: REL records are unsupported")
        if section.sh_type == SHT_REL:
            continue
        if section.entsize not in (0, rela_size):
            raise KofoPackError(f"{section.name}: bad RELA entry size")
        if section.size % rela_size != 0:
            raise KofoPackError(f"{section.name}: bad RELA section size")
        raw = read_range(data, section.off, section.size)
        for off in range(0, section.size, rela_size):
            r_offset, r_info, r_addend = struct.unpack_from(
                ELF_RELA, raw, off)
            sym = r_info >> 32
            r_type = r_info & 0xffffffff
            relocs.append((section.info, r_offset, r_type, sym, r_addend))
    return relocs


def symbol_name(symbol: ElfSymbol, sections: list[ElfSection]) -> str:
    if symbol.name:
        return symbol.name
    if symbol.section < len(sections):
        return f"{sections[symbol.section].name}+0x{symbol.value:x}"
    return f"sym{symbol.index}"


def build_symbols(
    sections: list[ElfSection],
    symbols: list[ElfSymbol],
    section_map: dict[int, int],
    raw_relocs: list[tuple[int, int, int, int, int]],
) -> tuple[list[KofoSymbol], dict[int, int], list[str]]:
    referenced = {reloc[3] for reloc in raw_relocs}
    kofo_symbols: list[KofoSymbol] = []
    symbol_map: dict[int, int] = {}
    imports: list[str] = []
    seen_imports: set[str] = set()

    for symbol in symbols:
        if symbol.index == 0:
            continue
        include = False
        section = KOFO_SYM_UNDEF
        flags = 0
        value = 0
        size = 0
        name = symbol_name(symbol, sections)

        if symbol.section == SHN_UNDEF:
            if symbol.name == "" and symbol.index in referenced:
                raise KofoPackError("relocation uses unnamed undefined symbol")
            if symbol.name:
                include = True
                flags |= KOFO_SYM_F_IMPORT
                if symbol.bind in (STB_GLOBAL, STB_WEAK):
                    flags |= KOFO_SYM_F_GLOBAL
                if name not in seen_imports:
                    seen_imports.add(name)
                    imports.append(name)
        elif symbol.section in section_map:
            include = True
            section = section_map[symbol.section]
            value = symbol.value
            size = 0 if symbol.sym_type == STT_SECTION else symbol.size
            kofo_sec_size = sections[symbol.section].size
            if value > kofo_sec_size or value + size > kofo_sec_size:
                raise KofoPackError(f"{name}: symbol outside section")
            if symbol.bind in (STB_GLOBAL, STB_WEAK):
                flags |= KOFO_SYM_F_GLOBAL | KOFO_SYM_F_EXPORT
        elif symbol.index in referenced:
            raise KofoPackError(
                f"{name}: relocation target is outside alloc sections")

        if include:
            symbol_map[symbol.index] = len(kofo_symbols)
            kofo_symbols.append(KofoSymbol(
                name=name,
                section=section,
                flags=flags,
                value=value,
                size=size,
            ))

    if not kofo_symbols:
        raise KofoPackError("no symbols survived conversion")
    for _target, _off, r_type, sym, _addend in raw_relocs:
        if r_type != R_X86_64_NONE and sym not in symbol_map:
            raise KofoPackError(f"relocation references dropped symbol {sym}")
    return kofo_symbols, symbol_map, imports


def reloc_type(r_type: int) -> int | None:
    if r_type == R_X86_64_NONE:
        return None
    if r_type == R_X86_64_64:
        return KOFO_RELOC_ABS64
    if r_type in (R_X86_64_PC32, R_X86_64_PLT32):
        return KOFO_RELOC_REL32
    if r_type == R_X86_64_32:
        return KOFO_RELOC_ABS32
    if r_type == R_X86_64_32S:
        raise KofoPackError(
            "R_X86_64_32S is unsupported; build modules with "
            "-mcmodel=large or PIE-safe module flags")
    raise KofoPackError(f"unsupported x86_64 relocation type {r_type}")


def build_relocs(
    raw_relocs: list[tuple[int, int, int, int, int]],
    section_map: dict[int, int],
    symbol_map: dict[int, int],
) -> list[KofoReloc]:
    kofo_relocs: list[KofoReloc] = []
    for target, offset, r_type, symbol, addend in raw_relocs:
        ktype = reloc_type(r_type)
        if ktype is None:
            continue
        kofo_relocs.append(KofoReloc(
            section=section_map[target],
            reloc_type=ktype,
            symbol=symbol_map[symbol],
            offset=offset,
            addend=addend,
        ))
    return kofo_relocs


def derive_name(path: str) -> str:
    base = os.path.basename(path)
    for suffix in (".ko", ".o"):
        if base.endswith(suffix):
            return base[:-len(suffix)]
    return os.path.splitext(base)[0]


def output_path(input_path: str, output: str | None) -> str:
    if output:
        return output
    return os.path.splitext(input_path)[0] + ".kofo"


def collect_strings(
    strings: StringTable,
    args: argparse.Namespace,
    sections: list[KofoSection],
    symbols: list[KofoSymbol],
    imports: list[str],
    drivers: list[KofoDriver],
) -> dict[str, int]:
    offsets = {
        "name": strings.add(args.name),
        "version": strings.add(args.version),
        "abi": strings.add(args.abi),
        "init": strings.add(args.init),
        "exit": strings.add(args.exit),
    }
    for section in sections:
        strings.add(section.name)
    for symbol in symbols:
        strings.add(symbol.name)
    for name in imports:
        strings.add(name)
    for driver in drivers:
        strings.add(driver.name)
        strings.add(driver.bus)
        strings.add(driver.class_name)
        strings.add(driver.probe)
        strings.add(driver.attach)
        strings.add(driver.detach)
    return offsets


def pack_kofo(
    args: argparse.Namespace,
    sections: list[KofoSection],
    symbols: list[KofoSymbol],
    imports: list[str],
    relocs: list[KofoReloc],
    drivers: list[KofoDriver],
) -> bytes:
    strings = StringTable()
    names = collect_strings(strings, args, sections, symbols, imports, drivers)
    header_size = struct.calcsize(KOFO_HEADER)

    pos = align(header_size, 8)
    string_off = pos
    string_data = bytes(strings.data)
    pos += len(string_data)
    pos = align(pos, 8)
    section_off = pos
    pos += len(sections) * struct.calcsize(KOFO_SECTION)
    pos = align(pos, 8)
    symbol_off = pos
    pos += len(symbols) * struct.calcsize(KOFO_SYMBOL)
    pos = align(pos, 8)
    import_off = pos
    pos += len(imports) * struct.calcsize(KOFO_IMPORT)
    pos = align(pos, 8)
    reloc_off = pos
    pos += len(relocs) * struct.calcsize(KOFO_RELOC)
    pos = align(pos, 8)
    driver_off = pos
    pos += len(drivers) * struct.calcsize(KOFO_DRIVER)
    pos = align(pos, 8)

    for section in sections:
        if section.flags & KOFO_SEC_F_BSS or section.size == 0:
            section.data_off = 0
            continue
        section.data_off = pos
        pos += len(section.data)
        pos = align(pos, 8)

    file_size = pos
    out = bytearray(file_size)
    header = struct.pack(
        KOFO_HEADER,
        KOFO_MAGIC,
        KOFO_VERSION,
        header_size,
        KOFO_ARCH_X86_64,
        KOFO_ABI_KERNEL,
        0,
        file_size,
        names["name"],
        names["version"],
        names["abi"],
        names["init"],
        names["exit"],
        string_off,
        len(string_data),
        section_off,
        len(sections),
        symbol_off,
        len(symbols),
        import_off,
        len(imports),
        reloc_off,
        len(relocs),
        driver_off,
        len(drivers),
        *([0] * 8),
    )
    out[:header_size] = header
    out[string_off:string_off + len(string_data)] = string_data

    off = section_off
    for section in sections:
        entry = struct.pack(
            KOFO_SECTION,
            strings.add(section.name),
            section.sec_type,
            section.flags,
            section.align,
            section.size,
            section.data_off,
            0,
        )
        out[off:off + len(entry)] = entry
        off += len(entry)

    off = symbol_off
    for symbol in symbols:
        entry = struct.pack(
            KOFO_SYMBOL,
            strings.add(symbol.name),
            symbol.section,
            symbol.flags,
            symbol.value,
            symbol.size,
        )
        out[off:off + len(entry)] = entry
        off += len(entry)

    off = import_off
    for name in imports:
        entry = struct.pack(
            KOFO_IMPORT,
            strings.add(name),
            0,
            KOFO_STR_NONE,
            0,
        )
        out[off:off + len(entry)] = entry
        off += len(entry)

    off = reloc_off
    for reloc in relocs:
        entry = struct.pack(
            KOFO_RELOC,
            reloc.section,
            reloc.reloc_type,
            reloc.symbol,
            reloc.offset,
            reloc.addend,
        )
        out[off:off + len(entry)] = entry
        off += len(entry)

    off = driver_off
    for driver in drivers:
        entry = struct.pack(
            KOFO_DRIVER,
            strings.add(driver.name),
            strings.add(driver.bus),
            strings.add(driver.class_name),
            driver.driver_pass,
            driver.order,
            driver.flags,
            strings.add(driver.probe),
            strings.add(driver.attach),
            strings.add(driver.detach),
            *([0] * 5),
        )
        out[off:off + len(entry)] = entry
        off += len(entry)

    for section in sections:
        if section.data_off != 0:
            start = section.data_off
            out[start:start + len(section.data)] = section.data
    return bytes(out)


def make_drivers(args: argparse.Namespace) -> list[KofoDriver]:
    if not args.driver:
        return []
    flags = KOFO_DRIVER_F_PSEUDO if args.bus == "pseudo" else 0
    return [KofoDriver(
        name=args.driver,
        bus=args.bus,
        class_name=args.driver_class,
        driver_pass=args.driver_pass,
        order=args.order,
        flags=flags,
        probe=args.probe,
        attach=args.attach,
        detach=args.detach,
    )]


def parse_args(argv: list[str]) -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description="convert an x86_64 relocatable .ko ELF into KOFO")
    ap.add_argument("input")
    ap.add_argument("-o", "--output")
    ap.add_argument("--name")
    ap.add_argument("--version", default="0.1")
    ap.add_argument("--abi", default="otsos-kernel-kofo-1")
    ap.add_argument("--init", default="kofo_module_init")
    ap.add_argument("--exit", default="kofo_module_exit")
    ap.add_argument("--driver")
    ap.add_argument("--bus", default="pseudo")
    ap.add_argument("--class", dest="driver_class", default="misc")
    ap.add_argument("--pass", dest="driver_pass", type=int, default=110)
    ap.add_argument("--order", type=int, default=100)
    ap.add_argument("--probe")
    ap.add_argument("--attach")
    ap.add_argument("--detach")
    args = ap.parse_args(argv)
    if args.name is None:
        args.name = derive_name(args.input)
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        data, elf_sections, elf_symbols, _symtab_index = parse_elf(args.input)
        sections, section_map = build_sections(data, elf_sections)
        raw_relocs = collect_raw_relocs(data, elf_sections, section_map)
        symbols, symbol_map, imports = build_symbols(
            elf_sections, elf_symbols, section_map, raw_relocs)
        relocs = build_relocs(raw_relocs, section_map, symbol_map)
        drivers = make_drivers(args)
        kofo = pack_kofo(args, sections, symbols, imports, relocs, drivers)
        out = output_path(args.input, args.output)
        out_dir = os.path.dirname(out)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)
        with open(out, "wb") as f:
            f.write(kofo)
    except (OSError, KofoPackError, struct.error) as exc:
        print(f"kofo_pack: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
