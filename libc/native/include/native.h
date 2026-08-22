/* !DEFINES!

$define %type api_sysinfo as struct with kernel identity strings
$define %type api_kmeminfo as struct with native kernel memory data
$define %type api_fs_stat as struct with native file metadata
$define %type api_net_addr as native IPv4 endpoint address
$define %type api_net_iface as native interface snapshot
$define %type api_net_msg as native network message descriptor
$define %type api_reg_value as native registry value IO descriptor
$define %type api_reg_entry as native registry enumeration entry
$define %type api_kofo_info as native KOFO module metadata
$define %type api_trace_probe as native trace probe metadata
$define %type api_trace_program as native trace program descriptor
$define %type api_trace_record as native trace record
$define %type api_term_mouse as struct with console mouse state args
$define %type api_shminfo_args as native shared memory metadata
$define %type kevent as struct with native event data
$define %func powerState as function with args uint32_t
$define %func termWrite as function with args const void *, size_t
$define %func termMouse as function with args api_term_mouse *
$define %func dataOpen as function with args const char *, int
$define %func dataDir as function with args uint32_t, const char *, const char *
$define %func fsMnt as function with args mount tuple
$define %func fsUmnt as function with args const char *, uint64_t
$define %func ptyOpen as function with args int *, int *
$define %func procSpawn as function with args spawn tuple
$define %func procSpawnAbi as function with args spawn tuple, uint32_t
$define %func procSpawnPty as function with args const char *, char *const [], char *const [], int, uint32_t
$define %func netListen as function with args int, int
$define %func netAccept as function with args int, api_net_addr *, uint32_t
$define %func ipcCreate as function with args const char *, uint32_t, uint32_t
$define %func ipcCall as function with args int, api_ipc_call *
$define %func shmGet as function with args uint64_t, size_t, uint32_t, int *
$define %func shmMap as function with args int, void *, size_t, uint32_t, uint32_t
$define %func shmCtl as function with args int, int, void *
$define %func shmClose as function with args int
$define %func regOpen as function with args const char *, const char *, uint32_t
$define %func regGet as function with args int, api_reg_value *
$define %func regUpd as function with args uint32_t
$define %func kofoLoad as function with args const char *, uint32_t
$define %func traceOpen as function with args uint32_t
$define %func traceRead as function with args int, api_trace_read *
$define %func traceLoad as function with args int, api_trace_load *
$define %func traceReadAggs as function with args int, api_trace_aggs *
$define %func traceMark as function with args uint32_t, five uint64_t
$define %func entityCreateEx as function with args archetype, flags, access, name
$define %func entityCreate as function with args archetype, access, name
$define %func entityOpen as function with args const char *, uint32_t
$define %func entityClose as function with args int
$define %func entityDup as function with args int, uint32_t
$define %func entityStat as function with args int, api_entity_stat *
$define %func entityList as function with args path, entries, max
$define %func entityQuery as function with args archetype, start, entries, max
$define %func entityCtl as function with args int, uint32_t, void *
$define %func entityGetData as function with args int, uint32_t, uint64_t *
$define %func entitySetData as function with args int, uint32_t, uint64_t
$define %func entityGetI32 as function with args int, uint32_t, int32_t *
$define %func entitySetI32 as function with args int, uint32_t, int32_t
$define %func entityBind as function with args int, const char *
$define %func entityUnbind as function with args int
$define %func entityDelete as function with args int
$define %func entityRead as function with args int, void *, size_t
$define %func entityWrite as function with args int, const void *, size_t
$define %func entitySeek as function with args int, long, int
$define %func entityIoctl as function with args int, uint64_t, void *

*/

/* !SPACE!

$space %export termRead, termReadFlags, termWrite, termPrint, termMouse
$space %export ptyOpen
$space %export dataOpen, dataClose, dataRead, dataWrite, dataReadFull
$space %export dataWriteFull, dataSeek, dataPipe, dataDir
$space %export fsChdir, fsGetcwd, fsListdir, fsStat, fsRename, fsUnlink
$space %export fsMnt, fsUmnt
$space %export procSpawn, procSpawnAbi, procSpawnNative, procSpawnPty, procWait
$space %export procRun, procExit, procKill
$space %export memMap, memUnmap, eventKqueue, eventWait, eventClose
$space %export powerState
$space %export netOpen, netBind, netConnect, netListen, netAccept
$space %export netSend, netRecv, netCtl
$space %export ipcCreate, ipcConnect, ipcSend, ipcRecv, ipcCall, ipcCtl
$space %export shmGet, shmMap, shmCtl, shmClose
$space %export regOpen, regClose, regGet, regSet, regCreateKey
$space %export regDeleteKey, regDeleteValue, regEnum, regUpd
$space %export regGetBool, regSetBool, regGetU32, regSetU32
$space %export regGetIpv4, regSetIpv4, regGetString, regSetString
$space %export drmCall, drmInfo, drmGemCreate, drmGemClose, drmGemMapInfo
$space %export drmGemMmap, drmFbCreate, drmFbDestroy, drmGetObjects
$space %export drmAtomicCommit, drmRapiClear, drmRapiPutPixel
$space %export drmRapiFillRect, drmRapiGlyph, drmRapiScroll, drmRapiBlit
$space %export drmDriverList, drmDriverSwitch
$space %export traceOpen, traceClose, traceRead, traceCtl, traceInfo
$space %export traceLoad, traceReadAggs, traceMark
$space %export kofoLoad, kofoInfo, kofoUnload
$space %export entityCreateEx, entityCreate, entityOpen, entityClose
$space %export entityDup, entityStat, entityList, entityQuery, entityCtl
$space %export entityGetData, entitySetData, entityGetI32, entitySetI32
$space %export entityBind, entityUnbind, entityDelete
$space %export entityRead, entityWrite, entitySeek, entityIoctl

*/

#ifndef _NATIVE_H
#define _NATIVE_H

#include <stddef.h>
#include <stdint.h>

#define CALL_TERM_READ		0x100
#define CALL_TERM_WRITE		0x101
#define CALL_TERM_INFO		0x102
#define CALL_TERM_MODE		0x103
#define CALL_TERM_POWER		0x111
#define CALL_TERM_MOUSE		0x112
#define TERM_READ_IGNORE_SIGINT	0x00000001
#define TERM_READ_NO_ECHO	0x00000002

#define API_TERM_POWER_GET	0
#define API_TERM_POWER_CHANGE	1
#define API_TERM_POWER_RESET	2
#define API_TERM_MOUSE_UPDATE	0
#define API_TERM_MOUSE_VISIBLE	0x00000001

#define API_TERM_ACTIVE		(-1)
#define API_TERM_MODE_GET	0
#define API_TERM_MODE_SET	1
#define API_TERM_NCCS		32

#define API_TERM_IFLAG_IGNBRK	0000001
#define API_TERM_IFLAG_BRKINT	0000002
#define API_TERM_IFLAG_IGNPAR	0000004
#define API_TERM_IFLAG_PARMRK	0000010
#define API_TERM_IFLAG_INPCK	0000020
#define API_TERM_IFLAG_ISTRIP	0000040
#define API_TERM_IFLAG_INLCR	0000100
#define API_TERM_IFLAG_IGNCR	0000200
#define API_TERM_IFLAG_ICRNL	0000400
#define API_TERM_IFLAG_IXON	0001000
#define API_TERM_IFLAG_IXOFF	0002000
#define API_TERM_IFLAG_IXANY	0004000
#define API_TERM_IFLAG_IMAXBEL	0010000
#define API_TERM_IFLAG_IUTF8	0040000

