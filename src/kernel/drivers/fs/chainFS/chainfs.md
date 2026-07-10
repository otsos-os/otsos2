# ChainFS (CFS) — Specification v3.0 (Symlinks + Hard Links)

## Key Characteristics
- **Block Size**: 512 bytes (equivalent to a standard disk sector).
- **File/Directory Name**: 30 bytes (fixed).
- **Max Disk Size**: 2 Terabytes (with 32-bit block addressing).
- **Structure**: **Hierarchical** (supports directories and nested paths).
- **Byte Order**: Little-Endian.
- **Max File Size**: ~2GB (32-bit size field).
- **Max Path Length**: 256 characters.
- **Entry Types**: File (0), Directory (1), Symlink (2).

## 1. Disk Layout
The disk is divided into 4 distinct regions. All addresses are block numbers (LBA), starting from 0.

| Zone       | Start (Block) | Size (Blocks) | Description                                    |
|------------|--------------|-----------------|---------------------------------------------|
| Superblock | 0            | 1               | System information + root directory reference |
| File Table | 1            | M               | Array of file and directory descriptors      |
| Block Map  | 1 + M        | N               | Chain map (defines the next block in a sequence) |
| Data Area  | 1 + M + N    | Remaining       | Actual file contents                         |

*Note: M (File Table size) and N (Block Map size) are calculated during formatting and stored in the Superblock.*

## 2. Data Structures

### 2.1. Superblock (Block 0)
The first 512 bytes of the disk.

| Offset | Type    | Name                   | Description                                    |
|--------|---------|----------------------|---------------------------------------------|
| 0x00   | u32     | Magic                | 0xCAFEBABE (Identification magic)           |
| 0x04   | u32     | BlockCount           | Total number of blocks on the disk          |
| 0x08   | u32     | FileTableBlockCount  | Size of "File Table" in blocks (M)          |
| 0x0C   | u32     | BlockMapBlockCount   | Size of "Block Map" in blocks (N)           |
| 0x10   | u32     | TotalFiles           | Maximum number of files/directories         |
| 0x14   | u32     | RootDirBlock         | Index of the root directory in the file table |
| 0x18   | u8[488] | Padding              | Zero-filled padding                         |

### 2.2. File Entry
The File Table is an array of these structures.
Size: 64 bytes (exactly 8 entries per 512-byte block).

| Offset | Type     | Name         | Description                                           |
|--------|----------|-------------|---------------------------------------------------|
| 0x00   | u8       | Status      | 0 = Free, 1 = Allocated                            |
| 0x01   | u8       | Type        | 0 = File, 1 = Directory, 2 = Symlink               |
| 0x02   | char[30] | Name        | Name (ASCII, NOT guaranteed to be null-terminated)  |
| 0x20   | u32      | Size        | File size in bytes (0 for directories, target len for symlinks) |
| 0x24   | u32      | StartBlock  | First block map index (0 for directories, EOF_MARKER for short symlinks) |
| 0x28   | u32      | ParentBlock | File-table index of the parent directory            |
| 0x2C   | u32      | Nlink       | Hard link reference count (1 = only this entry)     |
| 0x30   | u8[12]   | Reserved    | Short symlink target storage; zero otherwise        |

### 2.3. The Block Map
Each element corresponds to a block in the Data Area.
It acts as a linked list (chain) for file blocks.

**Block Map Values:**
- `0x00000000` (`CHAINFS_FREE_BLOCK`): Block is Free.
- `0xFFFFFFFF` (`CHAINFS_EOF_MARKER`): End Of File (EOF).
- Any other number: Index of the next block in the file's chain.

**Example:**
If a file occupies blocks 5, 6, and 9:
- Map[5] = 6
- Map[6] = 9
- Map[9] = 0xFFFFFFFF (End)

## 3. Hierarchical Directory Structure

### 3.1. Root Directory
- Automatically created during formatting.
- Name: `/`, Type: 1.
- ParentBlock: 0xFFFFFFFF (no parent).
- StartBlock: 0 (directories do not use data blocks).

### 3.2. Path Resolution
To find `/test/file.txt`:
1. Start at `RootDirBlock`.
2. Search for an entry with `Name = "test"` and `Type = 1` where `ParentBlock = RootDirBlock`.
3. Move to the found "test" directory.
4. Search for an entry with `Name = "file.txt"` where `ParentBlock = CurrentBlock`.

### 3.3. Child Elements
Child files, directories, and symlinks are identified by their `ParentBlock` field matching the index of their parent directory.

## 4. File Types

### 4.1. Regular Files (Type 0)
- `Size` = file content length in bytes.
- `StartBlock` = first block map index of the data chain.
- Data blocks are linked via the block map.

### 4.2. Directories (Type 1)
- `Size` = 0.
- `StartBlock` = 0 (no data blocks; membership is implicit via `ParentBlock`).
- Children are found by scanning the File Table for entries with matching `ParentBlock`.

