const std = @import("std");
const uefi = std.os.uefi;

const BlockIo = uefi.protocol.BlockIo;
const BootServices = uefi.tables.BootServices;
const ConfigurationTable = uefi.tables.ConfigurationTable;
const DevicePath = uefi.protocol.DevicePath;
const File = uefi.protocol.File;
const Guid = uefi.Guid;
const GraphicsOutput = uefi.protocol.GraphicsOutput;
const Handle = uefi.Handle;
const LoadedImage = uefi.protocol.LoadedImage;
const SimpleFileSystem = uefi.protocol.SimpleFileSystem;
const MemoryMapSlice = uefi.tables.MemoryMapSlice;
const MemoryType = uefi.tables.MemoryType;
const Page = uefi.Page;

const KERNEL_LOAD_ADDR: usize = 0x00100000;
const MB2_INFO_CAP: usize = 0x00010000;
const BOOTPACK_MAX_SIZE: usize = 0x04000000;
const MMAP_BUF_SIZE: usize = 0x00010000;
const MB2_MMAP_ENTRY_MAX: usize = 512;
const LOW_MAX_ADDR: usize = 0xeffff000;

const CFSR_BLOCK_SIZE: u32 = 512;
const CFSR_TYPE_FILE: u8 = 0;
const CFS_MAX_RUN: u32 = 64;
const CFS_KERNEL_PATH = "/system/init/kernel.bin";

const CFS_CMSEED_PATH = "/system/init/cmseed";
const CFS_CMSEED_NAME = "cmseed";
const BOOTPACK_KERNEL_NAME = "kernel";

const CFS_STAGE_MIN_ADDR: usize = 0x01000000;

const MB2_BOOTLOADER_MAGIC: u32 = 0x36d76289;
const COM1: u16 = 0x3f8;

const BootError = error{
	BadBootpack,
	AcpiNotFound,
	BootpackTooLarge,
	FileReadFailed,
	GopUnavailable,
	KernelLoadFailed,
	KernelNotFound,
	ModuleLoadFailed,
	MemoryMapTooBig,
	NoBootServices,
	NoDeviceHandle,
	NoFilesystem,
	OutOfMemory,
};

const Bootpack = extern struct {
	data: [*c]const u8,
	size: u32,
};

const BootpackFile = extern struct {
	name: [*c]const u8,
	data: [*c]const u8,
	size: u32,
};

const Mb2Builder = extern struct {
	buf: [*c]u8,
	cap: u32,
	off: u32,
};

const Mb2Framebuffer = extern struct {
	addr: u64,
	pitch: u32,
	width: u32,
	height: u32,
	bpp: u32,
	type: u32,
};

const Mb2MmapEntry = extern struct {
	base_addr: u64,
	length: u64,
	type: u32,
	reserved: u32,
};

const ModuleCtx = struct {
	mb: *Mb2Builder,
	failed: bool,
};

const CfsrEntry = extern struct {
	status: u8,
	type: u8,
	name: [30]u8,
	size: u32 align(1),
	start_block: u32 align(1),
	parent_block: u32 align(1),
	nlink: u32 align(1),
	reserved: [12]u8,
};

const CfsrVolume = extern struct {
	read: ?*const fn (?*anyopaque, u64, u32, ?*anyopaque) callconv(.c) c_int,
	ctx: ?*anyopaque,
	base_lba: u64,
	block_count: u32,
	file_table_blocks: u32,
	block_map_blocks: u32,
	root_dir_block: u32,
	data_area_start: u32,
	map_cached: u32,
	max_run: u32,
	sector: [CFSR_BLOCK_SIZE]u8,
	map: [CFSR_BLOCK_SIZE]u8,
};

const GptrPart = extern struct {
	first_lba: u64,
	last_lba: u64,
	index: u32,
};

const BlockCtx = struct {
	io: *BlockIo,
	media_id: u32,
	base_lba: u64,
};

const CfsBoot = struct {
	stage: usize,
	size: u32,
};

const FileImage = struct {
	addr: usize,
	size: usize,
};

const AcpiRsdp = struct {
	ptr: [*]const u8,
	size: u32,
	is_new: bool,
};

