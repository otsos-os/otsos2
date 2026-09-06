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

$define %func nvme_ctrl_disable as function with args nvme_ctrl_t *
$define %func nvme_ctrl_enable as function with args nvme_ctrl_t *
$define %func nvme_ctrl_reset as function with args nvme_ctrl_t *
$define %func nvme_identify as function with args nvme_ctrl_t *
$define %func nvme_ioq_create as function with args nvme_ctrl_t *
$define %func nvme_ioq_delete as function with args nvme_ctrl_t *
$define %func nvme_ioq_create_one as function with args nvme_ctrl_t *,
    nvme_queue_t *
$define %func nvme_ioq_delete_one as function with args nvme_ctrl_t *,
    nvme_queue_t *
$define %func nvme_irq_alloc as function with args nvme_ctrl_t *
$define %func nvme_irq_free as procedure with args nvme_ctrl_t *
$define %func nvme_ioq_count_pick as function with args nvme_ctrl_t *
$define %func nvme_ns_attach as function with args nvme_ctrl_t *
$define %func nvme_ns_detach as procedure with args nvme_ctrl_t *

$const NVME_IOQ_ID_BASE as queue identifier of the first I/O queue pair
$const NVME_ADMIN_VECTOR as MSI-X entry the admin completion queue is bound to
$const NVME_MDTS_UNLIMITED as MDTS value meaning the controller states no
    transfer ceiling
$const NVME_DRIVER_NAME as name used for registry lookups and log lines

*/

/* !SPACE!

$space %internal nvme_wait_ready, nvme_ready_timeout_ns, nvme_id_string
$space %internal nvme_tags_create, nvme_tags_destroy, nvme_probe_geometry
$space %internal nvme_set_num_queues, nvme_ns_setup, nvme_qdepth_pick
$space %internal nvme_ioq_create_one, nvme_ioq_delete_one
$space %internal nvme_irq_alloc, nvme_irq_free, nvme_ioq_count_pick
$space %internal nvme_ctrl_free, nvme_pci_probe, nvme_pci_remove
$space %export nvme_ctrl_disable, nvme_ctrl_enable, nvme_ctrl_reset
$space %export nvme_identify, nvme_ioq_create, nvme_ioq_delete
$space %export nvme_ns_attach, nvme_ns_detach

*/

#include <kernel/drivers/disk/nvme/nvme.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/vm/pmap.h>
#include <kernel/pci/utils/bar.h>
#include <kernel/smp/smp.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	NVME_IOQ_ID_BASE	1
#define	NVME_ADMIN_VECTOR	0
#define	NVME_MDTS_UNLIMITED	0
#define	NVME_DRIVER_NAME	"nvme"

static nvme_ctrl_t	*nvme_ctrls[NVME_MAX_CONTROLLERS];

static const disk_ops_t nvme_disk_ops = {
	.submit		= nvme_submit,
	.timeout	= nvme_timeout,
};

static u64	nvme_ready_timeout_ns(const nvme_ctrl_t *ctrl);
static int	nvme_wait_ready(nvme_ctrl_t *ctrl, u32 want);
static void	nvme_id_string(char *out, const char *in, u32 len);
static int	nvme_tags_create(nvme_ctrl_t *ctrl);
static void	nvme_tags_destroy(nvme_ctrl_t *ctrl);
static int	nvme_probe_geometry(nvme_ctrl_t *ctrl);
static int	nvme_set_num_queues(nvme_ctrl_t *ctrl);
static int	nvme_ioq_create_one(nvme_ctrl_t *ctrl, nvme_queue_t *q);
static int	nvme_ioq_delete_one(nvme_ctrl_t *ctrl, nvme_queue_t *q);
static int	nvme_irq_alloc(nvme_ctrl_t *ctrl);
static void	nvme_irq_free(nvme_ctrl_t *ctrl);
static u32	nvme_ioq_count_pick(nvme_ctrl_t *ctrl);
static int	nvme_ns_setup(nvme_ctrl_t *ctrl, nvme_ns_t *ns, u32 nsid);
static void	nvme_ctrl_free(nvme_ctrl_t *ctrl);
static int	nvme_pci_probe(pci_device_t *pdev, const pci_match_t *match);
static void	nvme_pci_remove(pci_device_t *pdev);


static u64
nvme_ready_timeout_ns(const nvme_ctrl_t *ctrl)
{
	u64	units, ns;

	units = (ctrl->cap >> NVME_CAP_TO_SHIFT) & NVME_CAP_TO_MASK;
	ns = units * NVME_CAP_TO_UNIT_NS;
	if (ns < NVME_RESET_TIMEOUT_FLOOR_NS) {
		ns = NVME_RESET_TIMEOUT_FLOOR_NS;
	}
	return (ns);
}