#define API_TERM_OFLAG_OPOST	0000001
#define API_TERM_OFLAG_OLCUC	0000002
#define API_TERM_OFLAG_ONLCR	0000004
#define API_TERM_OFLAG_OCRNL	0000010
#define API_TERM_OFLAG_ONOCR	0000020
#define API_TERM_OFLAG_ONLRET	0000040
#define API_TERM_OFLAG_OFILL	0000100
#define API_TERM_OFLAG_OFDEL	0000200

#define API_TERM_CFLAG_CBAUD	0010017
#define API_TERM_CFLAG_CSIZE	0000060
#define API_TERM_CFLAG_CS5	0000000
#define API_TERM_CFLAG_CS6	0000020
#define API_TERM_CFLAG_CS7	0000040
#define API_TERM_CFLAG_CS8	0000060
#define API_TERM_CFLAG_CSTOPB	0000100
#define API_TERM_CFLAG_CREAD	0000200
#define API_TERM_CFLAG_PARENB	0000400
#define API_TERM_CFLAG_PARODD	0001000
#define API_TERM_CFLAG_HUPCL	0002000
#define API_TERM_CFLAG_CLOCAL	0004000

#define API_TERM_LFLAG_ISIG	0000001
#define API_TERM_LFLAG_ICANON	0000002
#define API_TERM_LFLAG_XCASE	0000004
#define API_TERM_LFLAG_ECHO	0000010
#define API_TERM_LFLAG_ECHOE	0000020
#define API_TERM_LFLAG_ECHOK	0000040
#define API_TERM_LFLAG_ECHONL	0000100
#define API_TERM_LFLAG_NOFLSH	0000200
#define API_TERM_LFLAG_TOSTOP	0000400
#define API_TERM_LFLAG_ECHOCTL	0001000
#define API_TERM_LFLAG_ECHOPRT	0002000
#define API_TERM_LFLAG_ECHOKE	0004000
#define API_TERM_LFLAG_FLUSHO	0010000
#define API_TERM_LFLAG_PENDIN	0040000
#define API_TERM_LFLAG_IEXTEN	0100000

#define API_TERM_CC_VINTR	0
#define API_TERM_CC_VQUIT	1
#define API_TERM_CC_VERASE	2
#define API_TERM_CC_VKILL	3
#define API_TERM_CC_VEOF	4
#define API_TERM_CC_VTIME	5
#define API_TERM_CC_VMIN	6
#define API_TERM_CC_VSTART	8
#define API_TERM_CC_VSTOP	9
#define API_TERM_CC_VSUSP	10
#define API_TERM_CC_VEOL	11
#define API_TERM_CC_VREPRINT	12
#define API_TERM_CC_VDISCARD	13
#define API_TERM_CC_VWERASE	14
#define API_TERM_CC_VLNEXT	15
#define API_TERM_CC_VEOL2	16

#define API_TERM_SPEED_B38400	0000015

#define API_TIOCGWINSZ		0x5413
#define API_TIOCSWINSZ		0x5414
#define API_TIOCGPGRP		0x540F
#define API_TIOCSPGRP		0x5410
#define API_TIOCGSID		0x5429
#define API_TIOCSCTTY		0x540E
#define API_TIOCGPTN		0x80045430
#define API_TCGETS		0x5401
#define API_TCSETS		0x5402
#define API_TCSETSW		0x5403
#define API_TCSETSF		0x5404
#define API_TCFLSH		0x540B
#define API_FIONREAD		0x541B

struct api_winsize {
	uint16_t	ws_row;
	uint16_t	ws_col;
	uint16_t	ws_xpixel;
	uint16_t	ws_ypixel;
};

#define TERM_STATE_ACTIVE	0
#define TERM_STATE_SUSPENDED	1
#define TERM_STATE_DISABLED	2
#define CALL_INPUT_READ		0x120
#define CALL_INPUT_POLL		0x121
#define CALL_INPUT_FLUSH	0x122
#define CALL_DATA_OPEN		0x200
#define CALL_DATA_CLOSE		0x201
#define CALL_DATA_READ		0x202
#define CALL_DATA_WRITE		0x203
#define CALL_DATA_SEEK		0x204
#define CALL_DATA_PIPE		0x205
#define CALL_DATA_DIR		0x210
#define CALL_FS_CHDIR		0x206
#define CALL_FS_GETCWD		0x207
#define CALL_FS_LISTDIR		0x208
#define CALL_FS_STAT		0x209
#define CALL_FS_RENAME		0x20A
#define CALL_FS_UNLINK		0x20B
#define CALL_FS_LINKNEW		0x20C
#define CALL_FS_LINKGO		0x20D
#define CALL_FS_MNT		0x20E
#define CALL_FS_UMNT		0x20F
#define CALL_MEM_MAP		0x300
#define CALL_MEM_UNMAP		0x301
#define CALL_SHM_GET		0x302
#define CALL_SHM_MAP		0x303
#define CALL_SHM_CTL		0x304
#define CALL_PROC_CLONE		0x400
#define CALL_PROC_COPY		0x401
#define CALL_PROC_SPAWN		0x402
#define CALL_PROC_EXIT		0x403
#define CALL_PROC_WAIT		0x404
#define CALL_PROC_KILL		0x405
#define CALL_PROC_LIST		0x406
#define CALL_KUSR_AUTH		0x407
#define CALL_PROC_GETPID	0x408
#define CALL_PROC_GETPPID	0x409
#define CALL_PROC_THREAD_EXIT	0x40A
#define CALL_PROC_THREAD_JOIN	0x40B
#define CALL_PROC_GETTID	0x40C
#define CALL_PROC_EXIT_GROUP	0x40D
#define CALL_PROC_SET_TID_ADDR	0x40E
#define CALL_FUTEX_WAIT		0x40F
#define CALL_FUTEX_WAKE		0x410
#define CALL_PROC_SETSID	0x411
#define CALL_PROC_GETSID	0x412
#define CALL_PROC_PERM		0x413
#define CALL_SYS_INFO		0x500
#define CALL_SYS_MEMINFO	0x501
#define CALL_SYS_KMEMINFO	0x502
#define CALL_SYS_RANDOM		0x503
#define CALL_SYS_TIMEINFO	0x504
#define CALL_SYS_TIME		0x505
#define CALL_SYS_CPUINFO	0x506
#define CALL_POWER_STATE	0x507
#define CALL_DRM_CALL		0x600
#define CALL_EVENT_KQUEUE	0x700
#define CALL_EVENT_KEVENT	0x701
#define CALL_EVENT_CLOSE	0x702
#define CALL_NET_OPEN		0x800
#define CALL_NET_BIND		0x801
#define CALL_NET_CONNECT	0x802
#define CALL_NET_SEND		0x803
#define CALL_NET_RECV		0x804
#define CALL_NET_CTL		0x805
#define CALL_NET_LISTEN		0x806
#define CALL_NET_ACCEPT		0x807
#define CALL_IPC_CREATE	0xB00
#define CALL_IPC_CONNECT	0xB01
#define CALL_IPC_SEND		0xB02
#define CALL_IPC_RECV		0xB03
#define CALL_IPC_CALL		0xB04
#define CALL_IPC_CTL		0xB05
#define CALL_KOFO_LOAD		0xC00
#define CALL_KOFO_INFO		0xC01
#define CALL_KOFO_UNLOAD	0xC02
#define CALL_ENTITY_CREATE	0xD00
#define CALL_ENTITY_OPEN	0xD01
#define CALL_ENTITY_CLOSE	0xD02
#define CALL_ENTITY_DUP		0xD03
#define CALL_ENTITY_STAT	0xD04
#define CALL_ENTITY_LIST	0xD05
#define CALL_ENTITY_CTL		0xD06
#define CALL_ENTITY_QUERY	0xD07
#define CALL_ENTITY_READ	0xD08
#define CALL_ENTITY_WRITE	0xD09
#define CALL_ENTITY_SEEK	0xD0A
#define CALL_TRACE_OPEN		0x900
#define CALL_TRACE_CLOSE	0x901
#define CALL_TRACE_READ		0x902
#define CALL_TRACE_CTL		0x903
#define CALL_TRACE_INFO		0x904
#define CALL_TRACE_MARK		0x905
#define CALL_REG_OPEN		0xA00
#define CALL_REG_CLOSE		0xA01
#define CALL_REG_GET		0xA02
#define CALL_REG_SET		0xA03
#define CALL_REG_CREATE_KEY	0xA04
#define CALL_REG_DELETE_KEY	0xA05
#define CALL_REG_DELETE_VALUE	0xA06
#define CALL_REG_ENUM		0xA07
#define CALL_REG_UPD		0xA08
#define CALL_REG_ENUM_HIVES	0xA09
#define CALL_PERSONALITY	0xFFFF

