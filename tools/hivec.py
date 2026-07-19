#!/usr/bin/env python3
import argparse
import binascii
import os
import struct
import sys
from dataclasses import dataclass, field


PACK_MAGIC = b"OTSHPK1\0"
HIVE_MAGIC = b"OTSHIV1\0"
FORMAT_VERSION = 1

PACK_HEADER = "<8sIIIIIIII"
PACK_ENTRY = "<32sIIII"
HIVE_HEADER = "<8sIIIIIIIIIIIII"
NODE_ENTRY = "<IIIIIII"
VALUE_ENTRY = "<IIIIII"

TYPE_STRING = 1
TYPE_BOOL = 2
TYPE_I32 = 3
TYPE_U32 = 4
TYPE_U64 = 5
TYPE_IPV4 = 6
TYPE_BYTES = 7
TYPE_MULTI_STRING = 8

TYPE_IDS = {
    "string": TYPE_STRING,
    "bool": TYPE_BOOL,
    "i32": TYPE_I32,
    "u32": TYPE_U32,
    "u64": TYPE_U64,
    "ipv4": TYPE_IPV4,
    "bytes": TYPE_BYTES,
    "multi_string": TYPE_MULTI_STRING,
}

ATTR_FLAGS = {
    "protected": 0x00000001,
    "volatile": 0x00000002,
    "readonly": 0x00000004,
    "boot": 0x00000008,
}


@dataclass
class Token:
    kind: str
    text: str
    line: int
    col: int


@dataclass
class Value:
    name: str
    type_name: str
    flags: int
    data: bytes


@dataclass
class Key:
    path: str
    flags: int
    values: list[Value] = field(default_factory=list)


@dataclass
class Hive:
    name: str
    flags: int
    keys: list[Key] = field(default_factory=list)


@dataclass
class Node:
    path: str
    name: str
    flags: int = 0
    values: list[Value] = field(default_factory=list)


class HiveError(Exception):
    pass


class Lexer:
    def __init__(self, text: str, filename: str):
        self.text = text
        self.filename = filename
        self.pos = 0
        self.line = 1
        self.col = 1
        self.tokens: list[Token] = []

    def error(self, msg: str) -> HiveError:
        return HiveError(f"{self.filename}:{self.line}:{self.col}: {msg}")

    def peek(self, off: int = 0) -> str:
        idx = self.pos + off
        if idx >= len(self.text):
            return ""
        return self.text[idx]

    def advance(self) -> str:
        ch = self.peek()
        self.pos += 1
        if ch == "\n":
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return ch

    def add(self, kind: str, text: str, line: int, col: int) -> None:
        self.tokens.append(Token(kind, text, line, col))

    def skip_comment(self) -> None:
        while self.peek() and self.peek() != "\n":
            self.advance()

    def read_string(self) -> str:
        out = []
        self.advance()
        while self.peek():
            ch = self.advance()
            if ch == "\"":
                return "".join(out)
            if ch != "\\":
                out.append(ch)
                continue
            esc = self.advance()
            if esc == "n":
                out.append("\n")
            elif esc == "r":
                out.append("\r")
            elif esc == "t":
                out.append("\t")
            elif esc == "0":
                out.append("\0")
            elif esc in ("\\", "\""):
                out.append(esc)
            else:
                raise self.error(f"bad escape \\{esc}")
        raise self.error("unterminated string")

    def read_id(self) -> str:
        out = []
        while True:
            ch = self.peek()
            if not ch or not (ch.isalnum() or ch in "_.-"):
                break
            out.append(self.advance())
        return "".join(out)

    def read_number(self) -> str:
        out = []
        if self.peek() == "-":
            out.append(self.advance())
        while True:
            ch = self.peek()
            if not ch or not (ch.isdigit() or ch == "."):
                break
            out.append(self.advance())
        return "".join(out)

    def lex(self) -> list[Token]:
        while self.peek():
            ch = self.peek()
            if ch.isspace():
                self.advance()
                continue
            if ch == "#":
                self.skip_comment()
                continue
            if ch == "/" and self.peek(1) == "/":
                self.skip_comment()
                continue
            line = self.line
            col = self.col
            if ch in "{}=@[],":
                self.add(ch, ch, line, col)
                self.advance()
                continue
            if ch == "\"":
                self.add("STRING", self.read_string(), line, col)
                continue
            if ch.isalpha() or ch == "_":
                self.add("ID", self.read_id(), line, col)
                continue
            if ch.isdigit() or (ch == "-" and self.peek(1).isdigit()):
                text = self.read_number()
                kind = "IPV4" if "." in text else "INT"
                self.add(kind, text, line, col)
                continue
            raise self.error(f"unexpected character {ch!r}")
        self.add("EOF", "", self.line, self.col)
        return self.tokens