static int
nvme_wait_ready(nvme_ctrl_t *ctrl, u32 want)
{
	u64	deadline, spins;
	u32	csts;

	deadline = nvme_deadline_ns(nvme_ready_timeout_ns(ctrl));
	spins = 0;
	for (;;) {
		csts = nvme_reg32(ctrl, NVME_REG_CSTS);
		if ((csts & NVME_CSTS_CFS) != 0) {
			drivers_log("[NVME] controller fatal status while "
			    "waiting for RDY=%u (csts=0x%x)\n", want, csts);
			return (-1);
		}
		if (((csts & NVME_CSTS_RDY) != 0 ? 1U : 0U) == want) {
			return (0);
		}
		if (nvme_wait_expired(deadline, &spins)) {
			drivers_log("[NVME] timeout waiting for RDY=%u "
			    "(csts=0x%x)\n", want, csts);
			return (-1);
		}
		__asm__ volatile("pause");
	}
}

int
nvme_ctrl_disable(nvme_ctrl_t *ctrl)
{
	u32	cc;

	cc = nvme_reg32(ctrl, NVME_REG_CC);
	if ((cc & NVME_CC_EN) != 0) {
		nvme_reg32_set(ctrl, NVME_REG_CC, cc & ~NVME_CC_EN);
	}
	return (nvme_wait_ready(ctrl, 0));
}

int
nvme_ctrl_enable(nvme_ctrl_t *ctrl)
{
	u32	cc, aqa, mps_shift;
	aqa = ((ctrl->admin_q.qdepth - 1) & NVME_AQA_QS_MASK) <<
	    NVME_AQA_ASQS_SHIFT;
	aqa |= ((ctrl->admin_q.qdepth - 1) & NVME_AQA_QS_MASK) <<
	    NVME_AQA_ACQS_SHIFT;
	nvme_reg32_set(ctrl, NVME_REG_AQA, aqa);
	nvme_reg64_set(ctrl, NVME_REG_ASQ, ctrl->admin_q.sq_mem.phys);
	nvme_reg64_set(ctrl, NVME_REG_ACQ, ctrl->admin_q.cq_mem.phys);
	mps_shift = 0;
	while ((1U << (mps_shift + NVME_MPS_SHIFT_BIAS)) < PAGE_SIZE) {
		mps_shift++;
	}
	cc = NVME_CC_EN;
	cc |= (NVME_CC_CSS_NVM & NVME_CC_CSS_MASK) << NVME_CC_CSS_SHIFT;
	cc |= (mps_shift & NVME_CC_MPS_MASK) << NVME_CC_MPS_SHIFT;
	cc |= (NVME_CC_AMS_RR & NVME_CC_AMS_MASK) << NVME_CC_AMS_SHIFT;
	cc |= (u32)NVME_SQES_SHIFT << NVME_CC_IOSQES_SHIFT;
	cc |= (u32)NVME_CQES_SHIFT << NVME_CC_IOCQES_SHIFT;
	nvme_reg32_set(ctrl, NVME_REG_CC, cc);

	if (nvme_wait_ready(ctrl, 1) != 0) {
		return (-1);
	}
	ctrl->admin_q.created = 1;
	return (0);
}


int
nvme_ctrl_reset(nvme_ctrl_t *ctrl)
{
	u32	i;
	int	error;

	if (ctrl == NULL) {
		return (-1);
	}
	if (__atomic_exchange_n(&ctrl->resetting, 1, __ATOMIC_ACQ_REL) != 0) {
		return (0);
	}
	ctrl->nreset++;

	if (ctrl->irq_msi != 0) {
		for (i = 0; i < ctrl->nirq; i++) {
			(void)pci_msix_mask(ctrl->pci, (u16)ctrl->irq[i].entry,
			    1);
		}
	} else {
		nvme_reg32_set(ctrl, NVME_REG_INTMS, 0xFFFFFFFFU);
	}

	error = nvme_ctrl_disable(ctrl);

	for (i = 0; i < NVME_MAX_IO_QUEUES; i++) {
		ctrl->io_q[i].created = 0;
	}
	ctrl->admin_q.created = 0;

	for (i = 0; i < NVME_MAX_IO_QUEUES; i++) {
		nvme_queue_flush_inflight(&ctrl->io_q[i], BIO_STATUS_TIMEOUT);
	}
	nvme_queue_flush_inflight(&ctrl->admin_q, BIO_STATUS_TIMEOUT);
	for (i = 0; i < NVME_MAX_IO_QUEUES; i++) {
		nvme_queue_reset(&ctrl->io_q[i]);
	}
	nvme_queue_reset(&ctrl->admin_q);

	if (error == 0) {
		error = nvme_ctrl_enable(ctrl);
	}
	if (error == 0) {
		ctrl->nioq = ctrl->nioq_max;
		error = nvme_ioq_create(ctrl);
	}

	if (ctrl->irq_msi != 0) {
		for (i = 0; i < ctrl->nirq; i++) {
			(void)pci_msix_mask(ctrl->pci, (u16)ctrl->irq[i].entry,
			    0);
		}
	} else {
		nvme_reg32_set(ctrl, NVME_REG_INTMC, 0xFFFFFFFFU);
	}

	if (error != 0) {

		ctrl->failed = 1;
		drivers_log("[NVME] controller %u reset failed, disk offline\n",
		    ctrl->unit);
	} else {
		drivers_log("[NVME] controller %u reset complete\n",
		    ctrl->unit);
	}
	__atomic_store_n(&ctrl->resetting, 0, __ATOMIC_RELEASE);
	return (error);
}