#define API_OPEN_READ		0x0001
#define API_OPEN_WRITE		0x0002
#define API_OPEN_RW		(API_OPEN_READ | API_OPEN_WRITE)
#define API_OPEN_CREATE		0x0040
#define API_OPEN_TRUNC		0x0200
#define API_OPEN_APPEND		0x0400
#define API_DATA_DIR_MKDIR	1
#define API_DATA_DIR_RMDIR	2
#define API_DATA_DIR_RENAME	3

#define API_SEEK_SET		0
#define API_SEEK_CUR		1
#define API_SEEK_END		2

#define API_MAP_READ		0x1
#define API_MAP_WRITE		0x2
#define API_MAP_EXEC		0x4
#define API_MAP_SHARED		0x01
#define API_MAP_PRIVATE		0x02
#define API_MAP_FIXED		0x10
#define API_MAP_ANON		0x20
#define API_MAP_GEM		0x40

#define SHM_PRIVATE		0
#define SHM_CREAT		01000
#define SHM_EXCL		02000
#define SHM_RDONLY		010000
#define SHM_CTL_RMID		0
#define SHM_CTL_STAT		2

#define API_CLONE_VM		0x00000100
#define API_CLONE_THREAD	0x00010000
#define API_PROC_PERM_USER	0
#define API_PROC_PERM_KUSR	1

#define API_INPUT_NONBLOCK	0x00000001

#define API_NET_ADDR_IP4	1
#define API_NET_PROTO_UDP	1
#define API_NET_PROTO_TCP	2
#define API_NET_MODE_DGRAM	1
#define API_NET_MODE_STREAM	2
#define API_NET_OPEN_NONBLOCK	0x00000001
#define API_NET_MSG_NONBLOCK	0x00000001
#define API_NET_MSG_TRUNC	0x00000002
#define API_NET_CTL_COMMON_BASE	0x0000
#define API_NET_CTL_PROTO_BASE	0x1000
#define API_NET_CTL_PRIV_BASE	0x8000
#define API_NET_CTL_GET_LOCAL	(API_NET_CTL_COMMON_BASE + 1)
#define API_NET_CTL_GET_PEER	(API_NET_CTL_COMMON_BASE + 2)
#define API_NET_CTL_GET_IFACE	(API_NET_CTL_COMMON_BASE + 3)

#define API_REG_OPEN_READ	API_OPEN_READ
#define API_REG_OPEN_WRITE	API_OPEN_WRITE
#define API_REG_OPEN_RW		API_OPEN_RW
#define API_REG_OPEN_CREATE	API_OPEN_CREATE

#define API_REG_TYPE_STRING		1
#define API_REG_TYPE_BOOL		2
#define API_REG_TYPE_I32		3
#define API_REG_TYPE_U32		4
#define API_REG_TYPE_U64		5
#define API_REG_TYPE_IPV4		6
#define API_REG_TYPE_BYTES		7
#define API_REG_TYPE_MULTI_STRING	8

#define API_REG_KIND_KEY	1
#define API_REG_KIND_VALUE	2

#define API_REG_HIVE_CAN_READ	0x1
#define API_REG_HIVE_CAN_ADD	0x2
#define API_REG_HIVE_CAN_EDIT	0x4

#define API_REG_CONSUMER_NET		1
#define API_REG_CONSUMER_SCHEDULER	2
#define API_REG_CONSUMER_KUSR		3
#define API_REG_CONSUMER_CONSOLE	4
#define API_REG_CONSUMER_INPUT	5

#define API_KOFO_NAME_MAX	32
#define API_KOFO_VERSION_MAX	32
#define API_KOFO_PATH_MAX	128
#define API_KOFO_STATE_EMPTY	0
#define API_KOFO_STATE_LOADING	1
#define API_KOFO_STATE_LOADED	2
#define API_KOFO_STATE_UNLOADING	3

#define API_ENTITY_NAME_MAX		64
#define API_ENTITY_LIST_MAX_ENTRIES	256

#define API_ENTITY_ARCH_GENERIC		1
#define API_ENTITY_ARCH_FILE		2
#define API_ENTITY_ARCH_PIPE		3
#define API_ENTITY_ARCH_VNODE		4
#define API_ENTITY_ARCH_NET		5
#define API_ENTITY_ARCH_IPC		6
#define API_ENTITY_ARCH_REG		7
#define API_ENTITY_ARCH_KQUEUE		8
#define API_ENTITY_ARCH_SHM		9
#define API_ENTITY_ARCH_TRACE		10
#define API_ENTITY_ARCH_GEM		11
#define API_ENTITY_ARCH_KOFO		12
#define API_ENTITY_ARCH_NB_INTERFACE	13
#define API_ENTITY_ARCH_PROCESS		14
#define API_ENTITY_ARCH_THREAD		15
#define API_ENTITY_ARCH_TTY		16
#define API_ENTITY_ARCH_DRM		17
#define API_ENTITY_ARCH_PTY		18
#define API_ENTITY_ARCH_NB_DEVICE	19

#define ENTITY_ACCESS_READ		0x00000001
#define ENTITY_ACCESS_WRITE		0x00000002
#define ENTITY_ACCESS_EXEC		0x00000004
#define ENTITY_ACCESS_ALL		(ENTITY_ACCESS_READ | \
					    ENTITY_ACCESS_WRITE | \
					    ENTITY_ACCESS_EXEC)
#define ENTITY_ACCESS_DEFAULT		(ENTITY_ACCESS_READ | \
					    ENTITY_ACCESS_WRITE)

#define API_ENTITY_ACCESS_READ		ENTITY_ACCESS_READ
#define API_ENTITY_ACCESS_WRITE		ENTITY_ACCESS_WRITE
#define API_ENTITY_ACCESS_EXEC		ENTITY_ACCESS_EXEC
#define API_ENTITY_ACCESS_ALL		ENTITY_ACCESS_ALL
#define API_ENTITY_ACCESS_DEFAULT	ENTITY_ACCESS_DEFAULT