extern fn uefi_outb(port: u16, value: u8) callconv(.c) void;
extern fn uefi_inb(port: u16) callconv(.c) u8;
extern fn uefi_jump32(entry: u32, magic: u32, info: u32) callconv(.c) noreturn;
extern fn uefi_halt() callconv(.c) noreturn;

extern fn bootpack_init(pack: *Bootpack, data: ?*const anyopaque, size: u32) callconv(.c) void;
extern fn bootpack_find(pack: *Bootpack, name: [*:0]const u8, out: *BootpackFile) callconv(.c) c_int;
extern fn bootpack_foreach(pack: *Bootpack, cb: *const fn (*const BootpackFile, ?*anyopaque) callconv(.c) c_int, ctx: ?*anyopaque) callconv(.c) c_int;
extern fn elf64_load_kernel(data: ?*const anyopaque, size: u32, entry: *u64, kernel_end: *u64) callconv(.c) c_int;
extern fn mb2_builder_init(b: *Mb2Builder, buf: ?*anyopaque, cap: u32) callconv(.c) void;
extern fn mb2_add_bootloader_name(b: *Mb2Builder, name: [*:0]const u8) callconv(.c) c_int;
extern fn mb2_add_basic_meminfo(b: *Mb2Builder, lower: u32, upper: u32) callconv(.c) c_int;
extern fn mb2_add_simple_mmap(b: *Mb2Builder, lower: u32, upper: u32) callconv(.c) c_int;
extern fn mb2_add_mmap_entries(b: *Mb2Builder, entries: [*]const Mb2MmapEntry, count: u32) callconv(.c) c_int;
extern fn mb2_add_framebuffer(b: *Mb2Builder, fb: *const Mb2Framebuffer) callconv(.c) c_int;
extern fn mb2_add_module(b: *Mb2Builder, start: u32, end: u32, name: [*:0]const u8) callconv(.c) c_int;
extern fn mb2_add_acpi(b: *Mb2Builder, rsdp: ?*const anyopaque, size: u32, is_new: c_int) callconv(.c) c_int;
extern fn mb2_builder_finish(b: *Mb2Builder) callconv(.c) u32;

extern fn cfsr_mount(vol: *CfsrVolume, read: *const fn (?*anyopaque, u64, u32, ?*anyopaque) callconv(.c) c_int, ctx: ?*anyopaque, base_lba: u64, max_run: u32) callconv(.c) c_int;
extern fn cfsr_lookup(vol: *CfsrVolume, path: [*:0]const u8, out: *CfsrEntry) callconv(.c) c_int;
extern fn cfsr_read(vol: *CfsrVolume, entry: *const CfsrEntry, dst: ?*anyopaque, limit: u32, out_size: ?*u32) callconv(.c) c_int;
extern fn gptr_find(read: *const fn (?*anyopaque, u64, u32, ?*anyopaque) callconv(.c) c_int, ctx: ?*anyopaque, type_guid: [*]const u8, out: *GptrPart) callconv(.c) c_int;
extern const gptr_type_otsos: [16]u8;

pub fn main() uefi.Status {
	serialInit();
	puts("[UEFI] OTSOS UEFI loader\n");
	boot() catch |err| {
		puts("[UEFI] panic: ");
		puts(@errorName(err));
		puts("\n");
		return .load_error;
	};
	return .success;
}