static void
nvme_id_string(char *out, const char *in, u32 len)
{
	u32	i, end;

	end = 0;
	for (i = 0; i < len; i++) {
		out[i] = in[i];
		if (in[i] != ' ' && in[i] != '\0') {
			end = i + 1;
		}
	}
	out[end] = '\0';
}


static int
nvme_tags_create(nvme_ctrl_t *ctrl)
{
	u64	q_bytes, prp_bytes;
	if (dma_tag_create(bus_get_dma_tag(ctrl->nb_dev), 1, DMA_BOUNDARY_NONE,
	    0, DMA_HIGHADDR_ANY, 0, 0, DMA_SEGSZ_MAX, 0, "nvme",
	    &ctrl->tag) != 0) {
		return (-1);
	}
	q_bytes = (u64)NVME_IO_QDEPTH_MAX * NVME_SQE_SIZE;
	if (dma_tag_create(ctrl->tag, PAGE_SIZE, DMA_BOUNDARY_NONE, 0,
	    DMA_HIGHADDR_ANY, q_bytes, 1, DMA_SEGSZ_MAX, 0, "nvme-queue",
	    &ctrl->q_tag) != 0) {
		goto fail;
	}
	prp_bytes = (u64)NVME_IO_QDEPTH_MAX * PAGE_SIZE;
	if (dma_tag_create(ctrl->tag, PAGE_SIZE, DMA_BOUNDARY_NONE, 0,
	    DMA_HIGHADDR_ANY, prp_bytes, 1, DMA_SEGSZ_MAX, 0, "nvme-prp",
	    &ctrl->prp_tag) != 0) {
		goto fail;
	}

	if (dma_tag_create(ctrl->tag, 1, PAGE_SIZE, 0, DMA_HIGHADDR_ANY,
	    (u64)NVME_MAX_XFER_PAGES * PAGE_SIZE, NVME_MAX_XFER_PAGES + 1,
	    PAGE_SIZE, 0, "nvme-xfer", &ctrl->xfer_tag) != 0) {
		goto fail;
	}
	return (0);
fail:
	nvme_tags_destroy(ctrl);
	return (-1);
}

static void
nvme_tags_destroy(nvme_ctrl_t *ctrl)
{
	if (ctrl->xfer_tag != NULL) {
		dma_tag_destroy(ctrl->xfer_tag);
		ctrl->xfer_tag = NULL;
	}
	if (ctrl->prp_tag != NULL) {
		dma_tag_destroy(ctrl->prp_tag);
		ctrl->prp_tag = NULL;
	}
	if (ctrl->q_tag != NULL) {
		dma_tag_destroy(ctrl->q_tag);
		ctrl->q_tag = NULL;
	}
	if (ctrl->tag != NULL) {
		dma_tag_destroy(ctrl->tag);
		ctrl->tag = NULL;
	}
}


static int
nvme_probe_geometry(nvme_ctrl_t *ctrl)
{
	u64	mpsmin, mpsmax, css;
	u32	mqes;

	ctrl->cap = nvme_reg64(ctrl, NVME_REG_CAP);
	ctrl->dstrd = (u32)((ctrl->cap >> NVME_CAP_DSTRD_SHIFT) &
	    NVME_CAP_DSTRD_MASK);
	ctrl->mps = PAGE_SIZE;

	css = (ctrl->cap >> NVME_CAP_CSS_SHIFT) & NVME_CAP_CSS_MASK;
	if ((css & NVME_CAP_CSS_NVM) == 0) {
		drivers_log("[NVME] controller does not support the NVM "
		    "command set (css=0x%llx)\n", (unsigned long long)css);
		return (-1);
	}

	mpsmin = (ctrl->cap >> NVME_CAP_MPSMIN_SHIFT) & NVME_CAP_MPS_MASK;
	mpsmax = (ctrl->cap >> NVME_CAP_MPSMAX_SHIFT) & NVME_CAP_MPS_MASK;
	if (PAGE_SIZE < (1ULL << (mpsmin + NVME_MPS_SHIFT_BIAS)) ||
	    PAGE_SIZE > (1ULL << (mpsmax + NVME_MPS_SHIFT_BIAS))) {
		drivers_log("[NVME] host page %u outside controller range "
		    "[%llu,%llu]\n", (u32)PAGE_SIZE,
		    (unsigned long long)(1ULL << (mpsmin +
		    NVME_MPS_SHIFT_BIAS)),
		    (unsigned long long)(1ULL << (mpsmax +
		    NVME_MPS_SHIFT_BIAS)));
		return (-1);
	}

	mqes = (u32)(ctrl->cap & NVME_CAP_MQES_MASK) + 1;
	ctrl->qdepth = newbus_config_driver_get_u32(NVME_DRIVER_NAME,
	    "IoQueueDepth", NVME_IO_QDEPTH_DEFAULT);
	if (ctrl->qdepth > mqes) {
		ctrl->qdepth = mqes;
	}
	if (ctrl->qdepth > NVME_IO_QDEPTH_MAX) {
		ctrl->qdepth = NVME_IO_QDEPTH_MAX;
	}
	if (ctrl->qdepth < NVME_IO_QDEPTH_MIN) {
		ctrl->qdepth = NVME_IO_QDEPTH_MIN;
	}
	return (0);
}

