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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type multiboot_info_t as struct with multiboot1 info
$define %type multiboot2_info_t as struct with multiboot2 info
$define %type multiboot2_tag_t as struct with multiboot2 tag header
$define %type multiboot2_tag_module_t as struct with module tag
$define %type module_copy_ctx_t as struct with multiboot pointers, boot magic, init module

$define %func debug_multiboot_info as procedure with args multiboot_info_t *
$define %func debug_multiboot2_tags as procedure with args multiboot2_info_t *
$define %func mb2_find_module as function with args multiboot2_info_t *, const char *, void **, u32 *
$define %func mb2_total_modules_size as function with args multiboot2_info_t *
$define %func status_line as procedure with args const char *, int
$define %func timer_sanity_check as function with args void
$define %func net_test as procedure with args void
$define %func enable_sse as procedure with args void
$define %func kernel_ensure_parent_dirs as function with args const char *
$define %func kernel_install_module_cb as procedure with args const char *, const char *, void *
$define %func kernel_install_registry_module_cb as function with args const char *, void *
$define %func kmain as start with args u64, u64, u64, u64

*/

/* !SPACE!

$space %internal debug_multiboot_info, debug_multiboot2_tags
$space %internal mb2_find_module, mb2_total_modules_size, status_line
$space %internal timer_sanity_check, net_test, enable_sse
$space %internal kernel_ensure_parent_dirs, kernel_install_module_cb
$space %internal kernel_install_registry_module_cb
$space %export kmain

*/

#include <kernel/interrupts/apic/lapic.h>
#include <kernel/interrupts/apic/ioapic.h>
#include <kernel/cm/cm.h>
#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/disk/disk.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/drivers/power/power.h>
#include <kernel/drivers/timer.h>
#include <kernel/console/terminal.h>
#include <kernel/time.h>
#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/watchdog/watchdog.h>
#include <kernel/net/net.h>
#include <kernel/net/arp.h>
#include <kernel/net/icmp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/udp.h>
#include <kernel/event/event.h>
#include <kernel/interrupts/idt.h>
#include <kernel/console/console.h>
#include <mm/vm/pmap.h>
#include <mm/mm.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>
#include <kernel/panic.h>
#include <kernel/pci/pci.h>
#include <kernel/api/api.h>
#include <kernel/bootmem.h>
#include <kernel/crypto/crypto.h>
#include <kernel/kshell/kshell.h>
#include <kernel/other/kusr.h>
#include <kernel/syscall.h>
#include <kernel/smp/smp.h>
#include <kernel/sync/sync.h>
#include <kernel/trace/trace.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mlibc/stdlib.h>
#include <userland/userspace.h>

extern void	cpuid_get(u32 code, u32 *res);
extern void	cinfo(char *buf);
extern u64	rinfo(u64 mb_ptr);
extern char	start;
extern char	kernel_end;

static u32	boot_magic;
static int	is_multiboot2;

#define BOOT_FLAG_DISABLE_APIC	0x00000001ULL

static void
debug_multiboot_info(multiboot_info_t *mb_info)
{
	printk("Flags: 0x%x\n", mb_info->flags);

	if (mb_info->flags & MULTIBOOT_FLAG_MEM) {
		printk("mem_lower: %u KB, mem_upper: %u KB\n",
		    mb_info->mem_lower, mb_info->mem_upper);
	}

	if (mb_info->flags & MULTIBOOT_FLAG_CMDLINE) {
		printk("cmdline: %s\n",
		    (const char *)(u64)mb_info->cmdline);
	}

	if (mb_info->flags & MULTIBOOT_FLAG_BOOTLOADER_NAME) {
		printk("bootloader: %s\n",
		    (const char *)(u64)mb_info->boot_loader_name);
	}

	if (mb_info->flags & MULTIBOOT_FLAG_FRAMEBUFFER) {
		printk("framebuffer: %ux%ux%u\n",
		    mb_info->framebuffer_width,
		    mb_info->framebuffer_height,
		    mb_info->framebuffer_bpp);
	}
}