#define ENTITY_CTL_GET_INFO		1
#define ENTITY_CTL_GET_DATA		2
#define ENTITY_CTL_SET_DATA		3
#define ENTITY_CTL_GET_I32		4
#define ENTITY_CTL_SET_I32		5
#define ENTITY_CTL_BIND			6
#define ENTITY_CTL_UNBIND		7
#define ENTITY_CTL_DELETE		8
#define ENTITY_CTL_IOCTL		9

struct api_entity_create_args {
	uint16_t	archetype;
	uint16_t	flags;
	uint32_t	access;
	const char	*name;
};

struct api_entity_stat {
	uint64_t	id;
	uint16_t	archetype;
	uint16_t	state;
	uint32_t	flags;
	int32_t		refs;
	uint32_t	owner_pid;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	euid;
	uint32_t	egid;
	uint64_t	size;
	uint64_t	created;
	char		name[API_ENTITY_NAME_MAX];
};

struct api_entity_entry {
	uint64_t	id;
	uint16_t	archetype;
	uint16_t	state;
	uint32_t	owner_pid;
	char		name[API_ENTITY_NAME_MAX];
};

struct api_entity_data {
	uint32_t	index;
	uint32_t	pad;
	uint64_t	value;
};

struct api_entity_list {
	const char		*path;
	struct api_entity_entry	*entries;
	uint32_t		max_entries;
	uint32_t		count;
};

struct api_entity_query {
	uint16_t		archetype;
	uint16_t		pad;
	uint32_t		start;
	struct api_entity_entry	*entries;
	uint32_t		max_entries;
	uint32_t		count;
};

struct api_entity_ioctl {
	uint64_t		cmd;
	uint64_t		arg;
};

#define API_TRACE_MAX_CPUS		32
#define API_TRACE_MAX_PROVIDERS		16
#define API_TRACE_MAX_PROBES		256
#define API_TRACE_MAX_ARGS		8
#define API_TRACE_MAX_PREDICATES	8
#define API_TRACE_MAX_ACTIONS		16
#define API_TRACE_MAX_PROGRAMS		128
#define API_TRACE_MAX_AGGREGATIONS	256
#define API_TRACE_RECORD_STACK		8
#define API_TRACE_NAME_LEN		32
#define API_TRACE_MAX_PMU_COUNTERS	16
#define API_TRACE_READ_MAX_RECORDS	4096

#define API_TRACE_OPEN_PRIVILEGED	0x00000001
#define API_TRACE_OPEN_KERNEL_STACK	0x00000002

#define API_TRACE_CLEAR_RECORDS		0x00000001
#define API_TRACE_CLEAR_PROGRAMS	0x00000002
#define API_TRACE_CLEAR_AGGS		0x00000004
#define API_TRACE_CLEAR_ALL \
	(API_TRACE_CLEAR_RECORDS | API_TRACE_CLEAR_PROGRAMS | \
	API_TRACE_CLEAR_AGGS)

#define API_TRACE_REC_F_USER		0x00000001
#define API_TRACE_REC_F_KERNEL_STACK	0x00000002
#define API_TRACE_REC_F_PMU_VALID	0x00000004
#define API_TRACE_REC_F_DROPPED_BEFORE	0x00000008

#define API_TRACE_ARG_U64		1
#define API_TRACE_ARG_S64		2
#define API_TRACE_ARG_PID		3
#define API_TRACE_ARG_TID		4
#define API_TRACE_ARG_CPU		5
#define API_TRACE_ARG_ID		6
#define API_TRACE_ARG_PTR		7
#define API_TRACE_ARG_CYCLES		8
#define API_TRACE_ARG_ERRNO		9
#define API_TRACE_ARG_FLAGS		10
#define API_TRACE_ARG_BYTES		11

#define API_TRACE_FIELD_NONE		0
#define API_TRACE_FIELD_PID		1
#define API_TRACE_FIELD_TID		2
#define API_TRACE_FIELD_CPU		3
#define API_TRACE_FIELD_PROBE		4
#define API_TRACE_FIELD_ARG0		16
#define API_TRACE_FIELD_ARG1		17
#define API_TRACE_FIELD_ARG2		18
#define API_TRACE_FIELD_ARG3		19
#define API_TRACE_FIELD_ARG4		20
#define API_TRACE_FIELD_ARG5		21
#define API_TRACE_FIELD_ARG6		22
#define API_TRACE_FIELD_ARG7		23

#define API_TRACE_PRED_EQ		1
#define API_TRACE_PRED_NE		2
#define API_TRACE_PRED_LT		3
#define API_TRACE_PRED_LE		4
#define API_TRACE_PRED_GT		5
#define API_TRACE_PRED_GE		6
#define API_TRACE_PRED_MASK		7

#define API_TRACE_ACT_RECORD		1
#define API_TRACE_ACT_STACK		2
#define API_TRACE_ACT_COUNT		3
#define API_TRACE_ACT_SUM		4
#define API_TRACE_ACT_MIN		5
#define API_TRACE_ACT_MAX		6
#define API_TRACE_ACT_QUANTIZE		7
#define API_TRACE_ACT_LQUANTIZE		8

#define API_TRACE_OP_START		1
#define API_TRACE_OP_STOP		2
#define API_TRACE_OP_LOAD		3
#define API_TRACE_OP_CLEAR		4

#define API_TRACE_INFO_STATS		1
#define API_TRACE_INFO_PROVIDERS	2
#define API_TRACE_INFO_PROBES		3
#define API_TRACE_INFO_PMU		4
#define API_TRACE_INFO_AGGS		5

#define API_TRACE_PMU_CYCLES		0
#define API_TRACE_PMU_INSTRUCTIONS	1
#define API_TRACE_PMU_CACHE_REFERENCES	2
#define API_TRACE_PMU_CACHE_MISSES	3
#define API_TRACE_PMU_BRANCH_INSTRUCTIONS	4
#define API_TRACE_PMU_BRANCH_MISSES	5
#define API_TRACE_PMU_COUNTER_COUNT	6

#define API_FS_TYPE_REG		1
#define API_FS_TYPE_DIR		2
#define API_FS_TYPE_CHR		3
#define API_FS_TYPE_PIPE	4
#define API_FS_TYPE_LNK		5

#define API_MS_RDONLY		0x00000001ULL
#define API_MS_NOSUID		0x00000002ULL
#define API_MS_NODEV		0x00000004ULL
#define API_MS_NOEXEC		0x00000008ULL
#define API_MS_SYNCHRONOUS	0x00000010ULL
#define API_MS_MANDLOCK	0x00000040ULL
#define API_MS_DIRSYNC		0x00000080ULL
#define API_MS_NOATIME		0x00000400ULL
#define API_MS_NODIRATIME	0x00000800ULL
#define API_MS_RELATIME	0x00200000ULL
#define API_MS_KUSR_ONLY	0x100000000ULL

#define EVFILT_READ		(-1)
#define EVFILT_WRITE		(-2)
#define EVFILT_TIMER		(-3)
#define EVFILT_PROC		(-4)
#define EVFILT_SIGNAL		(-5)
#define EVFILT_USER		(-6)
#define EVFILT_KBD		(-7)
#define EVFILT_IPC		(-9)
#define EVFILT_INPUT		(-10)
#define EVFILT_ENTITY		(-11)
#define EVFILT_POWER		(-12)
#define POWER_EVENT_IDENT_SYSTEM	0