fn boot() !void {
	const bs = uefi.system_table.boot_services orelse return BootError.NoBootServices;

	const mmap_buf = try bs.allocatePool(.loader_data, MMAP_BUF_SIZE);
	const fb = try setupFramebuffer(bs);
	const mb2_addr = try allocLowPages(bs, MB2_INFO_CAP);

	var mb: Mb2Builder = undefined;
	mb2_builder_init(&mb, @ptrFromInt(mb2_addr), MB2_INFO_CAP);

	
	var kernel: CfsBoot = undefined;
	var pack: Bootpack = undefined;

	const own_disk = ownDiskHandle(bs);
	var installed = false;
	if (own_disk) |handle| {
		installed = mountRootOnDisk(bs, handle);
		if (installed) {
			puts("[UEFI] booting installed root on own device\n");
		}
	}
	if (installed) {
		kernel = try loadCfsKernel(bs);
		try loadCfsCmseed(bs, &mb);
	} else if (loadLive(bs, &mb, &pack)) |live| {
		kernel = live;
	} else |live_err| {
		switch (live_err) {
			BootError.NoFilesystem,
			BootError.NoDeviceHandle,
			BootError.FileReadFailed,
			BootError.BadBootpack,
			BootError.KernelNotFound,
			=> {},
			else => return live_err,
		}
		if (!mountInstalledRoot(bs, own_disk)) {
			puts("[UEFI] Kernel not found\n");
			return BootError.KernelNotFound;
		}
		installed = true;
		kernel = try loadCfsKernel(bs);
		try loadCfsCmseed(bs, &mb);
	}

	const mmap = try getMemoryMap(bs, mmap_buf);
	const mem = memoryInfo(mmap);
	const rsdp = try findAcpiRsdp();

	if (mb2_add_bootloader_name(&mb, "OTSOS UEFI bootloader") != 0) {
		return BootError.OutOfMemory;
	}
	if (mb2_add_basic_meminfo(&mb, mem.lower_kb, mem.upper_kb) != 0) {
		return BootError.OutOfMemory;
	}
	try addMmapTag(&mb, mmap);
	if (mb2_add_framebuffer(&mb, &fb) != 0) {
		return BootError.OutOfMemory;
	}
	if (mb2_add_acpi(&mb, rsdp.ptr, rsdp.size,
		if (rsdp.is_new) 1 else 0) != 0)
	{
		return BootError.OutOfMemory;
	}
	puts("[UEFI] ACPI RSDP ");
	puthex(@intCast(@intFromPtr(rsdp.ptr)));
	puts("\n");

	if (mb2_builder_finish(&mb) == 0) {
		return BootError.OutOfMemory;
	}

	puts("[UEFI] exit boot services\n");
	try exitBootServices(bs, mmap_buf);

	var entry: u64 = 0;
	var kernel_end: u64 = 0;
	if (elf64_load_kernel(@ptrFromInt(kernel.stage), kernel.size, &entry,
		&kernel_end) != 0)
	{
		puts("[UEFI] panic: KernelLoadFailed\n");
		uefi_halt();
	}
	puts("[UEFI] kernel entry ");
	puthex(@intCast(entry));
	puts("\n");

	uefi_jump32(@intCast(entry), MB2_BOOTLOADER_MAGIC, @intCast(mb2_addr));
}


var cfs_vol: CfsrVolume = undefined;
var cfs_ctx: BlockCtx = undefined;
var scan_ctx: BlockCtx = undefined;

var own_path_buf: [1024]u8 align(8) = @splat(0);

fn blockRead(ctx: ?*anyopaque, lba: u64, count: u32, dst: ?*anyopaque) callconv(.c) c_int {
	const c: *BlockCtx = @ptrCast(@alignCast(ctx orelse return -1));
	const out: [*]u8 = @ptrCast(dst orelse return -1);

	if (count == 0) {
		return -1;
	}

	if (c.io.media.block_size != CFSR_BLOCK_SIZE) {
		return -1;
	}
	const bytes: usize = @as(usize, count) * CFSR_BLOCK_SIZE;
	c.io.readBlocks(c.media_id, c.base_lba + lba, out[0..bytes]) catch {
		return -1;
	};
	return 0;
}

fn ownDiskHandle(bs: *BootServices) ?Handle {
	const loaded = (bs.handleProtocol(LoadedImage, uefi.handle) catch
		return null) orelse return null;
	const dev = loaded.device_handle orelse return null;
	const path = (bs.handleProtocol(DevicePath, dev) catch return null) orelse
		return null;

	
	const size = path.size();
	if (size > own_path_buf.len) {
		return null;
	}
	const src: [*]const u8 = @ptrCast(path);
	@memcpy(own_path_buf[0..size], src[0..size]);

	const copy: *DevicePath = @ptrCast(&own_path_buf);
	var node: *DevicePath = copy;
	var prev: ?*DevicePath = null;
	while (node.next()) |nxt| {
		prev = node;
		node = @constCast(nxt);
	}
	
	const last = prev orelse return null;
	last.type = .end;
	last.subtype = 0xff;
	last.length = 4;

	const found = (bs.locateDevicePath(copy, BlockIo) catch return null) orelse
		return null;
	return found[1];
}

