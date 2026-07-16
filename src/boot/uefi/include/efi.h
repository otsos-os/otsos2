/**
 * @file efi.h
 * @brief Basic UEFI definitions
 *
 * This is a minimal subset of UEFI definitions needed for the bootloader.
 * For a complete implementation, use the EDK2 headers.
 */

#ifndef _EFI_H_
#define _EFI_H_

#include <stdint.h>
#include <stddef.h>

// Basic types
typedef uint8_t BOOLEAN;
typedef uint8_t UINT8;
typedef int8_t INT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
typedef uint32_t UINT32;
typedef int32_t INT32;
typedef uint64_t UINT64;
typedef int64_t INT64;

// Character types
typedef uint16_t CHAR16;
typedef uint8_t CHAR8;

// Status codes
typedef UINTN RETURN_STATUS;
typedef UINTN EFI_STATUS;

#define EFI_SUCCESS 0
#define EFI_LOAD_ERROR 1
#define EFI_INVALID_PARAMETER 2
#define EFI_UNSUPPORTED 3
#define EFI_BAD_BUFFER_SIZE 4
#define EFI_BUFFER_TOO_SMALL 5
#define EFI_NOT_READY 6
#define EFI_DEVICE_ERROR 7
#define EFI_WRITE_PROTECTED 8
#define EFI_OUT_OF_RESOURCES 9
#define EFI_NOT_FOUND 14

// Memory types
typedef UINTN EFI_PHYSICAL_ADDRESS;
typedef UINTN EFI_VIRTUAL_ADDRESS;

// Memory types for AllocatePages
define EfiReservedMemoryType 0
#define EfiLoaderCode 1
#define EfiLoaderData 2
#define EfiBootServicesCode 3
#define EfiBootServicesData 4
#define EfiRuntimeServicesCode 5
#define EfiRuntimeServicesData 6
#define EfiConventionalMemory 7
#define EfiUnusableMemory 8
#define EfiACPIReclaimMemory 9
#define EfiACPIMemoryNVS 10
#define EfiMemoryMappedIO 11
#define EfiMemoryMappedIOPortSpace 12
#define EfiPalCode 13
#define EfiPersistentMemory 14

// Memory allocation types
define AllocateAnyPages 0
#define AllocateMaxAddress 1
#define AllocateAddress 2

// Size macros
#define EFI_SIZE_TO_PAGES(size) (((size) + 0xFFF) >> 12)

// GUID definition
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8 Data4[8];
} EFI_GUID;

// Table header
typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

// Memory descriptor
typedef struct {
    UINT32 Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

// File info structure
#define EFI_FILE_INFO_GUID \
    {0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

typedef struct {
    UINT64 Size;
    UINT64 FileSize;
    UINT64 PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64 Attribute;
    CHAR16 FileName[1];
} EFI_FILE_INFO;

// File protocol
#define EFI_FILE_PROTOCOL_GUID \
    {0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x09, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_OPEN)(
    IN EFI_FILE_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL **NewHandle,
    IN CHAR16 *FileName,
    IN UINT64 OpenMode,
    IN UINT64 Attributes
);

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_READ)(
    IN EFI_FILE_PROTOCOL *This,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
);

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_GET_INFO)(
    IN EFI_FILE_PROTOCOL *This,
    IN EFI_GUID *InformationType,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
);

typedef struct _EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_FILE_OPEN Open;
    EFI_FILE_READ Read;
    EFI_FILE_GET_INFO GetInfo;
    // ... other functions
} EFI_FILE_PROTOCOL;

// Simple file system protocol
#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    {0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x09, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