static void
debug_multiboot2_tags(multiboot2_info_t *mb_info)
{
	multiboot2_tag_t	*tag;
	u64			next_addr;

	tag = (multiboot2_tag_t *)((u8 *)mb_info + 8);

	while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
		printk("Tag type: %u, size: %u", tag->type,
		    tag->size);

		switch (tag->type) {
		case MULTIBOOT2_TAG_TYPE_CMDLINE:
			printk(" (CMDLINE)");
			break;
		case MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME:
			printk(" (BOOT_LOADER_NAME)");
			break;
		case MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO:
			printk(" (BASIC_MEMINFO)");
			break;
		case MULTIBOOT2_TAG_TYPE_MMAP:
			printk(" (MMAP)");
			break;
		case MULTIBOOT2_TAG_TYPE_FRAMEBUFFER:
			printk(" (FRAMEBUFFER)");
			break;
		case MULTIBOOT2_TAG_TYPE_ELF_SECTIONS:
			printk(" (ELF_SECTIONS)");
			break;
		case MULTIBOOT2_TAG_TYPE_ACPI_OLD:
			printk(" (ACPI_OLD)");
			break;
		case MULTIBOOT2_TAG_TYPE_ACPI_NEW:
			printk(" (ACPI_NEW)");
			break;
		default:
			printk(" (other)");
			break;
		}
		printk("\n");

		next_addr = (u64)tag + tag->size;
		next_addr = (next_addr + 7) & ~7;
		tag = (multiboot2_tag_t *)next_addr;
	}
}

static int
mb2_find_module(multiboot2_info_t *mb_info, const char *name,
    void **out_start, u32 *out_size)
{
	return (multiboot2_find_module(mb_info, name, out_start,
	    out_size));
}
static u32
mb2_total_modules_size(multiboot2_info_t *mb_info)
{
	multiboot2_tag_t		*tag;
	multiboot2_tag_module_t		*mod;
	u32				total;
	u64				next_addr;

	total = 0;
	tag = (multiboot2_tag_t *)((u8 *)mb_info + 8);

	while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
		if (tag->type == MULTIBOOT2_TAG_TYPE_MODULE) {
			mod = (multiboot2_tag_module_t *)tag;
			total += mod->mod_end - mod->mod_start;
		}
		next_addr = (u64)tag + tag->size;
		next_addr = (next_addr + 7) & ~7;
		tag = (multiboot2_tag_t *)next_addr;
	}
	return (total);
}

static void
status_line(const char *label, int ok)
{
	const int	pad_col = 32;
	int		len, i;

	len = strlen(label);
	printf("%s", label);
	printk("%s", label);
	for (i = len; i < pad_col; i++) {
		console_putchar(' ');
		printk(" ");
	}
	if (ok) {
		printf("\033[32m[OK]\033[0m\n");
	} else {
		printf("\033[31m[FAILED]\033[0m\n");
	}
}

static int
timer_sanity_check(void)
{
	u64	start;
	u64	i;

	if (!timer_is_initialized()) {
		return (0);
	}
	start = timer_get_ticks();
	for (i = 0; i < 1000000 &&
	    timer_get_ticks() == start; i++) {
		__asm__ volatile("pause");
	}
	return (timer_get_ticks() != start);
}

static void
enable_sse(void)
{
	u64	cr0, cr4;

	__asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 &= ~(1ULL << 2);
	cr0 &= ~(1ULL << 3);
	cr0 |= (1ULL << 1);
	__asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

	__asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= (1ULL << 9);
	cr4 |= (1ULL << 10);
	__asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
}

typedef struct {
	multiboot2_info_t	*mboot2_ptr;
	multiboot_info_t	*mboot1_ptr;
	u32			boot_magic;
	void			*init_mod;
	u32			init_sz;
} module_copy_ctx_t;

static int
kernel_ensure_parent_dirs(const char *path)
{
	char	buf[256];
	int	len;
	int	i;

	if (!path || path[0] != '/') {
		return (-1);
	}

	len = strlen(path);
	if (len >= (int)sizeof(buf)) {
		return (-1);
	}
	memcpy(buf, path, len + 1);

	for (i = 1; i < len; i++) {
		if (buf[i] == '/') {
			buf[i] = '\0';
			vfs_mkdir(buf);
			buf[i] = '/';
		}
	}
	return (0);
}