fn mountRootOnDisk(bs: *BootServices, handle: Handle) bool {
	const io = (bs.handleProtocol(BlockIo, handle) catch return false) orelse
		return false;
	const media = io.media;
	if (!media.media_present or media.block_size != CFSR_BLOCK_SIZE) {
		return false;
	}

	scan_ctx = .{
		.io = io,
		.media_id = media.media_id,
		.base_lba = 0,
	};
	var part: GptrPart = undefined;
	if (gptr_find(blockRead, &scan_ctx, &gptr_type_otsos, &part) != 0) {
		return false;
	}
	cfs_ctx = .{
		.io = io,
		.media_id = media.media_id,
		.base_lba = 0,
	};
	if (cfsr_mount(&cfs_vol, blockRead, &cfs_ctx, part.first_lba,
		CFS_MAX_RUN) != 0)
	{
		return false;
	}
	puts("[UEFI] chainfs root at lba ");
	puthex(@truncate(part.first_lba));
	puts("\n");
	return true;
}


fn mountInstalledRoot(bs: *BootServices, skip: ?Handle) bool {
	const handles = (bs.locateHandleBuffer(.{ .by_protocol = &BlockIo.guid }) catch
		return false) orelse return false;

	for (handles) |handle| {
		if (skip) |own| {
			if (handle == own) {
				continue;
			}
		}
		const io = (bs.handleProtocol(BlockIo, handle) catch continue) orelse
			continue;
		const media = io.media;
		if (!media.media_present or media.block_size != CFSR_BLOCK_SIZE) {
			continue;
		}

		if (!media.logical_partition) {
			if (mountRootOnDisk(bs, handle)) {
				return true;
			}
			continue;
		}

		cfs_ctx = .{
			.io = io,
			.media_id = media.media_id,
			.base_lba = 0,
		};
		if (cfsr_mount(&cfs_vol, blockRead, &cfs_ctx, 0, CFS_MAX_RUN) == 0) {
			puts("[UEFI] chainfs root on partition handle\n");
			return true;
		}
	}
	return false;
}

fn loadLive(bs: *BootServices, mb: *Mb2Builder, pack: *Bootpack) !CfsBoot {
	const root = try openRoot(bs);
	const bootpack = try readBootpack(bs, root);
	bootpack_init(pack, @ptrFromInt(bootpack.addr), @intCast(bootpack.size));

	var image: BootpackFile = undefined;
	if (bootpack_find(pack, BOOTPACK_KERNEL_NAME, &image) != 0) {
		return BootError.KernelNotFound;
	}

	var mod_ctx = ModuleCtx{
		.mb = mb,
		.failed = false,
	};
	if (bootpack_foreach(pack, moduleCallback, &mod_ctx) != 0 or mod_ctx.failed) {
		return BootError.ModuleLoadFailed;
	}
	puts("[UEFI] live boot from own device\n");
	return .{
		.stage = @intFromPtr(image.data),
		.size = image.size,
	};
}

fn stagePages(bs: *BootServices, size: u32) !usize {
	const pages = (@as(usize, size) + 4095) / 4096;
	const max: [*]align(4096) Page = @ptrFromInt(LOW_MAX_ADDR);
	const mem = bs.allocatePages(.{ .max_address = max }, .loader_data, pages) catch {
		return BootError.OutOfMemory;
	};
	const addr = @intFromPtr(mem.ptr);

	if (addr < CFS_STAGE_MIN_ADDR) {
		return BootError.OutOfMemory;
	}
	return addr;
}