### 4.3. Symbolic Links (Type 2)
- `Size` = target path length in bytes.
- **Short symlink** (target_len <= 12): target string stored inline in `Reserved[0..target_len-1]`, `StartBlock = 0xFFFFFFFF` (EOF_MARKER).
- **Long symlink** (target_len > 12): target string stored as data blocks (same mechanism as regular files), `StartBlock` points to the first block.
- Symlinks are followed by the VFS layer during path resolution (up to 40 hops).

## 5. Hard Links

ChainFS supports hard links. Each file entry has an `Nlink` field.

- When a file is first created: `Nlink = 1`.
- `chainfs_link(oldpath, newpath)`: creates a new entry with the same `StartBlock` and `Size` as `oldpath`, sets `Nlink = 1` on the new entry, increments `Nlink` on the original.
- `chainfs_delete_file`: decrements `Nlink`. Data blocks are freed only when `Nlink` reaches 0.
- Hard links to directories are not permitted.
- `Stat` reports `Nlink` as the link count.

## 6. Public API Functions

All functions return 0 on success, -1 on error (unless noted).

### Initialization & Formatting
- `chainfs_init(disk_t *disk)` — Attach and validate an existing ChainFS disk.
- `chainfs_format(u32 total_blocks, u32 max_files)` — Format a disk for ChainFS.

### File Operations
- `chainfs_find_file(path, entry, *entry_block, *entry_offset)` — Look up a file/dir by path.
- `chainfs_find_free_file_entry(*entry_block, *entry_offset)` — Find a free slot in the File Table.
- `chainfs_find_free_blocks(count, *blocks)` — Find `count` free data blocks.
- `chainfs_read_block_map_entry(block_index, *next_block)` — Read a block map entry.
- `chainfs_write_block_map_entry(block_index, next_block)` — Write a block map entry.
- `chainfs_free_block_chain(start_block)` — Free the entire block chain (void).
- `chainfs_read_file(filename, *buffer, buffer_size, *bytes_read)` — Read entire file.
- `chainfs_read_file_range(filename, *buffer, buffer_size, offset, *bytes_read)` — Read file at offset.
- `chainfs_write_file(filename, *data, size)` — Create or overwrite a file.
- `chainfs_delete_file(filename)` — Unlink a file (decrements nlink, frees data only at nlink==0).
- `chainfs_symlink(target, linkpath)` — Create a symbolic link.
- `chainfs_link(oldpath, newpath)` — Create a hard link.

### Directory Operations
- `chainfs_mkdir(path)` — Create a directory.
- `chainfs_rmdir(path)` — Remove an empty directory.
- `chainfs_chdir(path)` — Change current working directory.
- `chainfs_list_dir(path, *files, max_files, *file_count)` — List directory contents.
- `chainfs_get_file_list(*files, max_files, *file_count)` — List current directory (convenience wrapper).
- `chainfs_get_current_path(*buffer, buffer_size)` — Reconstruct absolute path of CWD.
- `chainfs_find_in_directory(dir_block, name, entry, *entry_block, *entry_offset)` — Search for name under a specific parent.
- `chainfs_resolve_path(path, entry, *entry_block, *entry_offset)` — Resolve full/relative path.

### Internal Helpers
- `read_entry_by_index(index, entry, *block, *offset)` — Read entry by flat file-table index.
- `split_path(path, components[][32], max_components)` — Split path into components.

## 7. Algorithms

### 7.1. Reading a file (`/test/file.txt`)
1. **Load Superblock**: Find `RootDirBlock`.
2. **Parse Path**: Split into components `["test", "file.txt"]`.
3. **Find "test" directory**: Scan File Table for `ParentBlock = RootDirBlock` and `Name = "test"`. Record its block index as `CurrentDirBlock`.
4. **Find "file.txt"**: Scan File Table for `ParentBlock = CurrentDirBlock` and `Name = "file.txt"`. Get `StartBlock` and `Size`.
5. **Read Data**: Follow the block chain in the Block Map.

### 7.2. Reading a file range (at offset)
1. Find the file entry (same as 7.1).
2. Compute block skip: `offset / 512` blocks are skipped from `StartBlock`.
3. Compute intra-block offset: `offset % 512`.
4. Walk the block chain to skip blocks, then read from intra-block offset up to `buffer_size`.

### 7.3. Creating a file (`/test/newfile.txt`)
1. **Parse Path**: Extract parent path `/test` and file name `newfile.txt`.
2. **Find Parent Directory**: Resolve the `/test` path.
3. **Validate**: Ensure the parent is a directory (`Type = 1`).
4. **Allocate Entry**: Find a free slot (`Status = 0`) in the File Table.
5. **Allocate Blocks**: Find free data blocks via `chainfs_find_free_blocks`.
6. **Write Data**: Write to each block, link them in the block map.
7. **Initialize Entry**: Status=1, Type=0, Name="newfile.txt", Nlink=1, ParentBlock, StartBlock, Size.

### 7.4. Creating a directory (`/test/newdir`)
1. **Parse Path**: Extract parent path `/test` and name `newdir`.
2. **Find Parent Directory**: `/test`.
3. **Allocate Entry**: Find a free slot in the File Table.
4. **Initialize Directory**: Status=1, Type=1, Name="newdir", ParentBlock, Nlink=1, StartBlock=0, Size=0.