#define EV_ADD			0x0001
#define EV_DELETE		0x0002
#define EV_ENABLE		0x0004
#define EV_DISABLE		0x0008
#define EV_ONESHOT		0x0010
#define EV_CLEAR		0x0020
#define EV_RECEIPT		0x0040
#define EV_ERROR		0x4000
#define EV_EOF			0x8000

#define NOTE_EXIT		0x80000000U
#define NOTE_POWER_BUTTON	0x00000001U

#define API_POWER_STATE_SHUTDOWN	1
#define API_POWER_STATE_REBOOT	2

#define DRM_OP_INFO		1
#define DRM_OP_GEM_CREATE	2
#define DRM_OP_GEM_CLOSE	3
#define DRM_OP_GEM_MAP		4
#define DRM_OP_FB_CREATE	5
#define DRM_OP_FB_DESTROY	6
#define DRM_OP_GET_OBJECTS	7
#define DRM_OP_ATOMIC_COMMIT	8
#define DRM_OP_RAPI_CLEAR	9
#define DRM_OP_RAPI_PUT_PIXEL	10
#define DRM_OP_RAPI_FILL_RECT	11
#define DRM_OP_RAPI_GLYPH	12
#define DRM_OP_RAPI_SCROLL	13
#define DRM_OP_RAPI_BLIT	14
#define DRM_OP_DRIVER_SWITCH	15
#define DRM_OP_DRIVER_LIST	16

#define DRM_PROP_PLANE_FB_ID	1
#define DRM_PROP_PLANE_CRTC_ID	2
#define DRM_PROP_PLANE_SRC_X	3
#define DRM_PROP_PLANE_SRC_Y	4
#define DRM_PROP_PLANE_SRC_W	5
#define DRM_PROP_PLANE_SRC_H	6
#define DRM_PROP_PLANE_CRTC_X	7
#define DRM_PROP_PLANE_CRTC_Y	8
#define DRM_PROP_PLANE_CRTC_W	9
#define DRM_PROP_PLANE_CRTC_H	10
#define DRM_PROP_PLANE_DIRTY_X	11
#define DRM_PROP_PLANE_DIRTY_Y	12
#define DRM_PROP_PLANE_DIRTY_W	13
#define DRM_PROP_PLANE_DIRTY_H	14

#define KBD_DATA_KEY(v)		((uint16_t)((uint64_t)(v) & 0xFFFF))
#define KBD_DATA_RELEASED(v)	(((uint64_t)(v) >> 16) & 1)
#define KBD_DATA_EXTENDED(v)	(((uint64_t)(v) >> 17) & 1)
#define KBD_DATA_ASCII(v)	((char)(((uint64_t)(v) >> 24) & 0xFF))

#define MOUSE_BUTTON_LEFT	0x00000001
#define MOUSE_BUTTON_RIGHT	0x00000002
#define MOUSE_BUTTON_MIDDLE	0x00000004
#define MOUSE_BUTTON_X1		0x00000008
#define MOUSE_BUTTON_X2		0x00000010

#define MOUSE_EVENT_MOVE	0x00000001
#define MOUSE_EVENT_BUTTON	0x00000002
#define MOUSE_EVENT_WHEEL	0x00000004
#define MOUSE_EVENT_OVERFLOW	0x00000008

#define API_INPUT_TYPE_NONE		0
#define API_INPUT_TYPE_KEYBOARD		1
#define API_INPUT_TYPE_MOUSE		2

#define API_INPUT_DEVICE_KEYBOARD0	1
#define API_INPUT_DEVICE_MOUSE0	2

#define API_INPUT_FLAG_DROPPED	0x80000000

struct api_sysinfo {
	char	sysname[65];
	char	nodename[65];
	char	release[65];
	char	version[65];
	char	machine[65];
	char	domainname[65];
};

struct api_meminfo {
	uint64_t	ram_total_kb;
	uint64_t	ram_free_kb;
	uint64_t	pages_total;
	uint64_t	pages_free;
	uint64_t	pages_active;
	uint64_t	pages_inactive;
	uint64_t	pages_cache;
	uint64_t	pages_wired;
	uint64_t	user_heap_base;
	uint64_t	user_heap_size_kb;
	uint64_t	mmap_base;
	uint64_t	mmap_limit;
};
struct api_kmeminfo {
	uint64_t	kmem_heap_total_kb;
	uint64_t	kmem_heap_used_kb;
	uint64_t	kmem_heap_free_kb;
	uint64_t	bootmem_free_kb;
	uint64_t	kmem_heap_addr;
};

struct api_dirent {
	char		name[32];
	uint8_t		type;
	uint8_t		pad[3];
};

struct api_fs_stat {
	uint32_t	type;
	uint32_t	mode;
	uint32_t	uid;
	uint32_t	gid;
	uint64_t	size;
	uint64_t	blocks;
	int64_t		atime;
	int64_t		mtime;
	int64_t		ctime;
	char		name[32];
};

#define API_CPUINFO_MAX_CPUS 32
#define API_CPUINFO_MAX_PIDS 64

struct api_cpu_entry {
	uint32_t	cpu_index;
	uint32_t	lapic_id;
	uint32_t	present;
	uint32_t	online;
	uint32_t	pid;
	uint32_t	tid;
	uint32_t	state;
	uint32_t	pid_count;
	uint32_t	pids[API_CPUINFO_MAX_PIDS];
	char		proc_name[32];
};

struct api_cpuinfo {
	uint32_t	cpu_count;
	uint32_t	entry_count;
	struct api_cpu_entry entries[API_CPUINFO_MAX_CPUS];
};

struct api_proc_info {
	uint32_t	pid;
	uint32_t	ppid;
	char		name[32];
	uint32_t	state;
};

#define API_PROC_SPAWN_ABI_POSIX	0
#define API_PROC_SPAWN_ABI_NATIVE	1
#define API_PROC_SPAWN_SET_TTY		0x00000002
#define API_PERSONALITY_NATIVE		0
#define API_PERSONALITY_POSIX		1

struct api_proc_spawn_args {
	uint32_t		size;
	uint32_t		flags;
	uint32_t		abi;
	int32_t			tty;
	const char		*path;
	const char *const	*argv;
	const char *const	*envp;
};

struct api_timeinfo {
	uint64_t	wall_sec;
	uint64_t	wall_nsec;
	uint64_t	local_sec;
	uint64_t	local_nsec;
	uint64_t	uptime_sec;
	uint64_t	uptime_nsec;
	uint64_t	ticks;
	uint64_t	frequency;
	int64_t		timezone_offset;
	char		clocksource[32];
};

struct api_trace_arg {
	char		name[API_TRACE_NAME_LEN];
	uint32_t	type;
	uint32_t	flags;
};

struct api_trace_provider {
	uint32_t	id;
	uint32_t	enabled;
	uint32_t	probe_count;
	uint32_t	reserved;
	char		name[API_TRACE_NAME_LEN];
};

struct api_trace_providers {
	struct api_trace_provider	*providers;
	uint32_t			max_providers;
	uint32_t			count;
};