typedef struct {
    UINT64 Revision;
    EFI_FILE_PROTOCOL *OpenVolume;
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

// Loaded image protocol
#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    {0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x4F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

typedef struct {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    EFI_FILE_PATH_PROTOCOL *FilePath;
    VOID *Reserved;
    UINT32 LoadOptionsSize;
    VOID *LoadOptions;
    VOID *ImageBase;
    UINT64 ImageSize;
    UINT32 ImageCodeType;
    UINT32 ImageDataType;
    EFI_TIME UnloadTime;
} EFI_LOADED_IMAGE_PROTOCOL;

// Boot services table
#define EFI_BOOT_SERVICES_SIGNATURE 0x56524553544F4F42
#define EFI_BOOT_SERVICES_REVISION EFI_SPECIFICATION_VERSION

typedef struct {
    EFI_TABLE_HEADER Hdr;
    
    // Task Priority Services
    VOID *RaiseTPL;
    VOID *RestoreTPL;
    
    // Memory Services
    EFI_STATUS (EFIAPI *AllocatePages)(
        IN EFI_ALLOCATE_TYPE Type,
        IN EFI_MEMORY_TYPE MemoryType,
        IN UINTN Pages,
        IN OUT EFI_PHYSICAL_ADDRESS *Memory
    );
    EFI_STATUS (EFIAPI *FreePages)(
        IN EFI_PHYSICAL_ADDRESS Memory,
        IN UINTN Pages
    );
    EFI_STATUS (EFIAPI *GetMemoryMap)(
        IN OUT UINTN *MemoryMapSize,
        IN OUT EFI_MEMORY_DESCRIPTOR *MemoryMap,
        OUT UINTN *MapKey,
        OUT UINTN *DescriptorSize,
        OUT UINT32 *DescriptorVersion
    );
    EFI_STATUS (EFIAPI *ExitBootServices)(
        IN EFI_HANDLE ImageHandle,
        IN UINTN MapKey
    );
    
    // Other services...
    EFI_STATUS (EFIAPI *GetHandleForImage)(
        IN EFI_HANDLE ImageHandle
    );
    EFI_STATUS (EFIAPI *HandleProtocol)(
        IN EFI_HANDLE Handle,
        IN EFI_GUID *Protocol,
        OUT VOID **Interface
    );
    
    // ... more functions
} EFI_BOOT_SERVICES;

// Runtime services table
#define EFI_RUNTIME_SERVICES_SIGNATURE 0x56524E4D49544E55
#define EFI_RUNTIME_SERVICES_REVISION EFI_SPECIFICATION_VERSION

typedef struct {
    EFI_TABLE_HEADER Hdr;
    
    // Time Services
    VOID *GetTime;
    VOID *SetTime;
    
    // Memory Services
    EFI_STATUS (EFIAPI *GetMemoryMap)(
        IN OUT UINTN *MemoryMapSize,
        IN OUT EFI_MEMORY_DESCRIPTOR *MemoryMap,
        OUT UINTN *MapKey,
        OUT UINTN *DescriptorSize,
        OUT UINT32 *DescriptorVersion
    );
    
    // ... more functions
} EFI_RUNTIME_SERVICES;

// System table
#define EFI_SYSTEM_TABLE_SIGNATURE 0x5453595320494249
#define EFI_SPECIFICATION_VERSION 0x00020000

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;

// Simple text output protocol
#define EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID \
    {0x387477c1, 0x69c7, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

typedef struct {
    VOID *Reset;
    VOID *OutputString;
    VOID *TestString;
    VOID *QueryMode;
    VOID *SetMode;
    VOID *SetAttribute;
    VOID *ClearScreen;
    VOID *SetCursorPosition;
    VOID *EnableCursor;
    VOID *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

// Simple text input protocol
#define EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID \
    {0x387477c2, 0x69c7, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

typedef struct {
    VOID *Reset;
    VOID *ReadKeyStroke;
    EFI_EVENT WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

// File path protocol
#define EFI_FILE_PATH_PROTOCOL_GUID \
    {0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x09, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

typedef struct {
    UINT8 Type;
    UINT8 SubType;
    UINT8 Length[2];
} EFI_FILE_PATH_PROTOCOL;

// Graphics Output Protocol
#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    {0xDEA0E6B1, 0xAC2D, 0x4A0A, {0x95, 0x1C, 0x8D, 0x16, 0x7C, 0x21, 0x00, 0x47}}

// Pixel formats
#define EFI_PIXEL_RED_GREEN_BLUE_RESERVED_8BIT_PER_COLOR 0
#define EFI_PIXEL_BLUE_GREEN_RED_RESERVED_8BIT_PER_COLOR 1

typedef struct {
    UINT8 RedMask;
    UINT8 GreenMask;
    UINT8 BlueMask;
    UINT8 ReservedMask;
} EFI_GRAPHICS_PIXEL_BITMASK;

typedef union {
    UINT32 PixelFormat;
    EFI_GRAPHICS_PIXEL_BITMASK BitMask;
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    UINT32 PixelFormat;
    EFI_GRAPHICS_PIXEL_FORMAT PixelInformation;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    VOID *QueryMode;
    VOID *SetMode;
    VOID *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

// ACPI 2.0 Table Protocol
typedef struct {
    VOID *GetAcpiTable;
} EFI_ACPI_20_TABLE_PROTOCOL;

// ACPI RSDP structure
typedef struct {
    UINT64 Signature;
    UINT8 Checksum;
    UINT8 OemId[6];
    UINT8 Revision;
    UINT32 RsdtAddress;
    UINT32 Length;
    UINT64 XsdtAddress;
    UINT8 ExtendedChecksum;
    UINT8 Reserved[3];
} EFI_ACPI_20_RSDP;

// Time structure
typedef struct {
    UINT16 Year;
    UINT8 Month;
    UINT8 Day;
    UINT8 Hour;
    UINT8 Minute;
    UINT8 Second;
    UINT8 Pad1;
    UINT32 Nanosecond;
    INT16 TimeZone;
    UINT8 Daylight;
    UINT8 Pad2;
} EFI_TIME;

// Configuration table
typedef struct {
    EFI_GUID VendorGuid;
    VOID *VendorTable;
} EFI_CONFIGURATION_TABLE;

// Graphics Output Protocol GUID
#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    {0xDEA0E6B1, 0xAC2D, 0x4A0A, {0x95, 0x1C, 0x8D, 0x16, 0x7C, 0x21, 0x00, 0x47}}

// ACPI 2.0 Table Protocol GUID
#define EFI_ACPI_20_TABLE_PROTOCOL_GUID \
    {0x88058032, 0x9207, 0x49C0, {0xA0, 0x64, 0x82, 0x6D, 0x4B, 0x95, 0x24, 0x98}}

// ACPI Table GUID
#define EFI_ACPI_TABLE_GUID \
    {0xEB9D2D30, 0x2D88, 0x11D3, {0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}}

// Function calling convention
#define EFIAPI __attribute__((ms_abi))

// Handle type
typedef VOID *EFI_HANDLE;

// Size type
typedef UINTN UINTN;

// ACPI signatures
#define ACPI_20_RSDP_SIGNATURE 0x2052545020445352  // "RSD PTR "

#endif /* _EFI_H_ */