int
nvme_identify(nvme_ctrl_t *ctrl)
{
	const nvme_id_ctrl_t	*id;
	nvme_sqe_t		sqe;
	u64			max_xfer;

	memset(&sqe, 0, sizeof(sqe));
	sqe.opc = NVME_ADMIN_IDENTIFY;
	sqe.prp1 = ctrl->id_mem.phys;
	sqe.cdw10 = NVME_CNS_CONTROLLER;
	memset(ctrl->id_mem.virt, 0, NVME_ID_DATA_SIZE);
	if (nvme_admin_cmd(ctrl, &sqe, NULL, NULL) != 0) {
		return (-1);
	}

	id = ctrl->id_mem.virt;
	nvme_id_string(ctrl->serial, id->sn, NVME_SERIAL_LEN);
	nvme_id_string(ctrl->model, id->mn, NVME_MODEL_LEN);
	ctrl->cntlid = id->cntlid;
	ctrl->vwc = (u8)(id->vwc & NVME_ID_CTRL_VWC_PRESENT);
	ctrl->ns_count = id->nn;
	max_xfer = (u64)NVME_MAX_XFER_PAGES * PAGE_SIZE;
	if (id->mdts != NVME_MDTS_UNLIMITED) {
		if (id->mdts >= 32) {
			drivers_log("[NVME] ignoring implausible MDTS %u\n",
			    id->mdts);
		} else if (((u64)ctrl->mps << id->mdts) < max_xfer) {
			max_xfer = (u64)ctrl->mps << id->mdts;
		}
	}
	ctrl->max_xfer = (u32)max_xfer;

	drivers_log("[NVME] nvme%u: %s sn=%s cntlid=%u ns=%u mdts=%u "
	    "maxio=%uKiB vwc=%u qdepth=%u\n", ctrl->unit, ctrl->model,
	    ctrl->serial, ctrl->cntlid, ctrl->ns_count, id->mdts,
	    ctrl->max_xfer / 1024U, ctrl->vwc, ctrl->qdepth);
	return (0);
}


static int
nvme_set_num_queues(nvme_ctrl_t *ctrl)
{
	nvme_sqe_t	sqe;
	u32		cdw0, nsqa, ncqa, granted;

	memset(&sqe, 0, sizeof(sqe));
	sqe.opc = NVME_ADMIN_SET_FEATURES;
	sqe.cdw10 = NVME_FEAT_NUM_QUEUES;
	sqe.cdw11 = ((ctrl->nioq - 1U) << 16) | (ctrl->nioq - 1U);
	cdw0 = 0;
	if (nvme_admin_cmd(ctrl, &sqe, &cdw0, NULL) != 0) {
		return (-1);
	}
	nsqa = (cdw0 & 0xFFFFU) + 1U;
	ncqa = ((cdw0 >> 16) & 0xFFFFU) + 1U;
	if (nsqa < 1 || ncqa < 1) {
		drivers_log("[NVME] controller granted no I/O queues "
		    "(cdw0=0x%x)\n", cdw0);
		return (-1);
	}

	granted = nsqa < ncqa ? nsqa : ncqa;
	if (granted < ctrl->nioq) {
		drivers_log("[NVME] nvme%u: asked %u I/O queues, granted %u\n",
		    ctrl->unit, ctrl->nioq, granted);
		ctrl->nioq = granted;
	}
	return (0);
}