static void
kernel_install_module_cb(const char *name, const char *dest, void *ctx)
{
	module_copy_ctx_t	*c;
	void			*mod;
	u32			sz;
	int			res;

	c = (module_copy_ctx_t *)ctx;
	if (!name || !dest || !c) {
		return;
	}

	mod = NULL;
	sz = 0;
	if (c->boot_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
		mb2_find_module(c->mboot2_ptr, name, &mod, &sz);
	} else {
		return;
	}

	if (!mod || sz == 0) {
		printk("[KERNEL] Module '%s' not found, skipping "
		    "copy to %s\n", name, dest);
		return;
	}

	if (kernel_ensure_parent_dirs(dest) != 0) {
		printk("[KERNEL] Failed to create parent directories "
		    "for %s\n", dest);
		return;
	}

	res = vfs_write_file(dest, (const u8 *)mod, sz);
	if (res == 0) {
		printk("[KERNEL] Installed %s from module '%s' "
		    "(%u bytes)\n", dest, name, sz);
	} else {
		printk("[KERNEL] Failed to install %s from module "
		    "'%s'\n", dest, name);
	}

	if (strcmp(name, "init") == 0) {
		c->init_mod = mod;
		c->init_sz = sz;
	}
}

static int
kernel_install_registry_module_cb(const char *name, void *ctx)
{
	char	key[128];
	char	dest[256];
	int	ret;

	if (!name || !ctx) {
		return (0);
	}
	if (strlen("Modules.") + strlen(name) >= sizeof(key)) {
		printk("[KERNEL] Registry module key too long: %s\n",
		    name);
		return (0);
	}

	strcpy(key, "Modules.");
	strcat(key, name);
	ret = cm_get_string("BOOT", key, "Dest", dest, sizeof(dest));
	if (ret != 0 || dest[0] == '\0') {
		printk("[KERNEL] Registry module '%s' has no Dest\n",
		    name);
		return (0);
	}

	kernel_install_module_cb(name, dest, ctx);
	return (0);
}