struct api_trace_probe {
	uint32_t	id;
	uint32_t	provider;
	uint32_t	enabled;
	uint32_t	argc;
	uint32_t	flags;
	uint32_t	reserved;
	char		provider_name[API_TRACE_NAME_LEN];
	char		module[API_TRACE_NAME_LEN];
	char		function[API_TRACE_NAME_LEN];
	char		name[API_TRACE_NAME_LEN];
	struct api_trace_arg args[API_TRACE_MAX_ARGS];
};

struct api_trace_probes {
	struct api_trace_probe	*probes;
	uint32_t		max_probes;
	uint32_t		count;
};

struct api_trace_predicate {
	uint32_t	field;
	uint32_t	op;
	uint64_t	value;
};

struct api_trace_action {
	uint32_t	kind;
	uint32_t	arg;
	uint32_t	key;
	uint32_t	id;
	uint64_t	value;
};

struct api_trace_program {
	uint32_t			probe_id;
	uint32_t			flags;
	uint32_t			predicate_count;
	uint32_t			action_count;
	struct api_trace_predicate predicates[API_TRACE_MAX_PREDICATES];
	struct api_trace_action	actions[API_TRACE_MAX_ACTIONS];
};

struct api_trace_load {
	struct api_trace_program	*programs;
	uint32_t		program_count;
	uint32_t		flags;
};

struct api_trace_record {
	uint64_t	seq;
	uint64_t	tsc;
	uint64_t	ticks;
	uint64_t	pid;
	uint64_t	tid;
	uint64_t	ip;
	uint64_t	sp;
	uint64_t	bp;
	uint64_t	probe_id;
	uint64_t	action_id;
	uint64_t	args[API_TRACE_MAX_ARGS];
	uint64_t	stack[API_TRACE_RECORD_STACK];
	uint32_t	cpu;
	uint32_t	flags;
	uint32_t	argc;
	uint32_t	stack_count;
};

struct api_trace_read {
	struct api_trace_record	*records;
	uint32_t		max_records;
	uint32_t		records_read;
	uint64_t		records_total;
	uint64_t		records_lost;
	uint32_t		flags;
	uint32_t		reserved;
};

struct api_trace_agg {
	uint32_t	id;
	uint32_t	kind;
	uint32_t	probe_id;
	uint32_t	arg;
	uint64_t	key[4];
	uint64_t	value;
	uint64_t	count;
};

struct api_trace_aggs {
	struct api_trace_agg	*aggs;
	int			trace;
	uint32_t		max_aggs;
	uint32_t		count;
	uint32_t		clear;
	uint32_t		reserved;
};

struct api_trace_stats {
	uint64_t	records_written;
	uint64_t	records_lost;
	uint64_t	probe_hits[API_TRACE_MAX_PROBES];
	uint64_t	action_hits;
	uint64_t	aggregation_updates;
	uint32_t	provider_count;
	uint32_t	probe_count;
	uint32_t	session_count;
	uint32_t	ring_records;
	uint32_t	enabled;
	uint32_t	initialized;
};

struct api_trace_session_stats {
	uint64_t	records_written;
	uint64_t	records_read;
	uint64_t	records_lost;
	uint64_t	aggregation_count;
	uint32_t	active;
	uint32_t	program_count;
	uint32_t	flags;
	uint32_t	reserved;
};

struct api_trace_pmu_counter {
	uint32_t	id;
	uint32_t	enabled;
	char		name[API_TRACE_NAME_LEN];
};

struct api_trace_pmu {
	struct api_trace_pmu_counter	*counters;
	uint32_t			max_counters;
	uint32_t			count;
	uint32_t			events_enabled;
	uint32_t			reserved;
};

struct api_net_addr {
	uint32_t	family;
	uint32_t	port;
	uint32_t	ip;
	uint32_t	ifindex;
};

struct api_net_iface {
	uint32_t	ifindex;
	uint32_t	flags;
	uint32_t	ip;
	uint32_t	netmask;
	uint32_t	gateway;
	uint32_t	mtu;
	uint8_t		mac[6];
	uint8_t		pad[2];
	char		name[16];
	char		device[16];
};

struct api_net_msg {
	void			*data;
	struct api_net_addr	*addr;
	uint32_t		length;
	uint32_t		flags;
};

struct api_reg_value {
	const char	*name;
	void		*data;
	uint32_t	size;
	uint32_t	type;
	uint32_t	flags;
	uint32_t	bytes;
};

struct api_reg_entry {
	uint32_t	index;
	uint32_t	kind;
	uint32_t	type;
	uint32_t	size;
	char		name[32];
};

struct api_reg_hive {
	uint32_t	index;
	uint32_t	access;
	char		name[32];
};

struct api_kofo_info {
	uint32_t	size;
	uint32_t	id;
	uint32_t	state;
	uint32_t	flags;
	uint64_t	image_base;
	uint64_t	image_size;
	uint32_t	section_count;
	uint32_t	symbol_count;
	uint32_t	import_count;
	uint32_t	reloc_count;
	uint32_t	driver_count;
	uint32_t	pad;
	char		name[API_KOFO_NAME_MAX];
	char		version[API_KOFO_VERSION_MAX];
	char		path[API_KOFO_PATH_MAX];
};

struct api_key_event {
	uint64_t	timestamp;
	uint16_t	key;
	uint16_t	raw;
	uint32_t	flags;
	uint32_t	mods;
	uint32_t	ch;
};

struct api_input_event {
	uint64_t	timestamp;
	uint64_t	seq;
	uint32_t	type;
	uint32_t	device;
	uint32_t	flags;
	uint32_t	lost;
	int32_t		x;
	int32_t		y;
	int32_t		dx;
	int32_t		dy;
	int32_t		dz;
	uint32_t	buttons;
	uint32_t	key;
	uint32_t	raw;
	uint32_t	mods;
	uint32_t	ch;
};

struct api_term_info {
	int	tty;
	int	state;
	uint16_t rows;
	uint16_t cols;
	uint16_t xpixel;
	uint16_t ypixel;
};

struct api_term_power {
	int	op;
	int	tty;
	int	state;
	int	flags;
};

struct api_term_mouse {
	int	op;
	int	tty;
	int	flags;
	int	x;
	int	y;
	int	buttons;
};

struct api_term_mode {
	int	op;
	int	tty;
	uint32_t	iflag;
	uint32_t	oflag;
	uint32_t	cflag;
	uint32_t	lflag;
	uint8_t		cc[API_TERM_NCCS];
	uint32_t	ispeed;
	uint32_t	ospeed;
};

struct mem_map_args {
	uint64_t	addr;
	uint64_t	length;
	uint32_t	prot;
	uint32_t	flags;
	int		fd;
	uint64_t	offset;
} __attribute__((packed));

struct api_shmget_args {
	uint64_t	key;
	uint64_t	size;
	uint32_t	flags;
	uint32_t	id;
};

struct api_shmmap_args {
	uint32_t	id;
	uint32_t	prot;
	uint32_t	flags;
	uint64_t	addr;
	uint64_t	size;
};

struct api_shminfo_args {
	uint32_t	id;
	uint32_t	mode;
	uint32_t	refs;
	uint32_t	removed;
	uint64_t	key;
	uint64_t	size;
};

#define NOTE_IPC_READ	0x00000001
#define NOTE_IPC_WRITE	0x00000002
#define NOTE_IPC_HUP	0x00000004
#define NOTE_IPC_PEER	0x00000008
#define NOTE_IPC_ALL	0x0000000F

