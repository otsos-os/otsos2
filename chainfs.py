#!/usr/bin/env python3
import argparse
import os
import struct
import sys

CHAINFS_MAGIC = 0xCAFEBABE
CHAINFS_CLUSTER_SIZE = 4096
CHAINFS_MAX_FILENAME = 31
CHAINFS_EOF_MARKER = 0xFFFFFFFF
CHAINFS_FREE_BLOCK = 0x00000000
CHAINFS_MAX_PATH = 256

CHAINFS_TYPE_FILE = 0
CHAINFS_TYPE_DIR = 1

SUPERBLOCK_STRUCT = struct.Struct("<6I488s")
ENTRY_STRUCT = struct.Struct("<BB30sIIII12s")


def die(msg):
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


class ChainFS:
    def __init__(self, image_path):
        self.image_path = image_path
        self.fd = None
        self.sb = None
        self.data_area_start = None
        self.entries_per_cluster = CHAINFS_CLUSTER_SIZE // ENTRY_STRUCT.size
        self.current_dir_index = 0  # root index

    def open(self):
        if not os.path.exists(self.image_path):
            die(f"image not found: {self.image_path}")
        self.fd = open(self.image_path, "r+b")
        self._load_superblock()

    def close(self):
        if self.fd:
            self.fd.close()
            self.fd = None

    def _read_cluster(self, cluster):
        self.fd.seek(cluster * CHAINFS_CLUSTER_SIZE)
        data = self.fd.read(CHAINFS_CLUSTER_SIZE)
        if len(data) != CHAINFS_CLUSTER_SIZE:
            die(f"short read at cluster {cluster}")
        return data

    def _write_cluster(self, cluster, data):
        if len(data) != CHAINFS_CLUSTER_SIZE:
            die("cluster write size mismatch")
        self.fd.seek(cluster * CHAINFS_CLUSTER_SIZE)
        self.fd.write(data)
        self.fd.flush()

    def _load_superblock(self):
        data = self._read_cluster(0)
        magic, block_count, ft_blocks, bm_blocks, total_files, root_dir_block, _ = SUPERBLOCK_STRUCT.unpack(
            data[:512]
        )
        if magic != CHAINFS_MAGIC:
            die(f"invalid magic 0x{magic:x}, expected 0x{CHAINFS_MAGIC:x}")
        self.sb = {
            "magic": magic,
            "block_count": block_count,
            "file_table_block_count": ft_blocks,
            "block_map_block_count": bm_blocks,
            "total_files": total_files,
            "root_dir_block": root_dir_block,
        }
        self.data_area_start = 1 + ft_blocks + bm_blocks

    def info(self):
        sb = self.sb
        return {
            "block_count": sb["block_count"],
            "file_table_blocks": sb["file_table_block_count"],
            "block_map_blocks": sb["block_map_block_count"],
            "total_files": sb["total_files"],
            "data_area_start": self.data_area_start,
            "root_dir_block": sb["root_dir_block"],
        }

    def _entry_index_to_block_offset(self, index):
        block = 1 + (index // self.entries_per_cluster)
        offset = index % self.entries_per_cluster
        return block, offset

    def _read_entry_by_index(self, index):
        block, offset = self._entry_index_to_block_offset(index)
        data = self._read_cluster(block)
        start = offset * ENTRY_STRUCT.size
        chunk = data[start : start + ENTRY_STRUCT.size]
        status, ftype, name_raw, size, start_block, parent_block, nlink, _ = ENTRY_STRUCT.unpack(
            chunk
        )
        name = name_raw.split(b"\x00", 1)[0].decode("utf-8", "ignore")
        return {
            "status": status,
            "type": ftype,
            "name": name,
            "size": size,
            "start_block": start_block,
            "parent_block": parent_block,
            "nlink": nlink,
            "entry_index": index,
            "entry_block": block,
            "entry_offset": offset,
        }

    def _write_entry(self, entry):
        block = entry["entry_block"]
        offset = entry["entry_offset"]
        data = bytearray(self._read_cluster(block))
        name_bytes = entry["name"].encode("utf-8")[:30]
        name_bytes = name_bytes.ljust(30, b"\x00")
        packed = ENTRY_STRUCT.pack(
            entry["status"],
            entry["type"],
            name_bytes,
            entry["size"],
            entry["start_block"],
            entry["parent_block"],
            entry.get("nlink", 1),
            b"\x00" * 12,
        )
        start = offset * ENTRY_STRUCT.size
        data[start : start + ENTRY_STRUCT.size] = packed
        self._write_cluster(block, bytes(data))

    def _iter_entries(self):
        total_entries = self.sb["total_files"]
        for i in range(total_entries):
            yield self._read_entry_by_index(i)

    def _find_free_entry(self):
        total_entries = self.sb["total_files"]
        for i in range(total_entries):
            entry = self._read_entry_by_index(i)
            if entry["status"] == 0:
                return entry
        return None

    def _read_block_map_entry(self, block_index):
        entries_per_cluster = CHAINFS_CLUSTER_SIZE // 4
        map_block = block_index // entries_per_cluster
        map_offset = block_index % entries_per_cluster
        if map_block >= self.sb["block_map_block_count"]:
            return None
        cluster = 1 + self.sb["file_table_block_count"] + map_block
        data = self._read_cluster(cluster)
        start = map_offset * 4
        return struct.unpack("<I", data[start : start + 4])[0]

    def _write_block_map_entry(self, block_index, next_block):
        entries_per_cluster = CHAINFS_CLUSTER_SIZE // 4
        map_block = block_index // entries_per_cluster
        map_offset = block_index % entries_per_cluster
        if map_block >= self.sb["block_map_block_count"]:
            die("block map index out of range")
        cluster = 1 + self.sb["file_table_block_count"] + map_block
        data = bytearray(self._read_cluster(cluster))
        start = map_offset * 4
        data[start : start + 4] = struct.pack("<I", next_block)
        self._write_cluster(cluster, bytes(data))

    def _find_free_blocks(self, count):
        total_data_blocks = self.sb["block_count"] - self.data_area_start
        found = []
        for i in range(total_data_blocks):
            nxt = self._read_block_map_entry(i)
            if nxt == CHAINFS_FREE_BLOCK:
                found.append(i)
                if len(found) == count:
                    return found
        return None

    def _free_block_chain(self, start_block):
        current = start_block
        while current != CHAINFS_EOF_MARKER:
            nxt = self._read_block_map_entry(current)
            if nxt is None:
                break
            self._write_block_map_entry(current, CHAINFS_FREE_BLOCK)
            current = nxt

    def _split_path(self, path):
        parts = []
        path = path.strip()
        if path.startswith("/"):
            path = path[1:]
        for comp in path.split("/"):
            if comp:
                parts.append(comp)
        return parts

    def _find_in_directory(self, dir_index, name):
        for entry in self._iter_entries():
            if entry["status"] != 1:
                continue
            if entry["name"] != name:
                continue
            if entry["parent_block"] == dir_index:
                return entry
            # compatibility with buggy parent_block storing entry_block
            if entry["parent_block"] == entry["entry_block"]:
                if entry["entry_block"] == self._entry_index_to_block_offset(
                    dir_index
                )[0]:
                    return entry
        return None

    def resolve_path(self, path):
        if path == "/":
            return self._read_entry_by_index(0)

        parts = self._split_path(path)
        current_index = 0 if path.startswith("/") else self.current_dir_index
        for i, comp in enumerate(parts):
            entry = self._find_in_directory(current_index, comp)
            if not entry:
                return None
            if i == len(parts) - 1:
                return entry
            if entry["type"] != CHAINFS_TYPE_DIR:
                return None
            current_index = entry["entry_index"]
        return None

    def list_dir(self, path):
        if path in ("", "."):
            dir_index = self.current_dir_index
        elif path == "/":
            dir_index = 0
        else:
            entry = self.resolve_path(path)
            if not entry or entry["type"] != CHAINFS_TYPE_DIR:
                die("not a directory")
            dir_index = entry["entry_index"]

        out = []
        for entry in self._iter_entries():
            if entry["status"] != 1:
                continue
            if entry["parent_block"] == dir_index:
                out.append(entry)
            else:
                # compatibility with buggy parent_block storing entry_block
                dir_block = self._entry_index_to_block_offset(dir_index)[0]
                if entry["parent_block"] == dir_block:
                    out.append(entry)
        return out

    def read_file(self, path):
        entry = self.resolve_path(path)
        if not entry or entry["type"] != CHAINFS_TYPE_FILE:
            die("file not found")
        remaining = entry["size"]
        data = bytearray()
        current = entry["start_block"]
        while remaining > 0 and current != CHAINFS_EOF_MARKER:
            cluster = self.data_area_start + current
            block_data = self._read_cluster(cluster)
            take = min(remaining, CHAINFS_CLUSTER_SIZE)
            data += block_data[:take]
            remaining -= take
            if remaining > 0:
                nxt = self._read_block_map_entry(current)
                if nxt is None:
                    break
                current = nxt
        return bytes(data)

    def write_file(self, path, payload):
        parent_path, name = os.path.split(path)
        if name == "":
            die("invalid path")
        if len(name) > 29:
            die("filename too long (max 29)")

        parent_index = 0
        if parent_path not in ("", "/"):
            parent_entry = self.resolve_path(parent_path)
            if not parent_entry or parent_entry["type"] != CHAINFS_TYPE_DIR:
                die("parent directory not found")
            parent_index = parent_entry["entry_index"]

        existing = self.resolve_path(path)
        if existing and existing["type"] != CHAINFS_TYPE_FILE:
            die("path exists and is not a file")

        if existing:
            self._free_block_chain(existing["start_block"])
            entry = existing
        else:
            entry = self._find_free_entry()
            if not entry:
                die("no free file entries")

        size = len(payload)
        blocks_needed = (size + CHAINFS_CLUSTER_SIZE - 1) // CHAINFS_CLUSTER_SIZE
        if blocks_needed == 0:
            blocks_needed = 1

        blocks = self._find_free_blocks(blocks_needed)
        if not blocks:
            die("not enough free blocks")

        remaining = size
        offset = 0
        for i, blk in enumerate(blocks):
            chunk = payload[offset : offset + CHAINFS_CLUSTER_SIZE]
            offset += CHAINFS_CLUSTER_SIZE
            remaining -= len(chunk)
            data = chunk.ljust(CHAINFS_CLUSTER_SIZE, b"\x00")
            self._write_cluster(self.data_area_start + blk, data)
            nxt = blocks[i + 1] if i + 1 < len(blocks) else CHAINFS_EOF_MARKER
            self._write_block_map_entry(blk, nxt)

        entry.update(
            {
                "status": 1,
                "type": CHAINFS_TYPE_FILE,
                "name": name,
                "size": size,
                "start_block": blocks[0],
                "parent_block": parent_index,
            }
        )
        self._write_entry(entry)

    def delete_file(self, path):
        entry = self.resolve_path(path)
        if not entry or entry["type"] != CHAINFS_TYPE_FILE:
            die("file not found")
        self._free_block_chain(entry["start_block"])
        entry["status"] = 0
        self._write_entry(entry)

    def mkdir(self, path):
        parent_path, name = os.path.split(path.rstrip("/"))
        if name == "":
            die("invalid path")
        if len(name) > 29:
            die("name too long (max 29)")

        if self.resolve_path(path):
            die("already exists")

        parent_index = 0
        if parent_path not in ("", "/"):
            parent_entry = self.resolve_path(parent_path)
            if not parent_entry or parent_entry["type"] != CHAINFS_TYPE_DIR:
                die("parent directory not found")
            parent_index = parent_entry["entry_index"]

        entry = self._find_free_entry()
        if not entry:
            die("no free entries")

        entry.update(
            {
                "status": 1,
                "type": CHAINFS_TYPE_DIR,
                "name": name,
                "size": 0,
                "start_block": 0,
                "parent_block": parent_index,
            }
        )
        self._write_entry(entry)

    def rmdir(self, path):
        entry = self.resolve_path(path)
        if not entry or entry["type"] != CHAINFS_TYPE_DIR:
            die("directory not found")
        if entry["entry_index"] == 0:
            die("cannot remove root")
        # check empty
        children = self.list_dir(path)
        if children:
            die("directory not empty")
        entry["status"] = 0
        self._write_entry(entry)

    def format(self, total_clusters, max_files):
        entries_per_cluster = CHAINFS_CLUSTER_SIZE // ENTRY_STRUCT.size
        file_table_clusters = (
            max_files + entries_per_cluster - 1
        ) // entries_per_cluster

        data_clusters = total_clusters - 1 - file_table_clusters
        map_entries_per_cluster = CHAINFS_CLUSTER_SIZE // 4
        block_map_clusters = (
            data_clusters + map_entries_per_cluster - 1
        ) // map_entries_per_cluster
        data_clusters = total_clusters - 1 - file_table_clusters - block_map_clusters

        sb = struct.pack(
            "<6I488s",
            CHAINFS_MAGIC,
            total_clusters,
            file_table_clusters,
            block_map_clusters,
            max_files,
            0,
            b"\x00" * 488,
        )
        self._write_cluster(0, sb.ljust(CHAINFS_CLUSTER_SIZE, b"\x00"))

        zero = b"\x00" * CHAINFS_CLUSTER_SIZE
        # root entry in first file table cluster
        entries = bytearray(zero)
        root = ENTRY_STRUCT.pack(
            1,
            CHAINFS_TYPE_DIR,
            b"/".ljust(30, b"\x00"),
            0,
            0,
            0xFFFFFFFF,
            b"\x00" * 16,
        )
        entries[: ENTRY_STRUCT.size] = root
        self._write_cluster(1, bytes(entries))

        for cl in range(2, 1 + file_table_clusters):
            self._write_cluster(cl, zero)

        # block map
        map_entry = struct.pack("<I", CHAINFS_FREE_BLOCK) * (
            CHAINFS_CLUSTER_SIZE // 4
        )
        for cl in range(
            1 + file_table_clusters,
            1 + file_table_clusters + block_map_clusters,
        ):
            self._write_cluster(cl, map_entry)

        self._load_superblock()


def main():
    # Pass 1: only grab -i/--image wherever it appears.
    base = argparse.ArgumentParser(add_help=False)
    base.add_argument(
        "-i",
        "--image",
        default=os.path.join(os.path.dirname(__file__), "bin", "disk.img"),
    )
    base_args, remaining = base.parse_known_args()

    # Full parser with subcommands.
    parser = argparse.ArgumentParser(description="ChainFS host utility")
    parser.add_argument(
        "-i",
        "--image",
        default=base_args.image,
        help="path to disk image (can be placed before or after subcommand)",
    )

    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("info")

    ls_p = sub.add_parser("ls")
    ls_p.add_argument("path", nargs="?", default="")

    cat_p = sub.add_parser("cat")
    cat_p.add_argument("path")

    get_p = sub.add_parser("get")
    get_p.add_argument("path")
    get_p.add_argument("dest")

    put_p = sub.add_parser("put")
    put_p.add_argument("src")
    put_p.add_argument("path")

    rm_p = sub.add_parser("rm")
    rm_p.add_argument("path")

    mk_p = sub.add_parser("mkdir")
    mk_p.add_argument("path")

    rd_p = sub.add_parser("rmdir")
    rd_p.add_argument("path")

    fmt_p = sub.add_parser("format")
    fmt_p.add_argument("total_blocks", type=int)
    fmt_p.add_argument("max_files", type=int)

    args = parser.parse_args(remaining)

    fs = ChainFS(args.image)
    fs.open()
    try:
        if args.cmd == "info":
            info = fs.info()
            for k, v in info.items():
                print(f"{k}: {v}")
        elif args.cmd == "ls":
            entries = fs.list_dir(args.path)
            for e in entries:
                t = "d" if e["type"] == CHAINFS_TYPE_DIR else "f"
                print(f"{t} {e['size']:8d} {e['name']}")
        elif args.cmd == "cat":
            data = fs.read_file(args.path)
            sys.stdout.buffer.write(data)
        elif args.cmd == "get":
            data = fs.read_file(args.path)
            with open(args.dest, "wb") as out:
                out.write(data)
        elif args.cmd == "put":
            with open(args.src, "rb") as f:
                data = f.read()
            fs.write_file(args.path, data)
        elif args.cmd == "rm":
            fs.delete_file(args.path)
        elif args.cmd == "mkdir":
            fs.mkdir(args.path)
        elif args.cmd == "rmdir":
            fs.rmdir(args.path)
        elif args.cmd == "format":
            fs.format(args.total_blocks, args.max_files)
    finally:
        fs.close()


if __name__ == "__main__":
    main()
