/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type usize as machine sized unsigned
$define %type api_shmget_args as native shmget syscall args
$define %type api_shmmap_args as native shmmap syscall args
$define %type api_shminfo_args as native shm stat result

$define %func _start as start with args void
$define %func syscall1 as function with args long, long
$define %func syscall3 as function with args long, long, long, long
$define %func strlen as function with args const char *
$define %func print as procedure with args const char *
$define %func print_num as procedure with args long
$define %func fail as procedure with args const char *, long
$define %func pass as procedure with args const char *
$define %func shm_get as function with args u64, u64, u32, u32 *
$define %func shm_map as function with args u32, u64, u32, u64 *
$define %func shm_ctl as function with args u32, u32, void *
$define %func shm_close as function with args u32
$define %func mem_unmap as function with args void *, u64
$define %func fill_pattern as procedure with args unsigned char *, u64
$define %func check_pattern as function with args unsigned char *, u64
$define %func run_tests as function with args void

*/

/* !SPACE!

$space %export _start
$space %internal syscall1, syscall3, strlen, print, print_num
$space %internal fail, pass, shm_get, shm_map, shm_ctl, shm_close, mem_unmap
$space %internal fill_pattern, check_pattern, run_tests

*/

#define	CALL_TERM_WRITE		0x101
#define	CALL_MEM_UNMAP		0x301
#define	CALL_SHM_GET		0x302
#define	CALL_SHM_MAP		0x303
#define	CALL_SHM_CTL		0x304
#define	CALL_PROC_EXIT		0x403
#define	CALL_ENTITY_CLOSE	0xD02
#define	CALL_PERSONALITY	0xFFFF
#define	API_MAP_READ		0x1
#define	API_MAP_WRITE		0x2
#define	API_MAP_SHARED		0x01
#define	SHM_CREAT		01000
#define	SHM_CTL_RMID		0
#define	SHM_CTL_STAT		2
#define	SHMTEST_KEY		0x53484d54ULL
#define	SHMTEST_SIZE		8192
typedef unsigned int	u32;
typedef unsigned long	u64;
typedef unsigned long	usize;

struct api_shmget_args {
	u64	key;
	u64	size;
	u32	flags;
	u32	id;
};

struct api_shmmap_args {
	u32	id;
	u32	prot;
	u32	flags;
	u64	addr;
	u64	size;
};

struct api_shminfo_args {
	u32	id;
	u32	mode;
	u32	refs;
	u32	removed;
	u64	key;
	u64	size;
};

static long
syscall1(long num, long arg1)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(arg1)
	    : "rcx", "r11", "memory");
	return (ret);
}

static long
syscall3(long num, long arg1, long arg2, long arg3)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
	    : "rcx", "r11", "memory");
	return (ret);
}

static usize
strlen(const char *s)
{
	usize	len;

	len = 0;
	while (s[len] != 0) {
		len++;
	}
	return (len);
}

static void
print(const char *s)
{
	syscall3(CALL_TERM_WRITE, (long)s, (long)strlen(s), 0);
}

static void
print_num(long value)
{
	char	buf[32];
	long	n;
	int	i;
	int	neg;

	if (value == 0) {
		print("0");
		return;
	}

	neg = 0;
	n = value;
	if (n < 0) {
		neg = 1;
		n = -n;
	}

	i = 31;
	buf[i] = 0;
	while (n > 0 && i > 0) {
		i--;
		buf[i] = (char)('0' + (n % 10));
		n /= 10;
	}
	if (neg && i > 0) {
		i--;
		buf[i] = '-';
	}
	print(&buf[i]);
}

static void
fail(const char *what, long value)
{
	print("[shmtest] FAIL ");
	print(what);
	print(" ret=");
	print_num(value);
	print("\n");
}

static void
pass(const char *what)
{
	print("[shmtest] ok ");
	print(what);
	print("\n");
}

static long
shm_get(u64 key, u64 size, u32 flags, u32 *id)
{
	struct api_shmget_args	args;
	long			ret;

	args.key = key;
	args.size = size;
	args.flags = flags;
	args.id = 0;
	ret = syscall1(CALL_SHM_GET, (long)&args);
	if (ret == 0 && id != 0) {
		*id = args.id;
	}
	return (ret);
}

static long
shm_map(u32 id, u64 size, u32 prot, u64 *addr)
{
	struct api_shmmap_args	args;
	long			ret;

	args.id = id;
	args.prot = prot;
	args.flags = API_MAP_SHARED;
	args.addr = 0;
	args.size = size;
	ret = syscall1(CALL_SHM_MAP, (long)&args);
	if (ret > 0 && addr != 0) {
		*addr = (u64)ret;
	}
	return (ret);
}

static long
shm_ctl(u32 id, u32 cmd, void *arg)
{
	return (syscall3(CALL_SHM_CTL, (long)id, (long)cmd, (long)arg));
}

static long
shm_close(u32 id)
{
	return (syscall1(CALL_ENTITY_CLOSE, (long)id));
}