fn loadCfsKernel(bs: *BootServices) !CfsBoot {
	var file: CfsrEntry = undefined;

	if (cfsr_lookup(&cfs_vol, CFS_KERNEL_PATH, &file) != 0) {
		return BootError.KernelNotFound;
	}
	if (file.type != CFSR_TYPE_FILE or file.size == 0) {
		return BootError.KernelNotFound;
	}
	const addr = try stagePages(bs, file.size);
	var got: u32 = 0;
	if (cfsr_read(&cfs_vol, &file, @ptrFromInt(addr), file.size, &got) != 0 or
		got != file.size)
	{
		return BootError.FileReadFailed;
	}
	puts("[UEFI] chainfs kernel ");
	puthex(file.size);
	puts(" bytes\n");
	return .{
		.stage = addr,
		.size = file.size,
	};
}


fn loadCfsCmseed(bs: *BootServices, mb: *Mb2Builder) !void {
	var file: CfsrEntry = undefined;

	if (cfsr_lookup(&cfs_vol, CFS_CMSEED_PATH, &file) != 0) {
		return BootError.ModuleLoadFailed;
	}
	if (file.type != CFSR_TYPE_FILE or file.size == 0) {
		return BootError.ModuleLoadFailed;
	}

	const addr = try stagePages(bs, file.size);
	var got: u32 = 0;
	if (cfsr_read(&cfs_vol, &file, @ptrFromInt(addr), file.size,
		&got) != 0 or got != file.size)
	{
		return BootError.FileReadFailed;
	}
	if (addr + file.size > std.math.maxInt(u32)) {
		return BootError.ModuleLoadFailed;
	}
	if (mb2_add_module(mb, @intCast(addr), @intCast(addr + file.size),
		CFS_CMSEED_NAME) != 0)
	{
		return BootError.ModuleLoadFailed;
	}
	puts("[UEFI] module cmseed ");
	puthex(file.size);
	puts(" bytes\n");
}

fn openRoot(bs: *BootServices) !*File {
	const loaded = (try bs.handleProtocol(LoadedImage, uefi.handle)) orelse {
		return BootError.NoFilesystem;
	};
	const dev = loaded.device_handle orelse return BootError.NoDeviceHandle;
	const fs = (try bs.handleProtocol(SimpleFileSystem, dev)) orelse {
		return BootError.NoFilesystem;
	};
	return fs.openVolume() catch BootError.NoFilesystem;
}

fn readBootpack(bs: *BootServices, root: *File) !FileImage {
	const path = std.unicode.utf8ToUtf16LeStringLiteral("\\boot\\bootpack.tar");
	const file = root.open(path, .read, .{}) catch return BootError.FileReadFailed;
	defer file.close() catch {};

	const info_size = file.getInfoSize(.file) catch return BootError.FileReadFailed;
	const info_buf = try bs.allocatePool(.loader_data, info_size);
	defer bs.freePool(info_buf.ptr) catch {};
	const aligned: []align(@alignOf(File.Info.File)) u8 = @alignCast(info_buf);
	const info = file.getInfo(.file, aligned) catch return BootError.FileReadFailed;
	const size: usize = @intCast(info.file_size);
	if (size > BOOTPACK_MAX_SIZE) {
		return BootError.BootpackTooLarge;
	}

	const addr = try allocLowPages(bs, size);
	const dst = bytesAt(addr, size);
	var done: usize = 0;
	while (done < size) {
		const got = file.read(dst[done..]) catch return BootError.FileReadFailed;
		if (got == 0) {
			return BootError.FileReadFailed;
		}
		done += got;
	}

	puts("[UEFI] bootpack ");
	puthex(@intCast(size));
	puts(" bytes\n");
	return .{
		.addr = addr,
		.size = size,
	};
}

fn setupFramebuffer(bs: *BootServices) !Mb2Framebuffer {
	const gop = (try bs.locateProtocol(GraphicsOutput, null)) orelse {
		return BootError.GopUnavailable;
	};
	try pickVideoMode(gop);

	const mode = gop.mode;
	const info = mode.info;
	return .{
		.addr = mode.frame_buffer_base,
		.pitch = info.pixels_per_scan_line * 4,
		.width = info.horizontal_resolution,
		.height = info.vertical_resolution,
		.bpp = 32,
		.type = 1,
	};
}

fn pickVideoMode(gop: *GraphicsOutput) !void {
	var id: u32 = 0;
	while (id < gop.mode.max_mode) : (id += 1) {
		const info = gop.queryMode(id) catch continue;
		if (info.horizontal_resolution != 1024 or
			info.vertical_resolution != 768)
		{
			continue;
		}
		if (!isFramebufferMode(info.pixel_format)) {
			continue;
		}
		gop.setMode(id) catch continue;
		return;
	}
}