class Parser:
    def __init__(self, tokens: list[Token], filename: str):
        self.tokens = tokens
        self.filename = filename
        self.pos = 0

    def cur(self) -> Token:
        return self.tokens[self.pos]

    def error(self, token: Token, msg: str) -> HiveError:
        return HiveError(f"{self.filename}:{token.line}:{token.col}: {msg}")

    def accept(self, kind: str, text: str | None = None) -> Token | None:
        tok = self.cur()
        if tok.kind != kind:
            return None
        if text is not None and tok.text != text:
            return None
        self.pos += 1
        return tok

    def expect(self, kind: str, text: str | None = None) -> Token:
        tok = self.accept(kind, text)
        if tok:
            return tok
        want = text if text is not None else kind
        raise self.error(self.cur(), f"expected {want}")

    def expect_id(self, text: str | None = None) -> str:
        return self.expect("ID", text).text

    def parse_attrs(self) -> int:
        flags = 0
        while self.accept("@"):
            name = self.expect_id()
            if name not in ATTR_FLAGS:
                raise self.error(self.cur(), f"unknown attr @{name}")
            flags |= ATTR_FLAGS[name]
        return flags

    def parse_string_list(self) -> list[str]:
        vals = []
        self.expect("[")
        if self.accept("]"):
            return vals
        while True:
            vals.append(self.expect("STRING").text)
            if self.accept("]"):
                return vals
            self.expect(",")

    def encode_value(self, type_name: str) -> bytes:
        if type_name == "string":
            return self.expect("STRING").text.encode("utf-8")
        if type_name == "bool":
            val = self.expect("ID").text
            if val not in ("true", "false"):
                raise self.error(self.cur(), "bool must be true or false")
            return b"\x01" if val == "true" else b"\x00"
        if type_name == "i32":
            return struct.pack("<i", self.read_int(-2147483648, 2147483647))
        if type_name == "u32":
            return struct.pack("<I", self.read_int(0, 0xFFFFFFFF))
        if type_name == "u64":
            return struct.pack("<Q", self.read_int(0, 0xFFFFFFFFFFFFFFFF))
        if type_name == "ipv4":
            return self.read_ipv4()
        if type_name == "bytes":
            return self.read_bytes()
        if type_name == "multi_string":
            vals = self.parse_string_list()
            return b"\0".join(v.encode("utf-8") for v in vals) + b"\0"
        raise self.error(self.cur(), f"unknown type {type_name}")

    def read_int(self, min_val: int, max_val: int) -> int:
        tok = self.expect("INT")
        try:
            val = int(tok.text, 10)
        except ValueError:
            raise self.error(tok, "bad integer") from None
        if val < min_val or val > max_val:
            raise self.error(tok, "integer out of range")
        return val

    def read_ipv4(self) -> bytes:
        tok = self.expect("IPV4")
        parts = tok.text.split(".")
        if len(parts) != 4:
            raise self.error(tok, "bad ipv4 address")
        nums = []
        for part in parts:
            if not part.isdigit():
                raise self.error(tok, "bad ipv4 address")
            num = int(part, 10)
            if num > 255:
                raise self.error(tok, "ipv4 octet out of range")
            nums.append(num)
        return bytes(nums)

    def read_bytes(self) -> bytes:
        self.expect_id("hex")
        tok = self.expect("STRING")
        text = "".join(tok.text.split())
        if len(text) % 2 != 0:
            raise self.error(tok, "hex string has odd length")
        try:
            return bytes.fromhex(text)
        except ValueError:
            raise self.error(tok, "bad hex string") from None

    def parse_value(self) -> Value:
        type_name = self.expect_id()
        if type_name not in TYPE_IDS:
            raise self.error(self.cur(), f"unknown type {type_name}")
        name = self.expect_id()
        flags = self.parse_attrs()
        self.expect("=")
        data = self.encode_value(type_name)
        return Value(name, type_name, flags, data)

    def parse_key(self) -> Key:
        self.expect_id("key")
        path = self.expect_id()
        flags = self.parse_attrs()
        key = Key(path, flags)
        seen = set()
        self.expect("{")
        while not self.accept("}"):
            value = self.parse_value()
            if value.name in seen:
                raise self.error(self.cur(), f"duplicate value {value.name}")
            seen.add(value.name)
            key.values.append(value)
        return key

    def parse(self) -> Hive:
        self.expect_id("hive")
        name = self.expect_id()
        flags = self.parse_attrs()
        hive = Hive(name, flags)
        seen = set()
        self.expect("{")
        while not self.accept("}"):
            key = self.parse_key()
            if key.path in seen:
                raise self.error(self.cur(), f"duplicate key {key.path}")
            seen.add(key.path)
            hive.keys.append(key)
        self.expect("EOF")
        return hive