static void
net_test(void)
{
	net_iface_t		*iface;
	netdev_t		*ndev;
	arp_cache_entry_t	*entry;
	u64			start, timeout, wait_us;
	u32			test_ip, ping_ip;
	int			i;
	u8			ping_data[56];
	u8			icmp_pkt[sizeof(icmp_header_t) + 56];
	icmp_header_t		*req;

	if (!net_is_initialized()) {
		printk("[NET_TEST] network subsystem not initialized\n");
		return;
	}
	printk("[NET_TEST] %d device(s) %d interface(s)\n",
	    netdev_count(), net_iface_count());
	netdev_dump_all();
	net_dump_ifaces();

	for (i = 0; i < netdev_count(); i++) {
		ndev = netdev_get(i);
		if (!ndev || !(ndev->flags & NETDEV_F_UP)) {
			continue;
		}

		printk("[NET_TEST] polling %s\n", ndev->name);
		ndev->ops->poll(ndev);
		printk("[NET_TEST] %s mac=%02x:%02x:%02x:%02x:%02x:%02x "
		    "link=%s\n",
		    ndev->name,
		    ndev->mac[0], ndev->mac[1], ndev->mac[2],
		    ndev->mac[3], ndev->mac[4], ndev->mac[5],
		    ndev->ops->is_link_up(ndev) ? "up" : "down");

		iface = net_iface_find_by_ndev(ndev);
		if (!iface) {
			continue;
		}

		if (!ndev->ops->is_link_up(ndev)) {
			continue;
		}
		if (iface->ip_addr == 0) {
			net_cm_update(0);
		}
		if (iface->ip_addr == 0) {
			printk("[NET_TEST] %s: no IP, "
			    "skipping\n", ndev->name);
			continue;
		}

		test_ip = iface->gw_addr ? iface->gw_addr :
		    (iface->ip_addr & iface->netmask) | 1;

		/* --- ARP resolution test --- */
		printk("[NET_TEST] === ARP test ===\n");
		arp_announce(iface);
		printk("[NET_TEST] sending ARP request for "
		    "gateway %d.%d.%d.%d...\n",
		    (test_ip >> 24) & 0xFF,
		    (test_ip >> 16) & 0xFF,
		    (test_ip >> 8) & 0xFF,
		    test_ip & 0xFF);
		net_poll_all();
		if (arp_send_request(iface, test_ip) != 0) {
			printk("[NET_TEST] %s: ARP request failed\n",
			    ndev->name);
			continue;
		}

		start = timer_get_ticks();
		timeout = timer_get_frequency();
		if (timeout == 0) {
			timeout = 1000;
		}
		entry = NULL;
		while (timer_get_ticks() - start < timeout) {
			net_poll_all();
			entry = arp_lookup(iface, test_ip);
			if (entry) {
				break;
			}
			__asm__ volatile("pause");
		}
		if (entry) {
			printk("[NET_TEST] %s: ARP reply from "
			    "%d.%d.%d.%d "
			    "(%02x:%02x:%02x:%02x:%02x:%02x)\n",
			    ndev->name,
			    (test_ip >> 24) & 0xFF,
			    (test_ip >> 16) & 0xFF,
			    (test_ip >> 8) & 0xFF,
			    test_ip & 0xFF,
			    entry->mac[0], entry->mac[1], entry->mac[2],
			    entry->mac[3], entry->mac[4], entry->mac[5]);
		} else {
			printk("[NET_TEST] %s: no ARP reply from "
			    "%d.%d.%d.%d within 1 second\n",
			    ndev->name,
			    (test_ip >> 24) & 0xFF,
			    (test_ip >> 16) & 0xFF,
			    (test_ip >> 8) & 0xFF,
			    test_ip & 0xFF);
			continue;
		}

		/* --- ICMP echo test --- */
		printk("[NET_TEST] === ICMP echo test ===\n");
		ping_ip = test_ip;
		memset(ping_data, 0x42, sizeof(ping_data));
		printk("[NET_TEST] sending ICMP echo request to "
		    "%d.%d.%d.%d...\n",
		    (ping_ip >> 24) & 0xFF,
		    (ping_ip >> 16) & 0xFF,
		    (ping_ip >> 8) & 0xFF,
		    ping_ip & 0xFF);
		memset(icmp_pkt, 0, sizeof(icmp_pkt));
		req = (icmp_header_t *)icmp_pkt;
		req->type = ICMP_TYPE_ECHO_REQUEST;
		req->code = 0;
		req->id = __builtin_bswap16(1);
		req->seq = __builtin_bswap16(1);
		memcpy(icmp_pkt + sizeof(icmp_header_t),
		    ping_data, 56);
		req->checksum = __builtin_bswap16(
		    ipv4_checksum(icmp_pkt, sizeof(icmp_pkt)));

		ipv4_output(iface, ping_ip, IPV4_PROTO_ICMP,
		    icmp_pkt, sizeof(icmp_pkt));

		start = timer_get_ticks();
		wait_us = timer_get_frequency() / 100;
		if (wait_us == 0) {
			wait_us = 10000;
		}
		while (timer_get_ticks() - start < wait_us) {
			net_poll_all();
			__asm__ volatile("pause");
		}

		/* --- UDP test --- */
		printk("[NET_TEST] === UDP test ===\n");
		printk("[NET_TEST] sending UDP datagram to "
		    "%d.%d.%d.%d:7...\n",
		    (test_ip >> 24) & 0xFF,
		    (test_ip >> 16) & 0xFF,
		    (test_ip >> 8) & 0xFF,
		    test_ip & 0xFF);
		udp_output(iface, test_ip, 12345, 7,
		    (const u8 *)"otsos2-udp-test", 15);

		start = timer_get_ticks();
		wait_us = timer_get_frequency() / 100;
		if (wait_us == 0) {
			wait_us = 10000;
		}
		while (timer_get_ticks() - start < wait_us) {
			net_poll_all();
			__asm__ volatile("pause");
		}

		printk("[NET_TEST] %s stats: tx=%llu done=%llu "
		    "drop=%llu rx=%llu delivered=%llu drop=%llu "
		    "icmp_unreach=%d frag_drop=%d\n",
		    ndev->name, ndev->tx_submitted, ndev->tx_completed,
		    ndev->tx_dropped, ndev->rx_completed,
		    ndev->rx_delivered, ndev->rx_dropped,
		    ipv4_get_icmp_unreach_sent(),
		    ipv4_get_frag_dropped());
	}
}

