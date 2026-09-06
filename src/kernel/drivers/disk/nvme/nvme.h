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

$define %type nvme_tracker_t as one command slot with a precreated DMA map
    and a PRP list page
$define %type nvme_queue_t as one submission and completion queue pair and
    its tracker pool
$define %type nvme_ns_t as one namespace registered as a block device
$define %type nvme_irq_t as one interrupt vector and the queue it services
$define %type nvme_ctrl_t as one NVMe controller and the tags, queues and
    namespaces it owns
$define %type nvme_done_t as one completion extracted from a CQ to be
    finished outside the queue lock

$const NVME_MAX_CONTROLLERS as controllers this driver will attach
$const NVME_MAX_NAMESPACES as namespaces one controller may register
$const NVME_MAX_IO_QUEUES as I/O queues one controller may run
$const NVME_MAX_VECTORS as interrupt vectors one controller may claim
$const NVME_ADMIN_QDEPTH as admin queue depth
$const NVME_IO_QDEPTH_DEFAULT as default I/O queue depth, overridable at
    runtime
$const NVME_IO_QDEPTH_MIN as smallest I/O queue depth still useful
$const NVME_IO_QDEPTH_MAX as largest I/O queue depth this driver will ask
    for
$const NVME_MAX_XFER_PAGES as largest I/O in host pages, sized so a single
    PRP list page can name every segment
$const NVME_PRP_ENTRIES_PER_PAGE as 64 bit PRP entries that fit in one page
$const NVME_TR_F_* as tracker flags
$const NVME_ADMIN_TIMEOUT_NS as budget for an admin command
$const NVME_SPIN_BUDGET as pause iterations that stand in for a deadline
    when the timecounter has not started
$const NVME_RESET_TIMEOUT_FLOOR_NS as shortest CSTS.RDY wait, used when
    CAP.TO reports zero
$const NVME_PCI_CLASS as Mass Storage class
$const NVME_PCI_SUBCLASS as NVM Express subclass
$const NVME_PCI_PROGIF as NVMHCI programming interface
$const NVME_BAR_MIN as smallest BAR0 this driver will map

*/

/* !SPACE!

$space %internal nvme_now_ns, nvme_deadline_ns, nvme_wait_expired
$space %internal nvme_reg32, nvme_reg32_set, nvme_reg64, nvme_reg64_set
$space %internal nvme_queue_init, nvme_queue_fini, nvme_queue_reset
$space %internal nvme_queue_service, nvme_queue_flush_inflight
$space %internal nvme_tracker_get, nvme_tracker_put
$space %internal nvme_admin_cmd, nvme_prp_build
$space %internal nvme_submit, nvme_timeout, nvme_intr
$space %internal nvme_ctrl_disable, nvme_ctrl_enable, nvme_ctrl_reset
$space %internal nvme_identify, nvme_ns_attach, nvme_ns_detach
$space %internal nvme_ioq_create, nvme_ioq_delete, nvme_queue_for_bio
$space %internal nvme_pci_probe, nvme_pci_remove
$space %none

*/

#ifndef KERNEL_DRIVERS_DISK_NVME_NVME_H
#define KERNEL_DRIVERS_DISK_NVME_NVME_H

#include <kernel/drivers/disk/bio.h>
#include <kernel/drivers/disk/disk.h>
#include <kernel/drivers/disk/nvme/nvmereg.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/mm/dma/dma.h>
#include <kernel/mm/vm/vm_page.h>
#include <kernel/pci/pci.h>
#include <kernel/sync/sync.h>
#include <mlibc/mlibc.h>

#define	NVME_MAX_CONTROLLERS	4
#define	NVME_MAX_NAMESPACES	8
#define	NVME_MAX_IO_QUEUES	8
#define	NVME_MAX_VECTORS	(NVME_MAX_IO_QUEUES + 1)

_Static_assert(NVME_MAX_VECTORS <= NEWBUS_MAX_MSI_ENTRIES,
    "NVMe wants more MSI-X entries than the bus will hand out");

#define	NVME_ADMIN_QDEPTH		32
#define	NVME_IO_QDEPTH_DEFAULT		64
#define	NVME_IO_QDEPTH_MIN		2
#define	NVME_IO_QDEPTH_MAX		256
#define	NVME_MAX_XFER_PAGES		512
#define	NVME_PRP_ENTRIES_PER_PAGE	(PAGE_SIZE / 8)
#define	NVME_TR_F_BUSY		0x01
#define	NVME_TR_F_COMPLETE	0x02
#define	NVME_TR_F_MAPPED	0x04
#define	NVME_ADMIN_TIMEOUT_NS		5000000000ULL
#define	NVME_RESET_TIMEOUT_FLOOR_NS	2000000000ULL
#define	NVME_SPIN_BUDGET		2000000000ULL
#define	NVME_PCI_CLASS		0x01
#define	NVME_PCI_SUBCLASS	0x08
#define	NVME_PCI_PROGIF		0x02
#define	NVME_BAR_MIN		0x2000

struct nvme_ctrl;
struct nvme_queue;