static long
mem_unmap(void *addr, u64 size)
{
	return (syscall3(CALL_MEM_UNMAP, (long)addr, (long)size, 0));
}

static void
fill_pattern(unsigned char *buf, u64 size)
{
	u64	i;

	for (i = 0; i < size; i++) {
		buf[i] = (unsigned char)((i * 31 + 7) & 0xff);
	}
}

static int
check_pattern(unsigned char *buf, u64 size)
{
	u64	i;

	for (i = 0; i < size; i++) {
		if (buf[i] != (unsigned char)((i * 31 + 7) & 0xff)) {
			return (-1);
		}
	}
	return (0);
}

static int
run_tests(void)
{
	struct api_shminfo_args	info;
	unsigned char		*a;
	unsigned char		*b;
	u64			addr1;
	u64			addr2;
	u32			id;
	long			ret;

	id = 0;
	addr1 = 0;
	addr2 = 0;

	ret = shm_get(SHMTEST_KEY, SHMTEST_SIZE, SHM_CREAT | 0600, &id);
	if (ret != 0 || id == 0) {
		fail("shm_get create", ret);
		return (1);
	}
	pass("shm_get create");

	ret = shm_ctl(id, SHM_CTL_STAT, &info);
	if (ret != 0 || info.refs != 0 || info.removed != 0 ||
	    info.size < SHMTEST_SIZE) {
		fail("stat after create", ret);
		return (1);
	}
	pass("stat after create");

	ret = shm_map(id, SHMTEST_SIZE, API_MAP_READ | API_MAP_WRITE, &addr1);
	if (ret < 0 || addr1 == 0) {
		fail("first map", ret);
		return (1);
	}
	pass("first map");

	ret = shm_map(id, SHMTEST_SIZE, API_MAP_READ | API_MAP_WRITE, &addr2);
	if (ret < 0 || addr2 == 0 || addr2 == addr1) {
		fail("second map", ret);
		return (1);
	}
	pass("second map");

	a = (unsigned char *)addr1;
	b = (unsigned char *)addr2;
	fill_pattern(a, SHMTEST_SIZE);
	if (check_pattern(b, SHMTEST_SIZE) != 0) {
		fail("shared bytes", -1);
		return (1);
	}
	b[123] = 0xa5;
	if (a[123] != 0xa5) {
		fail("reverse write", -1);
		return (1);
	}
	pass("shared bytes");

	ret = shm_ctl(id, SHM_CTL_STAT, &info);
	if (ret != 0 || info.refs != 2) {
		fail("stat refs 2", ret);
		return (1);
	}
	pass("stat refs 2");

	ret = mem_unmap((void *)addr1, SHMTEST_SIZE);
	if (ret != 0) {
		fail("unmap first", ret);
		return (1);
	}
	pass("unmap first");

	ret = shm_ctl(id, SHM_CTL_STAT, &info);
	if (ret != 0 || info.refs != 1) {
		fail("stat refs 1", ret);
		return (1);
	}
	pass("stat refs 1");

	ret = shm_ctl(id, SHM_CTL_RMID, 0);
	if (ret != 0) {
		fail("rmid", ret);
		return (1);
	}
	pass("rmid");

	ret = shm_ctl(id, SHM_CTL_STAT, &info);
	if (ret != 0 || info.removed != 1 || info.refs != 1) {
		fail("stat removed", ret);
		return (1);
	}
	pass("stat removed");

	b[321] = 0x5a;
	if (b[321] != 0x5a) {
		fail("removed mapping alive", -1);
		return (1);
	}
	pass("removed mapping alive");

	ret = shm_map(id, SHMTEST_SIZE, API_MAP_READ | API_MAP_WRITE, &addr1);
	if (ret >= 0) {
		fail("map removed id should fail", ret);
		return (1);
	}
	pass("map removed id fails");

	ret = shm_get(SHMTEST_KEY, SHMTEST_SIZE, 0, &id);
	if (ret >= 0) {
		fail("get removed key should fail", ret);
		return (1);
	}
	pass("get removed key fails");

	ret = mem_unmap((void *)addr2, SHMTEST_SIZE);
	if (ret != 0) {
		fail("unmap second", ret);
		return (1);
	}
	pass("unmap second");

	ret = shm_ctl(id, SHM_CTL_STAT, &info);
	if (ret != 0 || info.removed != 1 || info.refs != 0) {
		fail("stat after final detach", ret);
		return (1);
	}
	pass("stat after final detach");

	ret = shm_close(id);
	if (ret != 0) {
		fail("close handle", ret);
		return (1);
	}
	ret = shm_ctl(id, SHM_CTL_STAT, &info);
	if (ret >= 0) {
		fail("stat after close should fail", ret);
		return (1);
	}
	pass("final cleanup");
	return (0);
}

void
_start(void)
{
	int	code;

	syscall1(CALL_PERSONALITY, 0);
	print("[shmtest] native shm test start\n");
	code = run_tests();
	if (code == 0) {
		print("[shmtest] PASS\n");
	}
	syscall1(CALL_PROC_EXIT, code);
	for (;;) {
	}
}
