/**
 * @file efilib.c
 * @brief UEFI library functions implementation
 */

#include "efilib.h"
#include <stdarg.h>

// Global GUIDs
EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID gEfiFileInfoGuid = EFI_FILE_INFO_GUID;

// String functions
CHAR16 *
StrCpy(CHAR16 *dest, const CHAR16 *src)
{
    CHAR16 *p = dest;
    while ((*p++ = *src++) != 0) {
        ;
    }
    return dest;
}

CHAR16 *
StrnCpy(CHAR16 *dest, const CHAR16 *src, UINTN len)
{
    CHAR16 *p = dest;
    UINTN i = 0;
    
    while (i < len && (*p = *src) != 0) {
        p++;
        src++;
        i++;
    }
    
    if (i < len) {
        *p = 0;
    }
    
    return dest;
}

INTN
StrCmp(const CHAR16 *s1, const CHAR16 *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (INTN)(*s1 - *s2);
}

INTN
StrnCmp(const CHAR16 *s1, const CHAR16 *s2, UINTN len)
{
    UINTN i = 0;
    
    while (i < len && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        i++;
    }
    
    if (i == len) {
        return 0;
    }
    
    return (INTN)(*s1 - *s2);
}

UINTN
StrLen(const CHAR16 *s)
{
    UINTN len = 0;
    while (*s++) {
        len++;
    }
    return len;
}

CHAR16 *
StrCat(CHAR16 *dest, const CHAR16 *src)
{
    CHAR16 *p = dest + StrLen(dest);
    while ((*p++ = *src++) != 0) {
        ;
    }
    return dest;
}

// Memory functions
VOID *
MemCpy(VOID *dest, const VOID *src, UINTN len)
{
    UINT8 *d = (UINT8 *)dest;
    const UINT8 *s = (const UINT8 *)src;
    
    while (len--) {
        *d++ = *s++;
    }
    
    return dest;
}

VOID *
MemSet(VOID *dest, INT8 val, UINTN len)
{
    UINT8 *d = (UINT8 *)dest;
    
    while (len--) {
        *d++ = (UINT8)val;
    }
    
    return dest;
}

INTN
MemCmp(const VOID *buf1, const VOID *buf2, UINTN len)
{
    const UINT8 *b1 = (const UINT8 *)buf1;
    const UINT8 *b2 = (const UINT8 *)buf2;
    
    while (len--) {
        if (*b1 != *b2) {
            return (INTN)(*b1 - *b2);
        }
        b1++;
        b2++;
    }
    
    return 0;
}

// Simple print function
EFI_STATUS
Print(CHAR16 *fmt, ...)
{
    va_list args;
    CHAR16 buffer[256];
    
    va_start(args, fmt);
    
    // Simple format handling - just pass through for now
    // In a real implementation, we would format the string
    ST->ConOut->OutputString(ST->ConOut, fmt);
    
    va_end(args);
    
    return EFI_SUCCESS;
}