struct kevent {
	uint64_t	ident;
	int16_t		filter;
	uint16_t	flags;
	uint32_t	fflags;
	int64_t		data;
	uint64_t	udata;
	struct api_input_event	input;
};

struct api_drm_info {
	uint32_t	available;
	uint32_t	width;
	uint32_t	height;
	uint32_t	pitch;
	uint32_t	bpp;
	char		driver_name[32];
};

struct api_drm_gem_create {
	uint64_t	size;
	uint32_t	handle;
};

struct api_drm_gem_map {
	uint32_t	handle;
	uint64_t	vaddr;
	uint64_t	size;
};

struct api_drm_fb_create {
	uint32_t	gem_handle;
	uint32_t	width;
	uint32_t	height;
	uint32_t	pitch;
	uint8_t		bpp;
	uint32_t	fb_id;
};

struct api_drm_atomic_req {
	uint32_t	obj_id;
	uint32_t	prop_id;
	uint64_t	value;
};

struct api_drm_atomic_commit {
	struct api_drm_atomic_req *reqs;
	uint32_t	count;
	uint32_t	flags;
};

struct api_drm_objects {
	uint32_t	primary_plane_id;
	uint32_t	cursor_plane_id;
	uint32_t	crtc_id;
	uint32_t	connector_id;
};

struct api_drm_rapi_pixel {
	uint32_t	handle;
	uint32_t	pitch;
	uint8_t		bpp;
	uint32_t	x;
	uint32_t	y;
	uint32_t	color;
};

struct api_drm_rapi_rect {
	uint32_t	handle;
	uint32_t	pitch;
	uint8_t		bpp;
	uint32_t	x;
	uint32_t	y;
	uint32_t	width;
	uint32_t	height;
	uint32_t	color;
};

struct api_drm_rapi_glyph {
	uint32_t	handle;
	uint32_t	pitch;
	uint8_t		bpp;
	uint32_t	x;
	uint32_t	y;
	char		c;
	char		_pad[3];
	uint32_t	fg;
	uint32_t	bg;
};

struct api_drm_rapi_scroll {
	uint32_t	handle;
	uint32_t	pitch;
	uint8_t		bpp;
	uint32_t	lines;
	uint32_t	bg;
};

struct api_drm_rapi_blit {
	uint32_t	src_handle;
	uint32_t	src_pitch;
	uint32_t	dst_handle;
	uint32_t	dst_pitch;
	uint8_t		bpp;
	uint32_t	sx;
	uint32_t	sy;
	uint32_t	sw;
	uint32_t	sh;
	uint32_t	dx;
	uint32_t	dy;
};

struct api_drm_driver_entry {
	uint32_t	id;
	char		name[32];
	uint32_t	active;
};


#define IPC_NAME_MAX		48
#define IPC_MAX_PAYLOAD	1024
#define IPC_MAX_HANDLES		8

#define IPC_MSG_REQUEST	0x00000001
#define IPC_MSG_REPLY		0x00000002
#define IPC_MSG_EVENT		0x00000004
#define IPC_MSG_NONBLOCK	0x00000008
#define IPC_MSG_TRUNC		0x00000010

#define IPC_OPEN_NONBLOCK	0x00000001
#define IPC_OPEN_EXCLUSIVE	0x00000002

#define IPC_CTL_GET_INFO	1
#define IPC_CTL_SET_MODE	2
#define IPC_CTL_DISCONNECT	3

#define IPC_STATE_READABLE	0x00000001
#define IPC_STATE_WRITABLE	0x00000002
#define IPC_STATE_HUP		0x00000004
#define IPC_STATE_SERVER	0x00000008
#define IPC_STATE_CLIENT	0x00000010

struct api_ipc_cred {
	uint32_t	pid;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	reserved;
};

struct api_ipc_message {
	uint64_t	id;
	uint64_t	reply_to;
	uint64_t	peer;
	uint32_t	opcode;
	uint32_t	flags;
	uint32_t	length;
	uint32_t	capacity;
	void		*data;
	struct api_ipc_cred cred;
	uint32_t	handle_count;
	uint32_t	handle_capacity;
	int		handles[IPC_MAX_HANDLES];
};

struct api_ipc_call {
	struct api_ipc_message	request;
	struct api_ipc_message	reply;
	int64_t		timeout_ms;
};

struct api_ipc_info {
	uint32_t	state;
	uint32_t	mode;
	uint32_t	pending_messages;
	uint32_t	pending_bytes;
	uint32_t	owner_pid;
	uint32_t	owner_uid;
	uint32_t	owner_gid;
	uint32_t	peer_count;
	uint64_t	peer;
	char		name[IPC_NAME_MAX];
};

struct api_drm_driver_list {
	struct api_drm_driver_entry *entries;
	uint32_t	max_entries;
	uint32_t	count;
};

struct api_drm_driver_switch {
	uint32_t	id;
};

ssize_t	termRead(void *buf, size_t count);
ssize_t	termReadFlags(void *buf, size_t count, uint32_t flags);
ssize_t	termWrite(const void *buf, size_t count);
ssize_t	termPrint(const char *text);
int	termInfo(struct api_term_info *info);
int	termPower(struct api_term_power *args);
int	termMouse(struct api_term_mouse *args);
int	termMode(struct api_term_mode *args);
int	termGetMode(struct api_term_mode *mode);
int	termSetMode(const struct api_term_mode *mode);
int	termEnterRaw(struct api_term_mode *saved);
int	termRestoreMode(const struct api_term_mode *saved);
int	ptyOpen(int *out_master_handle, int *out_pts_id);
int	inputRead(struct api_key_event *buf, uint32_t count, uint32_t flags);
int	inputPoll(void);
int	inputFlush(void);

int	dataOpen(const char *path, int flags);
int	dataClose(int handle);
ssize_t	dataRead(int handle, void *buf, size_t count);
ssize_t	dataWrite(int handle, const void *buf, size_t count);
int	dataReadFull(int handle, void *buf, size_t count);
int	dataWriteFull(int handle, const void *buf, size_t count);
long	dataSeek(int handle, long offset, int whence);
int	dataPipe(int handles[2]);
int	dataDir(uint32_t op, const char *path, const char *newpath);

int	fsChdir(const char *path);
int	fsGetcwd(char *buf, size_t size);
int	fsListdir(const char *path, struct api_dirent *buf, uint32_t max_entries);
int	fsStat(const char *path, struct api_fs_stat *buf);
int	fsRename(const char *oldpath, const char *newpath);
int	fsUnlink(const char *path);
int	fsLinkNew(const char *target, const char *linkpath, uint32_t flags);
int	fsLinkGo(const char *path, char *buf, uint32_t bufsize);
int	fsMnt(const char *source, const char *target, const char *fstype,
	    uint64_t flags, const void *data);
int	fsUmnt(const char *target, uint64_t flags);

void	*memMap(const struct mem_map_args *args);
int	memUnmap(void *addr, size_t length);
int	shmGet(uint64_t key, size_t size, uint32_t flags, int *handle);
void	*shmMap(int handle, void *addr, size_t size, uint32_t prot,
	    uint32_t flags);
int	shmCtl(int handle, int cmd, void *arg);
int	shmClose(int handle);

long	procClone(uint64_t flags, void *child_stack, uint64_t ptid);
int	procCopy(void);
int	procSpawn(const char *path, char *const argv[], char *const envp[]);
int	procSpawnAbi(const char *path, char *const argv[], char *const envp[],
	    uint32_t abi);