void
kmain(u64 magic, u64 addr, u64 boot_option, u64 boot_flags)
{
	int		safe_mode, debug_mode, disable_apic;
	void		*module_pool;
	u32		module_pool_sz;
	multiboot2_info_t	*mboot2_ptr;
	multiboot_info_t	*mboot1_ptr;
	char		cpu_buf[64];
	char		*p;
	u64		ram_kb;
	char		c;
	int		fs_ok, heap_ok, idt_ok, timer_ok;
	int		pmap_ok, syscall_ok, disk_ok;
	int		storage_ok, fb_ok, drm_atomic_ok, acpi_ok;
	int		power_ok, pci_ok, watchdog_ok;
	int		cm_ok;
	vnode_t		*root_vn;
	u32		timer_hz;
	module_copy_ctx_t	mod_ctx;
	newbus_bootinfo_t	nb_boot;

	safe_mode = (boot_option == 1);
	debug_mode = (boot_option == 2);
	disable_apic = ((boot_flags & BOOT_FLAG_DISABLE_APIC) != 0);
	mboot2_ptr = NULL;
	mboot1_ptr = NULL;
	timer_hz = 1000;

	console_early_init();
	sync_init();
	bootmem_init(magic, addr, 0x100000,
    (u64)&kernel_end - KERNEL_VMA);
	kmem_init();

	stdio_init();
	memset(&nb_boot, 0, sizeof(nb_boot));
	nb_boot.magic = (u32)magic;
	nb_boot.mb2 = (magic == MULTIBOOT2_BOOTLOADER_MAGIC) ?
	    (void *)addr : NULL;
	nb_boot.mb1 = (magic == MULTIBOOT_BOOTLOADER_MAGIC) ?
	    (void *)addr : NULL;
	nb_boot.timer_hz = timer_hz;
	nb_boot.disable_apic = disable_apic;
	nb_boot.debug_mode = debug_mode;
	newbus_bootstrap(&nb_boot);

	init_idt();
	pmap_init();
	
	module_pool_sz = 0;
	if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
		module_pool_sz = mb2_total_modules_size(
		    (multiboot2_info_t *)addr);
	}
	if (module_pool_sz < 8 * 512) {
		module_pool_sz = 8 * 512;
	}
	module_pool_sz += 1024 * 1024;
	module_pool_sz = (module_pool_sz + PAGE_SIZE - 1) &
	    ~(PAGE_SIZE - 1);
	module_pool = bootmem_alloc(module_pool_sz, PAGE_SIZE);
	if (module_pool) {
		printk("[BOOT] module pool allocated: %u bytes "
		    "at %p\n", module_pool_sz, module_pool);
	} else {
		printk("[BOOT] module pool allocation failed\n");
	}