### 7.5. Deleting a file (unlink)
1. **Find entry** by path.
2. **Decrement Nlink** on the entry.
3. If `Nlink > 1`: write entry back with decremented Nlink (do not free data — other hard links still reference it).
4. If `Nlink == 0`: free the block chain via `chainfs_free_block_chain`, set `Status = 0`.

### 7.6. Deleting a directory
1. Locate the directory via its path.
2. **Check empty**: Scan File Table — no entries with `ParentBlock = this_block`.
3. **Release**: Set `Status = 0` for the entry.

### 7.7. Creating a symbolic link
1. **Parse Path**: Extract parent and name from `linkpath`.
2. **Allocate Entry**: Find a free slot.
3. If `target_len <= 12`: store target in `Reserved[0..target_len-1]`, set `StartBlock = CHAINFS_EOF_MARKER`.
4. If `target_len > 12`: allocate data blocks, write target as data, chain them in the block map.
5. **Initialize Entry**: Status=1, Type=2 (SYMLINK), Nlink=1, ParentBlock, Size=target_len.

### 7.8. Reading a symbolic link
1. **Find entry** by `path`.
2. If short symlink (Size <= 12 AND StartBlock == CHAINFS_EOF_MARKER): copy from `Reserved[]` to buffer.
3. If long symlink: read data blocks (same as file read).

### 7.9. Creating a hard link
1. **Find oldpath entry**; reject if it is a directory.
2. **Parse newpath**: extract parent and name.
3. **Allocate Entry**: Find a free slot.
4. **Increment Nlink** on the old entry.
5. **Create new entry**: same `Type`, `StartBlock`, `Size` as old; `Nlink = 1`.

## 8. Shell Operations

### Directory Commands:
- `mkdir <path>` - Create directory.
- `rmdir <path>` - Remove empty directory.
- `cd <path>` - Change current directory.
- `cd` - Go to root.
- `ls` - List current directory.
- `ls <path>` - List specified directory.
- `mydir` - Print current working directory.

### Path Support:
- **Absolute**: `/test/file.txt`
- **Relative**: `file.txt`, `subdir/file.txt`
- **Max Length**: 256 characters.

## 9. Detailed Example

Imagine a small disk with 15 blocks. Block size = 512 bytes.

**Block 0 (Superblock)**: 
- FileTableBlockCount = 2, BlockMapBlockCount = 1, RootDirBlock = 0 (index 0 in file table)

**Blocks 1-2 (File Table)**:
```
Block 1:
  Entry 0: Status=1, Type=1, Name="/", ParentBlock=0xFFFFFFFF, Nlink=1 (root dir)
  Entry 1: Status=1, Type=1, Name="test", ParentBlock=0, Nlink=1 (test dir)
  Entry 2: Status=1, Type=0, Name="root.txt", ParentBlock=0, Nlink=1, Start=0, Size=600
  ...

Block 2:  
  Entry 0: Status=1, Type=0, Name="file1.txt", ParentBlock=0, Nlink=2, Start=2, Size=300 (hard linked)
  Entry 1: Status=1, Type=0, Name="hardlink1", ParentBlock=2, Nlink=1, Start=2, Size=300 (hard link to file1.txt)
  Entry 2: Status=1, Type=0, Name="deep.txt", ParentBlock=1 (test dir), Nlink=1, Start=4, Size=200
  Entry 3: Status=1, Type=2, Name="link.ln", ParentBlock=0, Nlink=1, Size=8, Start=EOF, Reserved="deep.txt" (symlink)
```

**Block 3 (Block Map)**: 
```
[1, EOF, 3, EOF, EOF, 0, 0, ...]
Index 0: value 1 (root.txt: block 0 -> block 1)
Index 1: value EOF (root.txt: end)  
Index 2: value 3 (file1.txt/hardlink1: block 2 -> block 3)
Index 3: value EOF (file1.txt/hardlink1: end)
Index 4: value EOF (deep.txt: single block)
```

**Block 4+ (Data Area)**:
```
Data Area Block 0 (Physical Block 4): root.txt data (part 1)
Data Area Block 1 (Physical Block 5): root.txt data (part 2) 
Data Area Block 2 (Physical Block 6): file1.txt data (part 1)
Data Area Block 3 (Physical Block 7): file1.txt data (part 2)
Data Area Block 4 (Physical Block 8): deep.txt data
```

**Directory Structure Representation**:
```
/
├── root.txt (600 bytes)
├── file1.txt (300 bytes, nlink=2)
├── hardlink1 (300 bytes, hard link to file1.txt)
├── link.ln -> deep.txt
└── test/
    └── deep.txt (200 bytes)
```

## 10. System Limits
- **Max File Size**: ~2GB (u32 size field).
- **Max Disk Size**: 2TB (u32 block addressing).  
- **Files 1KB+**:  Fully supported (automatic block fragmentation).
- **Max Files/Directories**: Configured during formatting.
- **Nesting Depth**: Virtually unlimited.
- **Name Length**: 30 characters.
- **Path Length**: 256 characters.
- **Short Symlink Target**: 12 characters inline.
- **Max Symlink Follow**: 40 hops (VFS limit).
