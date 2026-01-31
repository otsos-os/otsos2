#ifndef CHAINFS_H
#define CHAINFS_H

#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define CHAINFS_MAGIC 0xCAFEBABE
#define CHAINFS_BLOCK_SIZE 512
#define CHAINFS_MAX_FILENAME 31
#define CHAINFS_EOF_MARKER 0xFFFFFFFF
#define CHAINFS_FREE_BLOCK 0x00000000
#define CHAINFS_MAX_PATH 256

#define CHAINFS_TYPE_FILE 0
#define CHAINFS_TYPE_DIR 1

typedef struct {
    u32 magic;                    //0xCAFEBABE
    u32 block_count;              
    u32 file_table_block_count;   //File Table (M)
    u32 block_map_block_count;    //Block Map (N)
    u32 total_files;              
    u32 root_dir_block;           // Root directory entry block
    u8 padding[488];             
} __attribute__((packed)) chainfs_superblock_t;

typedef struct {
    u8 status;                    // 0 = free, 1 = exists
    u8 type;                      // 0 = file, 1 = directory
    char name[30];                // Reduced by 1 to make room for type
    u32 size;                     
    u32 start_block;              
    u32 parent_block;             // Parent directory entry block (0 for root)
    u8 reserved[16];              // Reduced reserved space
} __attribute__((packed)) chainfs_file_entry_t;

typedef struct {
    chainfs_superblock_t superblock;
    u32 data_area_start;          
    u32 current_dir_block;        // Current directory for relative paths
    u8 sector_buffer[CHAINFS_BLOCK_SIZE];  
} chainfs_t;

extern chainfs_t g_chainfs;

int chainfs_init(void);
int chainfs_format(u32 total_blocks, u32 max_files);
int chainfs_read_file(const char* filename, u8* buffer, u32 buffer_size, u32* bytes_read);
int chainfs_write_file(const char* filename, const u8* data, u32 size);
int chainfs_delete_file(const char* filename);
int chainfs_get_file_list(chainfs_file_entry_t* files, u32 max_files, u32* file_count);

// Directory operations
int chainfs_mkdir(const char* path);
int chainfs_rmdir(const char* path);
int chainfs_chdir(const char* path);
int chainfs_list_dir(const char* path, chainfs_file_entry_t* files, u32 max_files, u32* file_count);
char* chainfs_get_current_path(char* buffer, u32 buffer_size);

// Path resolution
int chainfs_resolve_path(const char* path, chainfs_file_entry_t* entry, u32* entry_block, u32* entry_offset);
int chainfs_find_in_directory(u32 dir_block, const char* name, chainfs_file_entry_t* entry, u32* entry_block, u32* entry_offset);

int chainfs_find_file(const char* filename, chainfs_file_entry_t* entry, u32* entry_block, u32* entry_offset);
int chainfs_find_free_file_entry(u32* entry_block, u32* entry_offset);
int chainfs_find_free_blocks(u32 count, u32* blocks);
int chainfs_read_block_map_entry(u32 block_index, u32* next_block);
int chainfs_write_block_map_entry(u32 block_index, u32 next_block);
void chainfs_free_block_chain(u32 start_block);

#endif