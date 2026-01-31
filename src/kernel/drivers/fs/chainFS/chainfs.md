# ChainFS (CFS) — Specification v2.0 (Directory Support)

## Key Characteristics
- **Block Size**: 512 bytes (equivalent to a standard disk sector).
- **File/Directory Name**: 30 bytes (fixed).
- **Max Disk Size**: 2 Terabytes (with 32-bit block addressing).
- **Structure**: **Hierarchical** (supports directories and nested paths).
- **Byte Order**: Little-Endian.
- **Max File Size**: ~2GB (32-bit size field).
- **Max Path Length**: 256 characters.

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
| 0x14   | u32     | RootDirBlock         | Block index containing the root directory entry |
| 0x18   | u8[488] | Padding              | Zero-filled padding                         |

### 2.2. File Entry
The File Table is an array of these structures.
Size: 64 bytes (exactly 8 entries per 512-byte block).

| Offset | Type     | Name         | Description                                           |
|--------|----------|-------------|---------------------------------------------------|
| 0x00   | u8       | Status      | 0 = Free, 1 = File/Directory exists               |
| 0x01   | u8       | Type        | 0 = File, 1 = Directory                           |
| 0x02   | char[30] | Name        | Name (ASCII, null-terminated)                     |
| 0x20   | u32      | Size        | File size in bytes (0 for directories)            |
| 0x24   | u32      | StartBlock  | First data block index (0 for directories)        |
| 0x28   | u32      | ParentBlock | Index of the parent directory (0 for root)        |
| 0x2C   | u8[16]   | Reserved    | Zero-filled reservation                           |

### 2.3. The Block Map
Each element corresponds to a block in the Data Area.
It acts as a linked list (chain) for file blocks.

**Block Map Values:**
- `0x00000000`: Block is Free.
- `0xFFFFFFFF`: End Of File (EOF).
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
- ParentBlock: 0 (no parent).
- StartBlock: 0 (directories do not use data blocks).

### 3.2. Path Resolution
To find `/test/file.txt`:
1. Start at `RootDirBlock`.
2. Search for an entry with `Name = "test"` and `Type = 1` where `ParentBlock = RootDirBlock`.
3. Move to the found "test" directory.
4. Search for an entry with `Name = "file.txt"` and `Type = 0` where `ParentBlock = CurrentBlock`.

### 3.3. Child Elements
Child files and directories are identified by their `ParentBlock` field matching the index of their parent directory.

## 4. Algorithms

### 4.1. Reading a file (`/test/file.txt`)
1. **Load Superblock**: Find `RootDirBlock`.
2. **Parse Path**: Split into components `["test", "file.txt"]`.
3. **Find "test" directory**:
   - Scan File Table for `ParentBlock = RootDirBlock` and `Name = "test"`.
   - Record its block index as `CurrentDirBlock`.
4. **Find "file.txt"**:
   - Scan File Table for `ParentBlock = CurrentDirBlock` and `Name = "file.txt"`.
   - Get `StartBlock` and `Size`.
5. **Read Data**: Similar to v1.0 — follow the block chain in the Block Map.

### 4.2. Creating a file (`/test/newfile.txt`)
1. **Parse Path**: Extract parent path `/test` and file name `newfile.txt`.
2. **Find Parent Directory**: Resolve the `/test` path.
3. **Validate**: Ensure the parent is indeed a directory (`Type = 1`).
4. **Allocate Entry**: Find a free slot (`Status = 0`) in the File Table.
5. **Initialize Entry**:
   - Status = 1, Type = 0, Name = "newfile.txt".
   - ParentBlock = `test` directory block.
   - Allocate data blocks and set `StartBlock` and `Size`.
6. **Write Data**: Similar to v1.0.

### 4.3. Creating a directory (`/test/newdir`)
1. **Parse Path**: Extract parent path `/test` and name `newdir`.
2. **Find Parent Directory**: `/test`.
3. **Allocate Entry**: Find a free slot in the File Table.
4. **Initialize Directory**:
   - Status = 1, Type = 1, Name = "newdir".
   - ParentBlock = `test` directory block.
   - StartBlock = 0, Size = 0 (directories do not use data blocks).

### 4.4. Deleting a directory
1. Locate the directory via its path.
2. **Check empty**: Ensure no entries exist with `ParentBlock = this_block`.
3. **Release**: Set `Status = 0` for the entry.

## 5. Shell Operations

### Directory Commands:
- `mkdir <path>` - Create directory.
- `rmdir <path>` - Remove empty directory.
- `cd <path>` - Change current directory.
- `cd` - Go to root.
- `ls` - List current directory.
- `ls <path>` - List specified directory.
- `mydir` - Print current working directory (e.g., info about current path).

### Path Support:
- **Absolute**: `/test/file.txt`
- **Relative**: `file.txt`, `subdir/file.txt`
- **Max Length**: 256 characters.

## 6. Detailed Example

Imagine a small disk with 15 blocks. Block size = 512 bytes.

**Block 0 (Superblock)**: 
- FileTableBlockCount = 2, BlockMapBlockCount = 1, RootDirBlock = 1 (index in the table, effectively points to the entry)

**Blocks 1-2 (File Table)**:
```
Block 1:
  Entry 0: Status=1, Type=1, Name="/", ParentBlock=0      (root dir)
  Entry 1: Status=1, Type=1, Name="test", ParentBlock=1   (test dir)
  Entry 2: Status=1, Type=0, Name="root.txt", ParentBlock=1, Start=0, Size=600
  ...

Block 2:  
  Entry 0: Status=1, Type=0, Name="file1.txt", ParentBlock=1 (of root), Start=2, Size=300
  Entry 1: Status=1, Type=0, Name="deep.txt", ParentBlock=2 (of test), Start=4, Size=200
  ...
```

**Block 3 (Block Map)**: 
```
[1, EOF, 3, EOF, EOF, 0, 0, ...]
Index 0: value 1 (root.txt: block 0 -> block 1)
Index 1: value EOF (root.txt: end)  
Index 2: value 3 (file1.txt: block 2 -> block 3)
Index 3: value EOF (file1.txt: end)
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
└── test/
    ├── file1.txt (300 bytes)  <- Note: In example Entry 0 of Block 2 belongs to parent 1 (root)
    └── deep.txt (200 bytes)   <- Note: In example Entry 1 of Block 2 belongs to parent 2 (test)
```

## 7. System Limits
- **Max File Size**: ~2GB (u32 size field).
- **Max Disk Size**: 2TB (u32 block addressing).  
- **Files 1KB+**:  Fully supported (automatic block fragmentation).
- **Max Files/Directories**: Configured during formatting.
- **Nesting Depth**: Virtually unlimited.
- **Name Length**: 30 characters.
- **Path Length**: 256 characters.