fn isFramebufferMode(format: GraphicsOutput.PixelFormat) bool {
	return switch (format) {
		.red_green_blue_reserved_8_bit_per_color,
		.blue_green_red_reserved_8_bit_per_color,
		.bit_mask,
		=> true,
		else => false,
	};
}

fn getMemoryMap(bs: *BootServices, buf: []align(8) u8) !MemoryMapSlice {
	const aligned: []align(@alignOf(uefi.tables.MemoryDescriptor)) u8 =
		@alignCast(buf);
	return bs.getMemoryMap(aligned) catch return BootError.OutOfMemory;
}

fn memoryInfo(mmap: MemoryMapSlice) struct { lower_kb: u32, upper_kb: u32 } {
	var it = mmap.iterator();
	var top: u64 = 0x1000000;
	while (it.next()) |desc| {
		if (!isUsableMemory(desc.type)) {
			continue;
		}
		const end = desc.physical_start + desc.number_of_pages * 4096;
		if (end > top) {
			top = end;
		}
	}
	const upper = if (top > 0x100000) (top - 0x100000) / 1024 else 64512;
	return .{
		.lower_kb = 640,
		.upper_kb = @intCast(upper),
	};
}

fn addMmapTag(mb: *Mb2Builder, mmap: MemoryMapSlice) !void {
	var entries: [MB2_MMAP_ENTRY_MAX]Mb2MmapEntry = undefined;
	var count: usize = 0;
	var it = mmap.iterator();

	while (it.next()) |desc| {
		if (count >= entries.len) {
			return BootError.MemoryMapTooBig;
		}
		entries[count] = .{
			.base_addr = desc.physical_start,
			.length = desc.number_of_pages * 4096,
			.type = mapMb2MemType(desc.type),
			.reserved = 0,
		};
		count += 1;
	}

	if (mb2_add_mmap_entries(mb, entries[0..count].ptr,
		@intCast(count)) != 0)
	{
		return BootError.OutOfMemory;
	}
}

fn mapMb2MemType(mem_type: MemoryType) u32 {
	return switch (mem_type) {
		.conventional_memory => 1,
		.loader_code,
		.loader_data,
		.boot_services_code,
		.boot_services_data,
		.runtime_services_code,
		.runtime_services_data,
		.memory_mapped_io,
		.memory_mapped_io_port_space,
		.pal_code,
		.persistent_memory,
		.unaccepted_memory,
		=> 2,
		.acpi_reclaim_memory => 3,
		.acpi_memory_nvs => 4,
		.unusable_memory => 5,
		else => 2,
	};
}

fn findAcpiRsdp() !AcpiRsdp {
	const table = uefi.system_table.configuration_table;
	const count = uefi.system_table.number_of_table_entries;
	var i: usize = 0;
	var old: ?AcpiRsdp = null;

	while (i < count) : (i += 1) {
		const entry = table[i];
		if (guidEq(entry.vendor_guid, ConfigurationTable.acpi_20_table_guid)) {
			return .{
				.ptr = @ptrCast(entry.vendor_table),
				.size = rsdpSize(@ptrCast(entry.vendor_table)),
				.is_new = true,
			};
		}
		if (guidEq(entry.vendor_guid, ConfigurationTable.acpi_10_table_guid)) {
			old = .{
				.ptr = @ptrCast(entry.vendor_table),
				.size = 20,
				.is_new = false,
			};
		}
	}

	return old orelse BootError.AcpiNotFound;
}

fn rsdpSize(rsdp: [*]const u8) u32 {
	var rev: u8 = 0;
	var size: u32 = 20;

	rev = rsdp[15];
	if (rev >= 2) {
		size = readLe32(rsdp + 20);
		if (size < 36) {
			size = 36;
		}
	}
	return size;
}

fn readLe32(ptr: [*]const u8) u32 {
	return @as(u32, ptr[0]) |
		(@as(u32, ptr[1]) << 8) |
		(@as(u32, ptr[2]) << 16) |
		(@as(u32, ptr[3]) << 24);
}