static int
nvme_ioq_create_one(nvme_ctrl_t *ctrl, nvme_queue_t *q)
{
	nvme_sqe_t	sqe;

	memset(&sqe, 0, sizeof(sqe));
	sqe.opc = NVME_ADMIN_CREATE_IO_CQ;
	sqe.prp1 = q->cq_mem.phys;
	sqe.cdw10 = ((q->qdepth - 1) << 16) | q->qid;
	sqe.cdw11 = ((u32)q->cq_vector << 16) | NVME_CQ_IEN | NVME_CQ_PC;
	if (nvme_admin_cmd(ctrl, &sqe, NULL, NULL) != 0) {
		return (-1);
	}

	memset(&sqe, 0, sizeof(sqe));
	sqe.opc = NVME_ADMIN_CREATE_IO_SQ;
	sqe.prp1 = q->sq_mem.phys;
	sqe.cdw10 = ((q->qdepth - 1) << 16) | q->qid;
	sqe.cdw11 = ((u32)q->qid << 16) | NVME_SQ_PC;
	if (nvme_admin_cmd(ctrl, &sqe, NULL, NULL) != 0) {
		memset(&sqe, 0, sizeof(sqe));
		sqe.opc = NVME_ADMIN_DELETE_IO_CQ;
		sqe.cdw10 = q->qid;
		(void)nvme_admin_cmd(ctrl, &sqe, NULL, NULL);
		return (-1);
	}
	q->created = 1;
	return (0);
}

static int
nvme_ioq_delete_one(nvme_ctrl_t *ctrl, nvme_queue_t *q)
{
	nvme_sqe_t	sqe;
	int		error;

	if (q->created == 0) {
		return (0);
	}
	memset(&sqe, 0, sizeof(sqe));
	sqe.opc = NVME_ADMIN_DELETE_IO_SQ;
	sqe.cdw10 = q->qid;
	error = nvme_admin_cmd(ctrl, &sqe, NULL, NULL);

	memset(&sqe, 0, sizeof(sqe));
	sqe.opc = NVME_ADMIN_DELETE_IO_CQ;
	sqe.cdw10 = q->qid;
	if (nvme_admin_cmd(ctrl, &sqe, NULL, NULL) != 0) {
		error = -1;
	}
	q->created = 0;
	return (error);
}

int
nvme_ioq_create(nvme_ctrl_t *ctrl)
{
	u32	i;

	if (nvme_set_num_queues(ctrl) != 0) {
		return (-1);
	}
	for (i = 0; i < ctrl->nioq; i++) {
		if (nvme_ioq_create_one(ctrl, &ctrl->io_q[i]) == 0) {
			continue;
		}
		if (i == 0) {
			return (-1);
		}
		drivers_log("[NVME] nvme%u: queue %u creation failed, "
		    "running with %u\n", ctrl->unit, i, i);
		ctrl->nioq = i;
		break;
	}
	return (0);
}

int
nvme_ioq_delete(nvme_ctrl_t *ctrl)
{
	u32	i;
	int	error;

	error = 0;
	for (i = ctrl->nioq; i > 0; i--) {
		if (nvme_ioq_delete_one(ctrl, &ctrl->io_q[i - 1]) != 0) {
			error = -1;
		}
	}
	return (error);
}


static int
nvme_ns_setup(nvme_ctrl_t *ctrl, nvme_ns_t *ns, u32 nsid)
{
	const nvme_id_ns_t	*id;
	nvme_sqe_t		sqe;
	u32			lbaf, lbads, ms, index;

	memset(&sqe, 0, sizeof(sqe));
	sqe.opc = NVME_ADMIN_IDENTIFY;
	sqe.nsid = nsid;
	sqe.prp1 = ctrl->id_mem.phys;
	sqe.cdw10 = NVME_CNS_NAMESPACE;
	memset(ctrl->id_mem.virt, 0, NVME_ID_DATA_SIZE);
	if (nvme_admin_cmd(ctrl, &sqe, NULL, NULL) != 0) {
		return (-1);
	}

	id = ctrl->id_mem.virt;
	if (id->nsze == 0) {
		return (-1);
	}
	index = id->flbas & NVME_NS_FLBAS_INDEX_MASK;
	if (index >= NVME_NS_LBAF_COUNT || index > id->nlbaf) {
		drivers_log("[NVME] nsid %u: FLBAS index %u out of range\n",
		    nsid, index);
		return (-1);
	}
	lbaf = id->lbaf[index];
	ms = lbaf & NVME_NS_LBAF_MS_MASK;
	lbads = (lbaf >> NVME_NS_LBAF_LBADS_SHIFT) & NVME_NS_LBAF_LBADS_MASK;
	if (ms != 0) {
		drivers_log("[NVME] nsid %u: %u byte metadata per block is "
		    "unsupported, skipping\n", nsid, ms);
		return (-1);
	}
	if (lbads < 9 || lbads > 20) {
		drivers_log("[NVME] nsid %u: implausible LBA size 2^%u\n",
		    nsid, lbads);
		return (-1);
	}

	memset(ns, 0, sizeof(*ns));
	ns->ctrl = ctrl;
	ns->nsid = nsid;
	ns->nsze = id->nsze;
	ns->lba_size = 1U << lbads;

	snprintf(ns->disk.name, sizeof(ns->disk.name), "nvme%un%u",
	    ctrl->unit, nsid);
	ns->disk.ops = &nvme_disk_ops;
	ns->disk.private_data = ns;
	ns->disk.total_sectors = ns->nsze;
	ns->disk.sector_size = ns->lba_size;
	ns->disk.type = DISK_TYPE_NVME;
	ns->disk.flags = ctrl->vwc != 0 ? 0 : DISK_F_NO_FLUSH;
	ns->disk.max_io_sectors = ctrl->max_xfer / ns->lba_size;
	if (ns->disk.max_io_sectors > 0x10000U) {
		ns->disk.max_io_sectors = 0x10000U;
	}
	if (ns->disk.max_io_sectors == 0) {
		drivers_log("[NVME] nsid %u: %u byte blocks exceed the %u byte "
		    "transfer ceiling\n", nsid, ns->lba_size, ctrl->max_xfer);
		return (-1);
	}
	return (0);
}