class StringTable:
    def __init__(self):
        self.data = bytearray()
        self.index: dict[str, int] = {}

    def add(self, text: str) -> int:
        if text in self.index:
            return self.index[text]
        off = len(self.data)
        self.data.extend(text.encode("utf-8"))
        self.data.append(0)
        self.index[text] = off
        return off


def align(buf: bytearray, n: int) -> None:
    while len(buf) % n != 0:
        buf.append(0)


def parse_file(path: str) -> Hive:
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    tokens = Lexer(text, path).lex()
    hive = Parser(tokens, path).parse()
    stem = os.path.splitext(os.path.basename(path))[0]
    if stem != hive.name:
        raise HiveError(f"{path}: hive name must match filename stem")
    return hive


def build_nodes(hive: Hive) -> tuple[list[Node], dict[str, int]]:
    nodes: dict[str, Node] = {"": Node("", "")}
    for key in hive.keys:
        parts = key.path.split(".")
        path = ""
        for part in parts:
            if not part:
                raise HiveError(f"{hive.name}: bad key path {key.path}")
            path = part if not path else f"{path}.{part}"
            nodes.setdefault(path, Node(path, part))
        nodes[key.path].flags |= key.flags
        nodes[key.path].values.extend(key.values)

    paths = [""] + sorted((p for p in nodes if p), key=lambda p: (p.count("."), p))
    out = [nodes[p] for p in paths]
    index = {node.path: i for i, node in enumerate(out)}
    return out, index


def parent_path(path: str) -> str:
    if "." not in path:
        return ""
    return path.rsplit(".", 1)[0]


def build_hive(hive: Hive) -> bytes:
    nodes, node_index = build_nodes(hive)
    strings = StringTable()
    data = bytearray()
    node_entries = bytearray()
    value_entries = bytearray()
    child_map: dict[int, list[int]] = {i: [] for i in range(len(nodes))}
    value_map: dict[int, list[int]] = {i: [] for i in range(len(nodes))}
    value_rows = []

    strings.add("")
    for node in nodes:
        strings.add(node.name)
    for idx, node in enumerate(nodes):
        if idx == 0:
            continue
        pidx = node_index[parent_path(node.path)]
        child_map[pidx].append(idx)
    for nidx, node in enumerate(nodes):
        for value in sorted(node.values, key=lambda v: v.name):
            strings.add(value.name)
            off = len(data)
            data.extend(value.data)
            value_rows.append((nidx, value, off, len(value.data)))
            value_map[nidx].append(len(value_rows) - 1)

    for idx, node in enumerate(nodes):
        pidx = 0xFFFFFFFF if idx == 0 else node_index[parent_path(node.path)]
        children = child_map[idx]
        values = value_map[idx]
        first_child = children[0] if children else 0xFFFFFFFF
        first_value = values[0] if values else 0xFFFFFFFF
        node_entries.extend(struct.pack(
            NODE_ENTRY,
            pidx,
            strings.add(node.name),
            node.flags,
            first_child,
            len(children),
            first_value,
            len(values),
        ))

    for nidx, value, off, size in value_rows:
        value_entries.extend(struct.pack(
            VALUE_ENTRY,
            nidx,
            strings.add(value.name),
            TYPE_IDS[value.type_name],
            value.flags,
            off,
            size,
        ))

    header_size = struct.calcsize(HIVE_HEADER)
    node_off = header_size
    value_off = node_off + len(node_entries)
    string_off = value_off + len(value_entries)
    string_size = len(strings.data)
    data_off = string_off + string_size
    total_size = data_off + len(data)
    payload = node_entries + value_entries + strings.data + data
    crc = binascii.crc32(payload) & 0xFFFFFFFF

    header = struct.pack(
        HIVE_HEADER,
        HIVE_MAGIC,
        FORMAT_VERSION,
        header_size,
        hive.flags,
        len(nodes),
        len(value_rows),
        node_off,
        value_off,
        string_off,
        string_size,
        data_off,
        len(data),
        total_size,
        crc,
    )
    return header + payload