fn guidEq(a: Guid, b: Guid) bool {
	return a.time_low == b.time_low and
		a.time_mid == b.time_mid and
		a.time_high_and_version == b.time_high_and_version and
		a.clock_seq_high_and_reserved == b.clock_seq_high_and_reserved and
		a.clock_seq_low == b.clock_seq_low and
		std.mem.eql(u8, &a.node, &b.node);
}

fn isUsableMemory(mem_type: MemoryType) bool {
	return switch (mem_type) {
		.conventional_memory,
		.loader_code,
		.loader_data,
		.boot_services_code,
		.boot_services_data,
		=> true,
		else => false,
	};
}

fn exitBootServices(bs: *BootServices, buf: []align(8) u8) !void {
	while (true) {
		const mmap = try getMemoryMap(bs, buf);
		bs.exitBootServices(uefi.handle, mmap.info.key) catch |err| {
			if (err == error.InvalidParameter) {
				continue;
			}
			return err;
		};
		return;
	}
}

fn loadModule(file: *const BootpackFile, name: [*:0]const u8, ctx: *ModuleCtx) !void {
	if (file.size == 0) {
		return;
	}
	const start = @intFromPtr(file.data);
	const end = start + file.size;
	if (end > std.math.maxInt(u32)) {
		return BootError.ModuleLoadFailed;
	}

	if (mb2_add_module(ctx.mb, @intCast(start), @intCast(end), name) != 0) {
		return BootError.ModuleLoadFailed;
	}
	puts("[UEFI] module ");
	putsC(name);
	puts(" ");
	puthex(file.size);
	puts(" bytes\n");
}

fn moduleCallback(file: *const BootpackFile, arg: ?*anyopaque) callconv(.c) c_int {
	const ctx: *ModuleCtx = @ptrCast(@alignCast(arg.?));
	const name: [*:0]const u8 = @ptrCast(file.name);
	loadModule(file, name, ctx) catch {
		ctx.failed = true;
		return -1;
	};
	return 0;
}

fn allocLowPages(bs: *BootServices, size: usize) !usize {
	const pages = (size + 4095) / 4096;
	const max: [*]align(4096) Page = @ptrFromInt(LOW_MAX_ADDR);
	const mem = bs.allocatePages(.{ .max_address = max }, .loader_data, pages) catch {
		return BootError.OutOfMemory;
	};
	return @intFromPtr(mem.ptr);
}

fn bytesAt(addr: usize, size: usize) []u8 {
	const ptr: [*]u8 = @ptrFromInt(addr);
	return ptr[0..size];
}

fn cstrEq(str: [*c]const u8, comptime lit: []const u8) bool {
	var i: usize = 0;
	while (i < lit.len) : (i += 1) {
		if (str[i] != lit[i]) {
			return false;
		}
	}
	return str[lit.len] == 0;
}

fn serialInit() void {
	uefi_outb(COM1 + 1, 0x00);
	uefi_outb(COM1 + 3, 0x80);
	uefi_outb(COM1 + 0, 0x03);
	uefi_outb(COM1 + 1, 0x00);
	uefi_outb(COM1 + 3, 0x03);
	uefi_outb(COM1 + 2, 0xc7);
	uefi_outb(COM1 + 4, 0x0b);
}

fn serialPutc(c: u8) void {
	while ((uefi_inb(COM1 + 5) & 0x20) == 0) {}
	uefi_outb(COM1, c);
}

fn putc(c: u8) void {
	if (c == '\n') {
		putc('\r');
	}
	serialPutc(c);
}

fn puts(str: []const u8) void {
	for (str) |c| {
		putc(c);
	}
}

fn putsC(str: [*:0]const u8) void {
	var i: usize = 0;
	while (str[i] != 0) : (i += 1) {
		putc(str[i]);
	}
}

fn puthex(value: u32) void {
	const hex = "0123456789abcdef";
	puts("0x");
	var shift: i32 = 28;
	while (shift >= 0) : (shift -= 4) {
		const digit = (value >> @intCast(shift)) & 0xf;
		putc(hex[digit]);
	}
}
