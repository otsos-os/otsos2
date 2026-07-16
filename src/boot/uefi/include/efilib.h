/**
 * @file efilib.h
 * @brief UEFI library functions and macros
 */

#ifndef _EFILIB_H_
#define _EFILIB_H_

#include "efi.h"

// String functions
CHAR16 *StrCpy(CHAR16 *dest, const CHAR16 *src);
CHAR16 *StrnCpy(CHAR16 *dest, const CHAR16 *src, UINTN len);
INTN StrCmp(const CHAR16 *s1, const CHAR16 *s2);
INTN StrnCmp(const CHAR16 *s1, const CHAR16 *s2, UINTN len);
UINTN StrLen(const CHAR16 *s);
CHAR16 *StrCat(CHAR16 *dest, const CHAR16 *src);

// Memory functions
VOID *MemCpy(VOID *dest, const VOID *src, UINTN len);
VOID *MemSet(VOID *dest, INT8 val, UINTN len);
INTN MemCmp(const VOID *buf1, const VOID *buf2, UINTN len);

// Print functions
EFI_STATUS Print(CHAR16 *fmt, ...);

// GUID definitions
extern EFI_GUID gEfiLoadedImageProtocolGuid;
extern EFI_GUID gEfiSimpleFileSystemProtocolGuid;
extern EFI_GUID gEfiFileInfoGuid;

// Macro to convert ASCII to Unicode
#define L(x) u##x

// Macro to get current image handle
#define GetImageHandle() (BS->GetHandleForImage(BS->GetCurrentImage()))

#endif /* _EFILIB_H_ */