int	procSpawnNative(const char *path, char *const argv[], char *const envp[]);
int	procSpawnPty(const char *path, char *const argv[], char *const envp[],
	    int pts_id, uint32_t abi);
int	procWait(int *status);
int	procRun(const char *path, char *const argv[], char *const envp[],
	    int *status);
void	procExit(int code) __attribute__((noreturn));
int	procKill(uint32_t pid, int sig);
int	procList(struct api_proc_info *buf, uint32_t max_entries);
int	procGetpid(void);
int	procGetppid(void);
int	procGettid(void);
int	procPerm(uint32_t pid);
void	threadExit(int code) __attribute__((noreturn));
int	threadJoin(uint32_t tid, int *status);
void	procExitGroup(int code) __attribute__((noreturn));
int	procSetTidAddress(uint64_t tidptr);
int	procSetsid(void);
int	procGetsid(void);

int	futexWait(uint64_t uaddr, uint32_t expected_val);
int	futexWake(uint64_t uaddr, uint32_t max_waiters);
int	kusrAuth(const char *password);

int	sysInfo(struct api_sysinfo *buf);
int	sysMemInfo(struct api_meminfo *buf);
int	sysKmemInfo(struct api_kmeminfo *buf);
int	sysCpuInfo(struct api_cpuinfo *buf);
int	sysRandom(void *buf, size_t len);
int	sysTimeInfo(struct api_timeinfo *buf);
int	sysTime(void);
int	powerState(uint32_t state);

int	traceOpen(uint32_t flags);
int	traceClose(int trace);
ssize_t	traceRead(int trace, struct api_trace_read *args);
int	traceCtl(int trace, uint32_t op, void *arg);
int	traceInfo(uint32_t op, void *arg);
int	traceLoad(int trace, struct api_trace_load *load);
ssize_t	traceReadAggs(int trace, struct api_trace_aggs *args);
int	traceMark(uint32_t id, uint64_t a0, uint64_t a1, uint64_t a2,
	    uint64_t a3, uint64_t a4);

int	drmCall(uint64_t op, void *arg);
int	drmInfo(struct api_drm_info *info);
int	drmGemCreate(size_t size, uint32_t *handle);
int	drmGemClose(uint32_t handle);
int	drmGemMapInfo(uint32_t handle, struct api_drm_gem_map *info);
void	*drmGemMmap(uint32_t handle, size_t size, uint32_t prot);
int	drmFbCreate(uint32_t gem_handle, uint32_t width, uint32_t height,
	    uint32_t pitch, uint8_t bpp, uint32_t *fb_id);
int	drmFbDestroy(uint32_t fb_id);
int	drmGetObjects(struct api_drm_objects *objects);
int	drmAtomicCommit(struct api_drm_atomic_req *reqs, uint32_t count,
	    uint32_t flags);
int	drmRapiClear(uint32_t handle, uint32_t pitch, uint8_t bpp,
	    uint32_t color);
int	drmRapiPutPixel(uint32_t handle, uint32_t pitch, uint8_t bpp,
	    uint32_t x, uint32_t y, uint32_t color);
int	drmRapiFillRect(uint32_t handle, uint32_t pitch, uint8_t bpp,
	    uint32_t x, uint32_t y, uint32_t width, uint32_t height,
	    uint32_t color);
int	drmRapiGlyph(uint32_t handle, uint32_t pitch, uint8_t bpp,
	    uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
int	drmRapiScroll(uint32_t handle, uint32_t pitch, uint8_t bpp,
	    uint32_t lines, uint32_t bg);
int	drmRapiBlit(struct api_drm_rapi_blit *blit);
int	drmDriverList(struct api_drm_driver_entry *entries,
	    uint32_t max_entries, uint32_t *count);
int	drmDriverSwitch(uint32_t id);

int	eventKqueue(void);
int	eventClose(int kq);
int	eventWait(int kq, struct kevent *changes, int nchanges,
	    struct kevent *events, int nevents, int64_t timeout_ms);

int	netOpen(int proto, int mode, uint32_t flags);
int	netBind(int handle, const struct api_net_addr *addr);
int	netConnect(int handle, const struct api_net_addr *addr);
int	netListen(int handle, int backlog);
int	netAccept(int handle, struct api_net_addr *addr, uint32_t flags);
ssize_t	netSend(int handle, const struct api_net_msg *msg);
ssize_t	netRecv(int handle, struct api_net_msg *msg);
int	netCtl(int handle, int op, void *arg);
int	ipcCreate(const char *name, uint32_t flags, uint32_t mode);
int	ipcConnect(const char *name, uint32_t flags);
ssize_t	ipcSend(int handle, const struct api_ipc_message *message);
ssize_t	ipcRecv(int handle, struct api_ipc_message *message, uint32_t flags);
ssize_t	ipcCall(int handle, struct api_ipc_call *call);
int	ipcCtl(int handle, uint32_t op, void *arg);

int	regOpen(const char *hive, const char *key, uint32_t flags);
int	regClose(int reg);
ssize_t	regGet(int reg, struct api_reg_value *value);
int	regSet(int reg, const struct api_reg_value *value);
int	regCreateKey(int reg, const char *name);
int	regDeleteKey(int reg, const char *name);
int	regDeleteValue(int reg, const char *name);
int	regEnum(int reg, struct api_reg_entry *entry);
int	regEnumHives(struct api_reg_hive *hive);
int	regUpd(uint32_t consumer);
int	regGetBool(int reg, const char *name, int *out);
int	regSetBool(int reg, const char *name, int value);
int	regGetU32(int reg, const char *name, uint32_t *out);
int	regSetU32(int reg, const char *name, uint32_t value);
int	regGetIpv4(int reg, const char *name, uint32_t *out);
int	regSetIpv4(int reg, const char *name, uint32_t value);
int	regGetString(int reg, const char *name, char *buf, size_t size);
int	regSetString(int reg, const char *name, const char *value);

int	kofoLoad(const char *path, uint32_t flags);
int	kofoInfo(uint32_t id, struct api_kofo_info *info);
int	kofoUnload(uint32_t id, uint32_t flags);

int	entityCreate(uint16_t archetype, uint32_t access, const char *name);
int	entityCreateEx(uint16_t archetype, uint16_t flags, uint32_t access,
	    const char *name);
int	entityOpen(const char *name, uint32_t access);
int	entityClose(int handle);
int	entityDup(int handle, uint32_t access);
int	entityStat(int handle, struct api_entity_stat *stat);
int	entityList(const char *path, struct api_entity_entry *entries,
	    uint32_t max_entries);
int	entityQuery(uint16_t archetype, uint32_t start,
	    struct api_entity_entry *entries, uint32_t max_entries);
int	entityCtl(int handle, uint32_t op, void *arg);
int	entityGetData(int handle, uint32_t index, uint64_t *value);
int	entitySetData(int handle, uint32_t index, uint64_t value);
int	entityGetI32(int handle, uint32_t index, int32_t *value);
int	entitySetI32(int handle, uint32_t index, int32_t value);
int	entityBind(int handle, const char *name);
int	entityUnbind(int handle);
int	entityDelete(int handle);
ssize_t	entityRead(int handle, void *buf, size_t count);
ssize_t	entityWrite(int handle, const void *buf, size_t count);
long	entitySeek(int handle, long offset, int whence);
int	entityIoctl(int handle, uint64_t cmd, void *arg);

long	personality(long mode);

#endif