int
nvme_ns_attach(nvme_ctrl_t *ctrl)
{
	const u32	*list;
	nvme_sqe_t	sqe;
	u32		i, nsid, attached;

	memset(&sqe, 0, sizeof(sqe));
	sqe.opc = NVME_ADMIN_IDENTIFY;
	sqe.nsid = 0;
	sqe.prp1 = ctrl->id_mem.phys;
	sqe.cdw10 = NVME_CNS_ACTIVE_NSLIST;
	memset(ctrl->id_mem.virt, 0, NVME_ID_DATA_SIZE);
	if (nvme_admin_cmd(ctrl, &sqe, NULL, NULL) != 0) {
		return (-1);
	}

	list = ctrl->id_mem.virt;
	attached = 0;

	for (i = 0; i < NVME_NSLIST_MAX && attached < NVME_MAX_NAMESPACES;
	    i++) {
		nsid = list[i];
		if (nsid == 0) {
			break;
		}
		if (nvme_ns_setup(ctrl, &ctrl->ns[attached], nsid) != 0) {
			continue;
		}
		if (disk_register(&ctrl->ns[attached].disk) != 0) {
			drivers_log("[NVME] nsid %u: disk_register failed\n",
			    nsid);
			continue;
		}
		ctrl->ns[attached].registered = 1;
		drivers_log("[NVME] %s: %llu blocks of %u bytes (%llu MiB), "
		    "maxio=%u blocks\n", ctrl->ns[attached].disk.name,
		    (unsigned long long)ctrl->ns[attached].nsze,
		    ctrl->ns[attached].lba_size,
		    (unsigned long long)(disk_capacity_bytes(
		    &ctrl->ns[attached].disk) >> 20),
		    ctrl->ns[attached].disk.max_io_sectors);
		attached++;
	}
	if (attached == 0) {
		drivers_log("[NVME] nvme%u: no usable namespace\n", ctrl->unit);
		return (-1);
	}
	return (0);
}

void
nvme_ns_detach(nvme_ctrl_t *ctrl)
{
	u32	i;

	for (i = 0; i < NVME_MAX_NAMESPACES; i++) {
		if (ctrl->ns[i].registered == 0) {
			continue;
		}
		(void)disk_unregister(&ctrl->ns[i].disk);
		ctrl->ns[i].registered = 0;
	}
}

static void
nvme_ctrl_free(nvme_ctrl_t *ctrl)
{
	u32	i;

	if (ctrl == NULL) {
		return;
	}
	for (i = 0; i < NVME_MAX_IO_QUEUES; i++) {
		nvme_queue_fini(&ctrl->io_q[i]);
	}
	nvme_queue_fini(&ctrl->admin_q);
	dma_mem_free(&ctrl->id_mem);
	nvme_tags_destroy(ctrl);
	kmem_free(ctrl);
}


static u32
nvme_ioq_count_pick(nvme_ctrl_t *ctrl)
{
	u32	want;
	int	cpus;

	(void)ctrl;
	cpus = smp_cpu_count();
	want = cpus > 0 ? (u32)cpus : 1U;
	want = newbus_config_driver_get_u32(NVME_DRIVER_NAME, "IoQueues", want);
	if (want < 1) {
		want = 1;
	}
	if (want > NVME_MAX_IO_QUEUES) {
		want = NVME_MAX_IO_QUEUES;
	}
	return (want);
}