#define KMEM_GROWTH_RESERVE_SIZE	(22 * 1024 * 1024)
	{
		void *gpool = bootmem_alloc(
		    KMEM_GROWTH_RESERVE_SIZE, PAGE_SIZE);
		if (gpool) {
			kmem_set_growth_pool(gpool,
			    KMEM_GROWTH_RESERVE_SIZE);
		} else {
			printk("[KMEM] no growth reserve, "
			    "will use bootmem fallback\n");
		}
	}
	vm_page_init_from_bootmem();
	nb_boot.module_pool = module_pool;
	nb_boot.module_pool_size = module_pool_sz;
	newbus_update_bootinfo(&nb_boot);
	if (disable_apic) {
		printk("[BOOT] APIC disabled by boot settings\n");
	}
	newbus_configure_pass(NEWBUS_PASS_TIMER);
	timer_hz = cm_get_u32_default("SYSTEM", "Timer", "Hz", 1000);
	nb_boot.timer_hz = timer_hz;
	newbus_update_bootinfo(&nb_boot);
	timer_init(timer_hz);
	time_init();
	vm_object_init();
	uma_init();
	enable_sse();
	trace_init();
	__asm__ volatile("sti");

	syscall_init();
	event_init();
	crypto_rng_init();

	boot_magic = (u32)magic;
	pci_set_verbose_scan(debug_mode);
	if (debug_mode) {
		printk("[BOOT] Debug mode: verbose PCI "
		    "scan enabled\n");
	}

	if (boot_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
		is_multiboot2 = 1;
		printk("boot from mb2 (magic: 0x%x)\n",
		    boot_magic);

		mboot2_ptr = (multiboot2_info_t *)addr;
		debug_multiboot2_tags(mboot2_ptr);

		newbus_configure_pass(NEWBUS_PASS_DISPLAY);

		clear_scr();
		terminal_init();
		stdio_set_terminal_mirror(terminal_log_mirror);

		cinfo(cpu_buf);
		p = cpu_buf;
		while (*p == ' ') {
			p++;
		}

		ram_kb = multiboot2_get_ram_kb(mboot2_ptr);

		printk("CPU: %s\n", p);
		printk("RAM: %u MB (%u KB)\n",
		    ram_kb / 1024, ram_kb);

		if (drm_is_ready()) {
			printf("CPU: %s\n", p);
			printf("RAM: %u MB\n", ram_kb / 1024);
		}

	} else if (boot_magic == MULTIBOOT_BOOTLOADER_MAGIC) {
		is_multiboot2 = 0;
		printk("boot from mb1 (magic: 0x%x)\n",
		    boot_magic);

		mboot1_ptr = (multiboot_info_t *)addr;
		debug_multiboot_info(mboot1_ptr);
		newbus_configure_pass(NEWBUS_PASS_DISPLAY);
		clear_scr();
		terminal_init();
		stdio_set_terminal_mirror(terminal_log_mirror);

		cinfo(cpu_buf);
		p = cpu_buf;
		while (*p == ' ') {
			p++;
		}

		ram_kb = multiboot_get_ram_kb(mboot1_ptr);

		printk("CPU: %s\n", p);
		printk("RAM: %u MB (%u KB)\n",
		    ram_kb / 1024, ram_kb);

		if (drm_is_ready()) {
			printf("CPU: %s\n", p);
			printf("RAM: %u MB\n", ram_kb / 1024);
		}

	} else {
		panic("ERROR: Unknown bootloader magic: "
		    "0x%x\nExpected MB1: 0x%x or MB2: "
		    "0x%x\n",
		    boot_magic, MULTIBOOT_BOOTLOADER_MAGIC,
		    MULTIBOOT2_BOOTLOADER_MAGIC);
	}

	newbus_configure();
	if (watchdog_device_count() > 0) {
		if (watchdog_start(WATCHDOG_DEFAULT_TIMEOUT_SEC)
		    != 0) {
			panic("[WDT] failed to start watchdog\n");
		}
	}

	heap_ok = kmem_is_initialized() &&
	    kmem_free_bytes() > 0;
	idt_ok = idt_is_loaded();
	timer_ok = timer_sanity_check();
	pmap_ok = pmap_is_initialized() &&
	    pmap_get_cr3() != 0;
	syscall_ok = syscall_is_initialized();
	disk_ok = disk_manager_is_initialized();
	storage_ok = (disk_count() > 0);
	fb_ok = drm_is_ready() != 0;
	drm_atomic_ok = drm_is_ready();
	acpi_ok = acpi_is_initialized();
	power_ok = power_is_initialized();
	pci_ok = pci_is_initialized();
	watchdog_ok = watchdog_is_initialized() &&
	    watchdog_device_count() > 0;
	root_vn = NULL;
	fs_ok = 0;
	if (vfs_is_initialized() &&
	    vfs_resolve("/", &root_vn) == 0 && root_vn != NULL) {
		fs_ok = 1;
		vnode_release(root_vn);
	}
	cm_ok = 0;
	if (fs_ok) {
		cm_ok = (cm_init() == 0);
	}
	if (cm_ok) {
		sync_configure();
		(void)cm_update_consumer(CM_CONSUMER_INPUT, 0);
		(void)cm_update_consumer(CM_CONSUMER_NET, 0);
	}

	status_line("kmem heap", heap_ok);
	status_line("idt", idt_ok);
	status_line("timer", timer_ok);
	status_line("pmap", pmap_ok);
	status_line("syscall", syscall_ok);
	status_line("disk manager", disk_ok);
	status_line("storage devices", storage_ok);
	status_line("framebuffer", fb_ok);
	status_line("drm", drm_atomic_ok);
	status_line("acpi", acpi_ok);
	status_line("power", power_ok);
	status_line("pci scan", pci_ok);
	status_line("watchdog", watchdog_ok);
	status_line("vfs", vfs_is_initialized());
	status_line("root filesystem", fs_ok);
	status_line("cm registry", cm_ok);

	net_test();

	sleep(430);

	kshell_set_boot_info(is_multiboot2);

	if (cm_ok) {
		(void)cm_update_consumer(CM_CONSUMER_CONSOLE, 0);
	}
	terminal_set_active(terminal_get_default_tty());

	if (!fs_ok) {
		printk("[VFS] root filesystem unavailable, "
		    "skipping userspace startup\n");
	}

	if (!safe_mode && fs_ok) {
		while (keyboard_getchar() != 0) {
			;
		}

		userspace_init();

		memset(&mod_ctx, 0, sizeof(mod_ctx));
		mod_ctx.mboot2_ptr = mboot2_ptr;
		mod_ctx.mboot1_ptr = mboot1_ptr;
		mod_ctx.boot_magic = boot_magic;

		api_init();

		if (cm_is_initialized()) {
			if (cm_foreach_key("BOOT", "Modules",
			    kernel_install_registry_module_cb, &mod_ctx) != 0) {
				printk("[KERNEL] Cannot read registry "
				    "BOOT.Modules\n");
			}
		} else {
			printk("[KERNEL] No registry loaded, "
			    "cannot install multiboot modules\n");
		}

		kusr_init();

		/*
		 * Hand the system console over to userspace: suspend all
		 * TTYs now so that init can explicitly wake the one it
		 * wants to use.  The boot menu / disk selection remains
		 * visible up to this point.
		 */

		terminal_power_suspend_all();

		if (mod_ctx.init_mod && mod_ctx.init_sz > 0) {
			printk("[KERNEL] Found init module "
			    "at %p, size %d. Starting init...\n",
			    mod_ctx.init_mod, mod_ctx.init_sz);
			userspace_load_init(mod_ctx.init_mod,
			    (u64)mod_ctx.init_sz);
		} else {
			printk("[KERNEL] Init module not "
			    "found! Falling back to kernel "
			    "loop...\n");
			while (1) {
				c = keyboard_getchar();
				if (c) {
					printf("\033[31m %c "
					    "\033[0m", c);
				}
			}
		}
	} else {
		if (!fs_ok) {
			printf("\n\033[31mRoot filesystem unavailable. "
			    "Staying in kernel console.\033[0m\n");
		}
		if (safe_mode) {
			printf("\n\033[33m--- SAFE MOD ---"
			    "\033[0m\n");
			printf("init and userlang are disabled.\n");
			kshell_run();
		} else if (debug_mode) {
			printf("\n\033[36m--- DEBUG MODE ---"
			    "\033[0m\n");
			printf("userspace disabled because "
			    "filesystem is unavailable.\n");
			kshell_run();
		}

		while (1) {
			c = keyboard_getchar();
			if (c) {
				printf("%c", c);
			}
		}
	}
}