def fixed_name(name: str) -> bytes:
    raw = name.encode("ascii")
    if len(raw) >= 32:
        raise HiveError(f"hive name too long: {name}")
    return raw + b"\0" * (32 - len(raw))


def build_pack(hives: list[Hive]) -> bytes:
    seen = set()
    hive_blobs = []
    for hive in hives:
        if hive.name in seen:
            raise HiveError(f"duplicate hive {hive.name}")
        seen.add(hive.name)
        hive_blobs.append((hive, build_hive(hive)))

    header_size = struct.calcsize(PACK_HEADER)
    entry_size = struct.calcsize(PACK_ENTRY)
    table_off = header_size
    data_off = table_off + entry_size * len(hive_blobs)
    body = bytearray()
    entries = bytearray()
    offset = data_off

    for hive, blob in hive_blobs:
        align(body, 8)
        offset = data_off + len(body)
        body.extend(blob)
        crc = binascii.crc32(blob) & 0xFFFFFFFF
        entries.extend(struct.pack(
            PACK_ENTRY,
            fixed_name(hive.name),
            offset,
            len(blob),
            hive.flags,
            crc,
        ))

    total_size = data_off + len(body)
    payload = entries + body
    crc = binascii.crc32(payload) & 0xFFFFFFFF
    header = struct.pack(
        PACK_HEADER,
        PACK_MAGIC,
        FORMAT_VERSION,
        header_size,
        len(hive_blobs),
        table_off,
        data_off,
        total_size,
        0,
        crc,
    )
    return header + payload


def dump_pack(hives: list[Hive]) -> None:
    for hive in hives:
        values = sum(len(key.values) for key in hive.keys)
        print(f"{hive.name}: keys={len(hive.keys)} values={values}")
        for key in hive.keys:
            print(f"  {key.path}: values={len(key.values)}")


def list_boot_modules(hives: list[Hive]) -> None:
    names = []
    for hive in hives:
        if hive.name != "BOOT":
            continue
        for key in hive.keys:
            if not key.path.startswith("Modules."):
                continue
            name = key.path[len("Modules."):]
            if not name or "." in name:
                continue
            if any(value.name == "Dest" for value in key.values):
                names.append(name)
    print(" ".join(names))


def main() -> int:
    ap = argparse.ArgumentParser(description="compile otsos registry hives")
    ap.add_argument("-o", "--output")
    ap.add_argument("--dump", action="store_true")
    ap.add_argument("--list-boot-modules", action="store_true")
    ap.add_argument("inputs", nargs="+")
    args = ap.parse_args()

    try:
        hives = [parse_file(path) for path in sorted(args.inputs)]
        if args.list_boot_modules:
            list_boot_modules(hives)
        if args.output:
            data = build_pack(hives)
            out_dir = os.path.dirname(args.output)
            if out_dir:
                os.makedirs(out_dir, exist_ok=True)
            with open(args.output, "wb") as f:
                f.write(data)
        if args.dump:
            dump_pack(hives)
            if args.output:
                print(f"wrote {args.output}: {len(data)} bytes")
    except (OSError, HiveError) as e:
        print(f"hivec: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