static int
nvme_irq_alloc(nvme_ctrl_t *ctrl)
{
	nvme_irq_t	*irq;
	u32		want, i;
	int		rid;

	want = ctrl->nioq + 1U;
	if (want > NVME_MAX_VECTORS) {
		want = NVME_MAX_VECTORS;
	}
	for (i = 0; i < want; i++) {
		irq = &ctrl->irq[i];
		irq->ctrl = ctrl;
		rid = 0;
		irq->res = bus_alloc_resource_any(ctrl->nb_dev, SYS_RES_IRQ,
		    &rid, RF_ACTIVE);
		if (irq->res == NULL) {
			break;
		}

		if (i > 0 && (irq->res->flags & RF_IRQ_MSI) == 0) {
			bus_release_resource(ctrl->nb_dev, SYS_RES_IRQ,
			    irq->res->rid, irq->res);
			irq->res = NULL;
			break;
		}
		irq->entry = (u32)irq->res->start;
		irq->queue = NULL;
		if (bus_setup_intr(ctrl->nb_dev, irq->res, nvme_intr, irq,
		    &irq->cookie) != 0) {
			bus_release_resource(ctrl->nb_dev, SYS_RES_IRQ,
			    irq->res->rid, irq->res);
			irq->res = NULL;
			break;
		}
		irq->intr_res = bus_intr_resource(irq->cookie);
		if (irq->intr_res == NULL) {
			irq->intr_res = irq->res;
		}
		irq->is_msi = (u8)(bus_intr_is_msi(irq->cookie) != 0);
		ctrl->nirq = i + 1U;

		if (irq->is_msi == 0) {
			break;
		}
	}
	if (ctrl->nirq == 0) {
		return (-1);
	}
	ctrl->irq_msi = ctrl->irq[0].is_msi;

	if (ctrl->nirq == 1) {
		ctrl->irq_shared = 1;
		for (i = 0; i < NVME_MAX_IO_QUEUES; i++) {
			ctrl->io_q[i].cq_vector = NVME_ADMIN_VECTOR;
		}
		ctrl->nioq_max = ctrl->nioq;
		return (0);
	}

	if (ctrl->nirq < ctrl->nioq + 1U) {
		drivers_log("[NVME] nvme%u: %u vectors for %u queues, "
		    "running %u\n", ctrl->unit, ctrl->nirq, ctrl->nioq,
		    ctrl->nirq - 1U);
		ctrl->nioq = ctrl->nirq - 1U;
	}
	ctrl->irq_shared = 0;
	ctrl->irq[0].queue = &ctrl->admin_q;
	for (i = 0; i < ctrl->nioq; i++) {
		ctrl->irq[i + 1U].queue = &ctrl->io_q[i];
		ctrl->io_q[i].cq_vector = (u16)ctrl->irq[i + 1U].entry;
	}
	ctrl->nioq_max = ctrl->nioq;
	return (0);
}

static void
nvme_irq_free(nvme_ctrl_t *ctrl)
{
	u32	i;

	for (i = 0; i < ctrl->nirq; i++) {
		if (ctrl->irq[i].cookie != NULL) {
			bus_teardown_intr(ctrl->nb_dev,
			    ctrl->irq[i].intr_res, ctrl->irq[i].cookie);
			ctrl->irq[i].cookie = NULL;
			ctrl->irq[i].intr_res = NULL;
		}
		if (ctrl->irq[i].res != NULL) {
			bus_release_resource(ctrl->nb_dev, SYS_RES_IRQ,
			    ctrl->irq[i].res->rid, ctrl->irq[i].res);
			ctrl->irq[i].res = NULL;
		}
	}
	ctrl->nirq = 0;
}