typedef struct nvme_tracker {
	dma_map_t		map;
	struct bio		*bio;
	struct nvme_queue	*queue;
	void			*prp_virt;
	u64			prp_phys;
	u64			submit_ns;
	u32			cdw0;
	u16			cid;
	u16			status;
	u8			flags;
	u8			opc;
	u16			pad;
} nvme_tracker_t;

typedef struct nvme_queue {
	spin_t			lock;
	struct nvme_ctrl	*ctrl;
	dma_mem_t		sq_mem;
	dma_mem_t		cq_mem;
	dma_mem_t		prp_mem;
	nvme_tracker_t		*trackers;
	u16			*free_cids;
	volatile u32		*sq_tdbl;
	volatile u32		*cq_hdbl;
	u32			qdepth;
	u32			sq_tail;
	u32			cq_head;
	u16			cq_vector;
	u16			qid;
	u8			phase;
	u8			created;
	u32			free_count;
	u64			nsubmit;
	u64			ncomplete;
	u64			fail_busy;
	u64			fail_prp;
} nvme_queue_t;

typedef struct nvme_ns {
	disk_t			disk;
	struct nvme_ctrl	*ctrl;
	u64			nsze;
	u32			nsid;
	u32			lba_size;
	u8			registered;
	u8			pad[7];
} nvme_ns_t;


typedef struct nvme_irq {
	struct nvme_ctrl	*ctrl;
	struct nvme_queue	*queue;
	resource_t		*res;
	resource_t		*intr_res;
	void			*cookie;
	u32			entry;
	u8			is_msi;
	u8			pad[3];
	u64			ncall;
} nvme_irq_t;

typedef struct nvme_ctrl {
	volatile u8		*regs;
	pci_device_t		*pci;
	device_t		nb_dev;
	dma_tag_t		tag;
	dma_tag_t		q_tag;
	dma_tag_t		prp_tag;
	dma_tag_t		xfer_tag;
	nvme_queue_t		admin_q;
	nvme_queue_t		io_q[NVME_MAX_IO_QUEUES];
	nvme_irq_t		irq[NVME_MAX_VECTORS];
	nvme_ns_t		ns[NVME_MAX_NAMESPACES];
	dma_mem_t		id_mem;
	u64			cap;
	u32			ns_count;
	u32			nioq;
	u32			nioq_max;
	u32			nirq;
	u32			mps;
	u32			dstrd;
	u32			max_xfer;
	u32			qdepth;
	u16			cntlid;
	u8			vwc;
	u8			irq_msi;
	u8			irq_shared;
	u8			resetting;
	u8			failed;
	u8			unit;
	char			model[NVME_MODEL_LEN + 1];
	char			serial[NVME_SERIAL_LEN + 1];
	u64			nreset;
	u64			nfail_status;
} nvme_ctrl_t;

typedef struct nvme_done {
	struct bio		*bio;
	int			status;
	u32			resid;
} nvme_done_t;

u64		nvme_now_ns(void);
u64		nvme_deadline_ns(u64 timeout_ns);
int		nvme_wait_expired(u64 deadline_ns, u64 *spins);
u32		nvme_reg32(const nvme_ctrl_t *ctrl, u32 off);
void		nvme_reg32_set(nvme_ctrl_t *ctrl, u32 off, u32 value);
u64		nvme_reg64(const nvme_ctrl_t *ctrl, u32 off);
void		nvme_reg64_set(nvme_ctrl_t *ctrl, u32 off, u64 value);
int		nvme_queue_init(nvme_ctrl_t *ctrl, nvme_queue_t *q,
		    u16 qid, u32 qdepth);
void		nvme_queue_fini(nvme_queue_t *q);
void		nvme_queue_reset(nvme_queue_t *q);
u32		nvme_queue_service(nvme_queue_t *q);
void		nvme_queue_flush_inflight(nvme_queue_t *q, int status);
nvme_tracker_t	*nvme_tracker_get(nvme_queue_t *q);
void		nvme_tracker_put(nvme_queue_t *q, nvme_tracker_t *tr);
int		nvme_prp_build(nvme_tracker_t *tr, u64 *prp1, u64 *prp2);
int		nvme_admin_cmd(nvme_ctrl_t *ctrl, nvme_sqe_t *sqe,
		    u32 *cdw0, u16 *status);
int		nvme_submit(disk_t *disk, bio_t *bio);
int		nvme_timeout(disk_t *disk, bio_t *bio);
int		nvme_intr(void *arg);
int		nvme_ctrl_disable(nvme_ctrl_t *ctrl);
int		nvme_ctrl_enable(nvme_ctrl_t *ctrl);
int		nvme_ctrl_reset(nvme_ctrl_t *ctrl);
int		nvme_identify(nvme_ctrl_t *ctrl);
int		nvme_ns_attach(nvme_ctrl_t *ctrl);
void		nvme_ns_detach(nvme_ctrl_t *ctrl);
int		nvme_ioq_create(nvme_ctrl_t *ctrl);
int		nvme_ioq_delete(nvme_ctrl_t *ctrl);
nvme_queue_t	*nvme_queue_for_bio(nvme_ctrl_t *ctrl);

#endif
