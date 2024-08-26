#pragma once
#include "src/fs/fatfs/ffconf.h"
#include <cstdint>

namespace SamplerKit
{

#define FF_INTDEF 2
typedef unsigned int UINT;	/* int must be 16-bit or 32-bit */
typedef unsigned char BYTE; /* char must be 8-bit */
typedef uint16_t WORD;		/* 16-bit unsigned integer */
typedef uint32_t DWORD;		/* 32-bit unsigned integer */
typedef uint64_t QWORD;		/* 64-bit unsigned integer */
typedef WORD WCHAR;			/* UTF-16 character type */

#if FF_FS_EXFAT
#if FF_INTDEF != 2
#error exFAT feature wants C99 or later
#endif
typedef QWORD FSIZE_t;
#if FF_LBA64
typedef QWORD LBA_t;
#else
typedef DWORD LBA_t;
#endif
#else
#if FF_LBA64
#error exFAT needs to be enabled when enable 64-bit LBA
#endif
typedef DWORD FSIZE_t;
typedef DWORD LBA_t;
#endif

#if FF_USE_LFN && FF_LFN_UNICODE == 1 /* Unicode in UTF-16 encoding */
typedef WCHAR TCHAR;
#define _T(x) L##x
#define _TEXT(x) L##x
#elif FF_USE_LFN && FF_LFN_UNICODE == 2 /* Unicode in UTF-8 encoding */
typedef char TCHAR;
#define _T(x) u8##x
#define _TEXT(x) u8##x
#elif FF_USE_LFN && FF_LFN_UNICODE == 3 /* Unicode in UTF-32 encoding */
typedef DWORD TCHAR;
#define _T(x) U##x
#define _TEXT(x) U##x
#elif FF_USE_LFN && (FF_LFN_UNICODE < 0 || FF_LFN_UNICODE > 3)
#error Wrong FF_LFN_UNICODE setting
#else /* ANSI/OEM code in SBCS/DBCS */
typedef char TCHAR;
#define _T(x) x
#define _TEXT(x) x
#endif

typedef enum {
	FR_OK = 0,				/* (0) Succeeded */
	FR_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
	FR_INT_ERR,				/* (2) Assertion failed */
	FR_NOT_READY,			/* (3) The physical drive cannot work */
	FR_NO_FILE,				/* (4) Could not find the file */
	FR_NO_PATH,				/* (5) Could not find the path */
	FR_INVALID_NAME,		/* (6) The path name format is invalid */
	FR_DENIED,				/* (7) Access denied due to prohibited access or directory full */
	FR_EXIST,				/* (8) Access denied due to prohibited access */
	FR_INVALID_OBJECT,		/* (9) The file/directory object is invalid */
	FR_WRITE_PROTECTED,		/* (10) The physical drive is write protected */
	FR_INVALID_DRIVE,		/* (11) The logical drive number is invalid */
	FR_NOT_ENABLED,			/* (12) The volume has no work area */
	FR_NO_FILESYSTEM,		/* (13) There is no valid FAT volume */
	FR_MKFS_ABORTED,		/* (14) The f_mkfs() aborted due to any problem */
	FR_TIMEOUT,				/* (15) Could not get a grant to access the volume within defined period */
	FR_LOCKED,				/* (16) The operation is rejected according to the file sharing policy */
	FR_NOT_ENOUGH_CORE,		/* (17) LFN working buffer could not be allocated */
	FR_TOO_MANY_OPEN_FILES, /* (18) Number of open files > FF_FS_LOCK */
	FR_INVALID_PARAMETER	/* (19) Given parameter is invalid */
} FRESULT;

struct FATFS;

typedef struct {
	FATFS *fs; /* Pointer to the hosting volume of this object */
	WORD id;   /* Hosting volume mount ID */
	BYTE attr; /* Object attribute */
	BYTE
		stat; /* Object chain status (b1-0: =0:not contiguous, =2:contiguous, =3:fragmented in this session, b2:sub-directory stretched) */
	DWORD sclust;	 /* Object data start cluster (0:no cluster or root directory) */
	FSIZE_t objsize; /* Object size (valid when sclust != 0) */
#if FF_FS_EXFAT
	DWORD n_cont; /* Size of first fragment - 1 (valid when stat == 3) */
	DWORD n_frag; /* Size of last fragment needs to be written to FAT (valid when not zero) */
	DWORD c_scl;  /* Containing directory start cluster (valid when sclust != 0) */
	DWORD c_size; /* b31-b8:Size of containing directory, b7-b0: Chain status (valid when c_scl != 0) */
	DWORD c_ofs;  /* Offset in the containing directory (valid when file object and sclust != 0) */
#endif
#if FF_FS_LOCK
	UINT lockid; /* File lock ID origin from 1 (index of file semaphore table Files[]) */
#endif
} FFOBJID;

typedef struct {
	FFOBJID obj;  /* Object identifier (must be the 1st member to detect invalid object pointer) */
	BYTE flag;	  /* File status flags */
	BYTE err;	  /* Abort flag (error code) */
	FSIZE_t fptr; /* File read/write pointer (Zeroed on file open) */
	DWORD clust;  /* Current cluster of fpter (invalid when fptr is 0) */
	LBA_t sect;	  /* Sector number appearing in buf[] (0:invalid) */
#if !FF_FS_READONLY
	LBA_t dir_sect; /* Sector number containing the directory entry (not used at exFAT) */
	BYTE *dir_ptr;	/* Pointer to the directory entry in the win[] (not used at exFAT) */
#endif
#if FF_USE_FASTSEEK
	DWORD *cltbl; /* Pointer to the cluster link map table (nulled on open, set by application) */
#endif
#if !FF_FS_TINY
	BYTE buf[FF_MAX_SS]; /* File private data read/write window */
#endif
} FIL;

typedef struct {
	FFOBJID obj; /* Object identifier */
	DWORD dptr;	 /* Current read/write offset */
	DWORD clust; /* Current cluster */
	LBA_t sect;	 /* Current sector (0:Read operation has terminated) */
	BYTE *dir;	 /* Pointer to the directory item in the win[] */
	BYTE fn[12]; /* SFN (in/out) {body[8],ext[3],status[1]} */
#if FF_USE_LFN
	DWORD blk_ofs; /* Offset of current entry block being processed (0xFFFFFFFF:Invalid) */
#endif
#if FF_USE_FIND
	const TCHAR *pat; /* Pointer to the name matching pattern */
#endif
} DIR;

typedef struct {
	FSIZE_t fsize; /* File size */
	WORD fdate;	   /* Modified date */
	WORD ftime;	   /* Modified time */
	BYTE fattrib;  /* File attribute */
#if FF_USE_LFN
	TCHAR altname[FF_SFN_BUF + 1]; /* Altenative file name */
	TCHAR fname[FF_LFN_BUF + 1];   /* Primary file name */
#else
	TCHAR fname[12 + 1]; /* File name */
#endif
} FILINFO;

/* File access mode and open method flags (3rd argument of f_open) */
#define FA_READ 0x01
#define FA_WRITE 0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_NEW 0x04
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS 0x10
#define FA_OPEN_APPEND 0x30

/* Fast seek controls (2nd argument of f_lseek) */
#define CREATE_LINKMAP ((FSIZE_t)0 - 1)

/* Filesystem type (FATFS.fs_type) */
#define FS_FAT12 1
#define FS_FAT16 2
#define FS_FAT32 3
#define FS_EXFAT 4

/* File attribute bits for directory entry (FILINFO.fattrib) */
#define AM_RDO 0x01 /* Read only */
#define AM_HID 0x02 /* Hidden */
#define AM_SYS 0x04 /* System */
#define AM_DIR 0x10 /* Directory */
#define AM_ARC 0x20 /* Archive */

} // namespace SamplerKit