static int
nvme_pci_probe(pci_device_t *pdev, const pci_match_t *match)
{
	nvme_ctrl_t	*ctrl;
	pci_bar_t	bar;
	u64		dbl_span;
	u32		qi;
	int		index;

	(void)match;

	if (pci_read_bar(pdev, 0, &bar) != 0 || bar.is_io || bar.base == 0 ||
	    bar.size < NVME_BAR_MIN) {
		return (-1);
	}
	for (index = 0; index < NVME_MAX_CONTROLLERS; index++) {
		if (nvme_ctrls[index] == NULL) {
			break;
		}
	}
	if (index == NVME_MAX_CONTROLLERS) {
		return (-1);
	}
	ctrl = kmem_calloc(1, sizeof(*ctrl));
	if (ctrl == NULL) {
		return (-1);
	}
	ctrl->pci = pdev;
	ctrl->nb_dev = pdev->nb_device;
	ctrl->unit = (u8)index;
	pci_enable_memory_space(pdev);
	pci_enable_bus_mastering(pdev);
	ctrl->regs = pmap_map_mmio(bar.base, bar.size);
	if (ctrl->regs == NULL) {
		kmem_free(ctrl);
		return (-1);
	}
	if (nvme_probe_geometry(ctrl) != 0) {
		kmem_free(ctrl);
		return (-1);
	}
	ctrl->nioq = nvme_ioq_count_pick(ctrl);

	for (;;) {
		dbl_span = NVME_DBL_BASE +
		    ((((u64)NVME_IOQ_ID_BASE + ctrl->nioq - 1ULL) * 2ULL) +
		    2ULL) * (4ULL << ctrl->dstrd);
		if (dbl_span <= bar.size) {
			break;
		}
		if (ctrl->nioq <= 1) {
			drivers_log("[NVME] BAR0 of %llu bytes cannot hold "
			    "doorbells up to %llu (dstrd=%u)\n",
			    (unsigned long long)bar.size,
			    (unsigned long long)dbl_span, ctrl->dstrd);
			kmem_free(ctrl);
			return (-1);
		}
		ctrl->nioq--;
	}
	if (nvme_tags_create(ctrl) != 0) {
		kmem_free(ctrl);
		return (-1);
	}
	if (dma_mem_alloc(ctrl->prp_tag, NVME_ID_DATA_SIZE, DMA_F_NOWAIT,
	    &ctrl->id_mem) != 0) {
		nvme_ctrl_free(ctrl);
		return (-1);
	}
	if (nvme_queue_init(ctrl, &ctrl->admin_q, 0, NVME_ADMIN_QDEPTH) != 0) {
		nvme_ctrl_free(ctrl);
		return (-1);
	}
	for (qi = 0; qi < ctrl->nioq; qi++) {
		if (nvme_queue_init(ctrl, &ctrl->io_q[qi],
		    (u16)(NVME_IOQ_ID_BASE + qi), ctrl->qdepth) == 0) {
			continue;
		}

		if (qi == 0) {
			nvme_ctrl_free(ctrl);
			return (-1);
		}
		drivers_log("[NVME] nvme%u: queue %u alloc failed, "
		    "running with %u\n", ctrl->unit, qi, qi);
		ctrl->nioq = qi;
		break;
	}

	if (nvme_irq_alloc(ctrl) != 0) {
		nvme_irq_free(ctrl);
		nvme_ctrl_free(ctrl);
		return (-1);
	}

	if (nvme_ctrl_disable(ctrl) != 0 || nvme_ctrl_enable(ctrl) != 0 ||
	    nvme_identify(ctrl) != 0 || nvme_ioq_create(ctrl) != 0 ||
	    nvme_ns_attach(ctrl) != 0) {
		nvme_ns_detach(ctrl);
		(void)nvme_ioq_delete(ctrl);
		(void)nvme_ctrl_disable(ctrl);
		nvme_irq_free(ctrl);
		nvme_ctrl_free(ctrl);
		return (-1);
	}

	pdev->driver_data = ctrl;
	nvme_ctrls[index] = ctrl;
	drivers_log("[NVME] nvme%u attached: irq=%s vectors=%u ioq=%u "
	    "dstrd=%u\n", ctrl->unit,
	    ctrl->irq_msi != 0 ? (ctrl->irq_shared != 0 ? "msi-shared" : "msi") :
	    "legacy", ctrl->nirq, ctrl->nioq, ctrl->dstrd);
	return (0);
}

static void
nvme_pci_remove(pci_device_t *pdev)
{
	nvme_ctrl_t	*ctrl;
	u32		qi;
	int		index;

	ctrl = pdev->driver_data;
	if (ctrl == NULL) {
		return;
	}

	nvme_ns_detach(ctrl);
	(void)nvme_ioq_delete(ctrl);
	(void)nvme_ctrl_disable(ctrl);
	nvme_irq_free(ctrl);
	for (qi = 0; qi < NVME_MAX_IO_QUEUES; qi++) {
		nvme_queue_flush_inflight(&ctrl->io_q[qi], BIO_STATUS_NODEV);
	}
	nvme_queue_flush_inflight(&ctrl->admin_q, BIO_STATUS_NODEV);
	for (index = 0; index < NVME_MAX_CONTROLLERS; index++) {
		if (nvme_ctrls[index] == ctrl) {
			nvme_ctrls[index] = NULL;
		}
	}
	pdev->driver_data = NULL;
	nvme_ctrl_free(ctrl);
}

static const pci_match_t nvme_matches[] = {
	{ PCI_ANY_ID, PCI_ANY_ID, NVME_PCI_CLASS, NVME_PCI_SUBCLASS,
	    NVME_PCI_PROGIF },
};

static pci_driver_t nvme_pci_driver = {
	.name		= NVME_DRIVER_NAME,
	.matches	= nvme_matches,
	.match_count	= 1,
	.probe		= nvme_pci_probe,
	.remove		= nvme_pci_remove,
};

static devclass_t nvme_devclass = {
	.name		= NVME_DRIVER_NAME,
	.maxunit	= NVME_MAX_CONTROLLERS,
};

PCI_DRIVER_MODULE(nvme, nvme_pci_driver, nvme_devclass,
    NEWBUS_PASS_STORAGE, NEWBUS_ORDER_EARLY);
