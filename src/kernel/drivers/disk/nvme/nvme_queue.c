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

$define %func nvme_now_ns as function with args void
$define %func nvme_deadline_ns as function with args u64
$define %func nvme_wait_expired as function with args u64, u64 *
$define %func nvme_reg32 as function with args const nvme_ctrl_t *, u32
$define %func nvme_reg32_set as procedure with args nvme_ctrl_t *, u32, u32
$define %func nvme_reg64 as function with args const nvme_ctrl_t *, u32
$define %func nvme_reg64_set as procedure with args nvme_ctrl_t *, u32, u64
$define %func nvme_queue_init as function with args nvme_ctrl_t *,
    nvme_queue_t *, u16, u32
$define %func nvme_queue_fini as procedure with args nvme_queue_t *
$define %func nvme_queue_reset as procedure with args nvme_queue_t *
$define %func nvme_queue_service as function with args nvme_queue_t *
$define %func nvme_tracker_get as function with args nvme_queue_t *
$define %func nvme_tracker_put as procedure with args nvme_queue_t *,
    nvme_tracker_t *
$define %func nvme_prp_build as function with args nvme_tracker_t *, u64 *,
    u64 *
$define %func nvme_admin_cmd as function with args nvme_ctrl_t *, nvme_sqe_t *,
    u32 *, u16 *
$define %func nvme_queue_flush_inflight as procedure with args nvme_queue_t *,
    int
$define %func nvme_queue_for_bio as function with args nvme_ctrl_t *
$define %func nvme_submit as function with args disk_t *, bio_t *
$define %func nvme_timeout as function with args disk_t *, bio_t *
$define %func nvme_intr as function with args void *

$const NVME_DONE_BATCH as completions taken out of a CQ per lock acquisition
$const NVME_ABORT_SETTLE_NS as grace given to an aborted command before the
    controller is reset out from under it

*/

/* !SPACE!

$space %internal nvme_status_to_bio, nvme_sqe_slot, nvme_cqe_slot
$space %internal nvme_tracker_release, nvme_cq_drain, nvme_sq_ring
$space %export nvme_now_ns, nvme_deadline_ns, nvme_wait_expired
$space %export nvme_reg32, nvme_reg32_set, nvme_reg64, nvme_reg64_set
$space %export nvme_queue_init, nvme_queue_fini, nvme_queue_reset
$space %export nvme_queue_service, nvme_queue_flush_inflight
$space %export nvme_tracker_get, nvme_tracker_put
$space %export nvme_prp_build, nvme_admin_cmd
$space %export nvme_queue_for_bio
$space %export nvme_submit, nvme_timeout, nvme_intr

*/

#include <kernel/drivers/disk/nvme/nvme.h>
#include <kernel/mm/kmem.h>
#include <kernel/smp/smp.h>
#include <kernel/time.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	NVME_DONE_BATCH		32
#define	NVME_ABORT_SETTLE_NS	1000000000ULL

u64
nvme_now_ns(void)
{
	struct timespec	ts;


	nanouptime(&ts);
	return ((u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec);
}

u64
nvme_deadline_ns(u64 timeout_ns)
{
	return (nvme_now_ns() + timeout_ns);
}


int
nvme_wait_expired(u64 deadline_ns, u64 *spins)
{
	if (spins != NULL) {
		(*spins)++;
		if (*spins > NVME_SPIN_BUDGET) {
			return (1);
		}
	}
	return (nvme_now_ns() >= deadline_ns ? 1 : 0);
}

u32
nvme_reg32(const nvme_ctrl_t *ctrl, u32 off)
{
	return (*(volatile u32 *)(ctrl->regs + off));
}

void
nvme_reg32_set(nvme_ctrl_t *ctrl, u32 off, u32 value)
{
	*(volatile u32 *)(ctrl->regs + off) = value;
}


u64
nvme_reg64(const nvme_ctrl_t *ctrl, u32 off)
{
	return (*(volatile u64 *)(ctrl->regs + off));
}

void
nvme_reg64_set(nvme_ctrl_t *ctrl, u32 off, u64 value)
{
	*(volatile u64 *)(ctrl->regs + off) = value;
}

static int
nvme_status_to_bio(u16 status)
{
	u32	sct, sc;

	sct = (status >> NVME_STATUS_SCT_SHIFT) & NVME_STATUS_SCT_MASK;
	sc = (status >> NVME_STATUS_SC_SHIFT) & NVME_STATUS_SC_MASK;

	if (sct == NVME_SCT_GENERIC && sc == NVME_SC_SUCCESS) {
		return (BIO_STATUS_OK);
	}
	if (sct == NVME_SCT_MEDIA) {
		switch (sc) {
		case NVME_SC_WRITE_FAULT:
		case NVME_SC_UNRECOVERED_READ:
		case NVME_SC_GUARD_CHECK_ERROR:
		case NVME_SC_APPTAG_CHECK_ERROR:
		case NVME_SC_REFTAG_CHECK_ERROR:
			return (BIO_STATUS_MEDIUM);
		default:
			return (BIO_STATUS_IOERR);
		}
	}
	if (sct == NVME_SCT_GENERIC) {
		switch (sc) {
		case NVME_SC_LBA_OUT_OF_RANGE:
		case NVME_SC_CAPACITY_EXCEEDED:
		case NVME_SC_INVALID_FIELD:
			return (BIO_STATUS_INVAL);
		case NVME_SC_INVALID_OPCODE:
			return (BIO_STATUS_UNSUPP);
		case NVME_SC_ABORTED_BY_REQUEST:
		case NVME_SC_ABORTED_SQ_DELETION:
			return (BIO_STATUS_TIMEOUT);
		default:
			return (BIO_STATUS_IOERR);
		}
	}
	return (BIO_STATUS_IOERR);
}

static nvme_sqe_t *
nvme_sqe_slot(nvme_queue_t *q, u32 index)
{
	return (&((nvme_sqe_t *)q->sq_mem.virt)[index]);
}

static nvme_cqe_t *
nvme_cqe_slot(nvme_queue_t *q, u32 index)
{
	return (&((nvme_cqe_t *)q->cq_mem.virt)[index]);
}

int
nvme_queue_init(nvme_ctrl_t *ctrl, nvme_queue_t *q, u16 qid, u32 qdepth)
{
	nvme_tracker_t	*tr;
	u64		stride, sq_bytes, cq_bytes, prp_bytes;
	u32		i;

	if (ctrl == NULL || q == NULL || qdepth < 2) {
		return (-1);
	}
	memset(q, 0, sizeof(*q));
	q->ctrl = ctrl;
	q->qid = qid;
	q->qdepth = qdepth;
	q->phase = 1;
	spin_init(&q->lock, qid == 0 ? "nvme-admin" : "nvme-io", LO_NVME);

	sq_bytes = (u64)qdepth * NVME_SQE_SIZE;
	cq_bytes = (u64)qdepth * NVME_CQE_SIZE;
	prp_bytes = (u64)qdepth * PAGE_SIZE;

	if (dma_mem_alloc(ctrl->q_tag, sq_bytes, DMA_F_NOWAIT,
	    &q->sq_mem) != 0) {
		goto fail;
	}
	if (dma_mem_alloc(ctrl->q_tag, cq_bytes, DMA_F_NOWAIT,
	    &q->cq_mem) != 0) {
		goto fail;
	}

	if (dma_mem_alloc(ctrl->prp_tag, prp_bytes, DMA_F_NOWAIT,
	    &q->prp_mem) != 0) {
		goto fail;
	}
	q->trackers = kmem_calloc(qdepth, sizeof(*q->trackers));
	if (q->trackers == NULL) {
		goto fail;
	}
	q->free_cids = kmem_calloc(qdepth, sizeof(*q->free_cids));
	if (q->free_cids == NULL) {
		goto fail;
	}
	for (i = 0; i < qdepth; i++) {
		tr = &q->trackers[i];
		tr->queue = q;
		tr->cid = (u16)i;
		tr->prp_virt = (u8 *)q->prp_mem.virt + ((u64)i * PAGE_SIZE);
		tr->prp_phys = q->prp_mem.phys + ((u64)i * PAGE_SIZE);

		if (dma_map_create(ctrl->xfer_tag, DMA_F_NOWAIT,
		    &tr->map) != 0) {
			goto fail;
		}
		q->free_cids[i] = (u16)i;
	}
	q->free_count = qdepth;

	stride = 4ULL << ctrl->dstrd;
	q->sq_tdbl = (volatile u32 *)(ctrl->regs + NVME_DBL_BASE +
	    ((u64)qid * 2ULL) * stride);
	q->cq_hdbl = (volatile u32 *)(ctrl->regs + NVME_DBL_BASE +
	    (((u64)qid * 2ULL) + 1ULL) * stride);
	return (0);
fail:
	nvme_queue_fini(q);
	return (-1);
}

void
nvme_queue_fini(nvme_queue_t *q)
{
	u32	i;

	if (q == NULL) {
		return;
	}
	if (q->trackers != NULL) {
		for (i = 0; i < q->qdepth; i++) {

			if ((q->trackers[i].flags & NVME_TR_F_MAPPED) != 0) {
				dma_map_unload(&q->trackers[i].map);
			}
			dma_map_destroy(&q->trackers[i].map);
		}
		kmem_free(q->trackers);
		q->trackers = NULL;
	}
	if (q->free_cids != NULL) {
		kmem_free(q->free_cids);
		q->free_cids = NULL;
	}
	dma_mem_free(&q->prp_mem);
	dma_mem_free(&q->cq_mem);
	dma_mem_free(&q->sq_mem);
	q->free_count = 0;
	q->created = 0;
}


void
nvme_queue_reset(nvme_queue_t *q)
{
	u32	i;

	if (q == NULL) {
		return;
	}
	q->sq_tail = 0;
	q->cq_head = 0;
	q->phase = 1;
	if (q->sq_mem.virt != NULL) {
		memset(q->sq_mem.virt, 0, q->sq_mem.size);
	}
	if (q->cq_mem.virt != NULL) {
		memset(q->cq_mem.virt, 0, q->cq_mem.size);
	}
	if (q->trackers == NULL || q->free_cids == NULL) {
		return;
	}
	for (i = 0; i < q->qdepth; i++) {
		q->trackers[i].bio = NULL;
		q->trackers[i].flags = 0;
		q->trackers[i].status = 0;
		q->trackers[i].cdw0 = 0;
		q->free_cids[i] = (u16)i;
	}
	q->free_count = q->qdepth;
}

nvme_tracker_t *
nvme_tracker_get(nvme_queue_t *q)
{
	nvme_tracker_t	*tr;
	u16		cid;

	if (q->free_count == 0) {
		return (NULL);
	}
	q->free_count--;
	cid = q->free_cids[q->free_count];
	tr = &q->trackers[cid];
	tr->flags = NVME_TR_F_BUSY;
	tr->bio = NULL;
	tr->status = 0;
	tr->cdw0 = 0;
	tr->submit_ns = nvme_now_ns();
	return (tr);
}

void
nvme_tracker_put(nvme_queue_t *q, nvme_tracker_t *tr)
{
	if (tr == NULL || (tr->flags & NVME_TR_F_BUSY) == 0) {
		return;
	}
	tr->flags = 0;
	tr->bio = NULL;
	q->free_cids[q->free_count] = tr->cid;
	q->free_count++;
}


static void
nvme_tracker_release(nvme_tracker_t *tr)
{
	if ((tr->flags & NVME_TR_F_MAPPED) == 0) {
		return;
	}
	if (tr->opc == NVME_NVM_READ) {
		dma_sync(&tr->map, DMA_SYNC_POSTREAD);
	} else {
		dma_sync(&tr->map, DMA_SYNC_POSTWRITE);
	}
	dma_map_unload(&tr->map);
	tr->flags &= (u8)~NVME_TR_F_MAPPED;
}


static u32
nvme_cq_drain(nvme_queue_t *q, nvme_done_t *done, u32 *ndone)
{
	nvme_tracker_t	*tr;
	nvme_cqe_t	*cqe;
	u32		consumed, resid;
	u16		status;
	int		bstatus;

	consumed = 0;
	*ndone = 0;
	for (;;) {
		if (*ndone >= NVME_DONE_BATCH) {
			break;
		}
		cqe = nvme_cqe_slot(q, q->cq_head);
		status = cqe->status;
		if ((status & NVME_STATUS_PHASE) != q->phase) {
			break;
		}

		if (cqe->cid >= q->qdepth) {
			drivers_log("[NVME] q%u: completion for unknown cid "
			    "%u\n", q->qid, cqe->cid);
			q->ctrl->nfail_status++;
		} else {
			tr = &q->trackers[cqe->cid];
			if ((tr->flags & NVME_TR_F_BUSY) == 0) {

				drivers_log("[NVME] q%u: completion for idle "
				    "cid %u\n", q->qid, cqe->cid);
				q->ctrl->nfail_status++;
			} else {
				tr->status = status;
				tr->cdw0 = cqe->cdw0;
				tr->flags |= NVME_TR_F_COMPLETE;
				bstatus = nvme_status_to_bio(status);
				if (bstatus != BIO_STATUS_OK) {
					q->ctrl->nfail_status++;
				}
				if (tr->bio != NULL) {
					nvme_tracker_release(tr);
					resid = bstatus == BIO_STATUS_OK ? 0 :
					    tr->bio->nsectors;
					done[*ndone].bio = tr->bio;
					done[*ndone].status = bstatus;
					done[*ndone].resid = resid;
					(*ndone)++;
					nvme_tracker_put(q, tr);
				}
				q->ncomplete++;
			}
		}
		q->cq_head++;
		if (q->cq_head == q->qdepth) {
			q->cq_head = 0;
			q->phase ^= 1;
		}
		consumed++;
	}
	if (consumed != 0) {
		*q->cq_hdbl = q->cq_head;
	}
	return (consumed);
}


u32
nvme_queue_service(nvme_queue_t *q)
{
	nvme_done_t	done[NVME_DONE_BATCH];
	u32		consumed, total, ndone, i;

	if (q == NULL || q->cq_mem.virt == NULL) {
		return (0);
	}
	total = 0;
	for (;;) {
		spin_lock(&q->lock);
		consumed = nvme_cq_drain(q, done, &ndone);
		spin_unlock(&q->lock);
		for (i = 0; i < ndone; i++) {
			bio_done(done[i].bio, done[i].status, done[i].resid);
		}
		total += consumed;

		if (ndone < NVME_DONE_BATCH) {
			break;
		}
	}
	return (total);
}

int
nvme_intr(void *arg)
{
	nvme_ctrl_t	*ctrl;
	nvme_irq_t	*irq;
	u32		handled, i;

	irq = arg;
	if (irq == NULL || irq->ctrl == NULL) {
		return (-1);
	}
	ctrl = irq->ctrl;
	irq->ncall++;

	if (irq->queue != NULL) {
		handled = nvme_queue_service(irq->queue);
		return (handled != 0 || irq->is_msi != 0 ? 0 : -1);
	}

	handled = nvme_queue_service(&ctrl->admin_q);
	for (i = 0; i < ctrl->nioq; i++) {
		handled += nvme_queue_service(&ctrl->io_q[i]);
	}
	if (handled == 0 && ctrl->irq_msi == 0) {
		return (-1);
	}
	return (0);
}


int
nvme_prp_build(nvme_tracker_t *tr, u64 *prp1, u64 *prp2)
{
	const dma_seg_t	*segs;
	u64		*list;
	u32		nsegs, i, mps;

	mps = tr->queue->ctrl->mps;
	segs = dma_map_segs(&tr->map, &nsegs);
	if (segs == NULL || nsegs == 0) {
		return (-1);
	}
	for (i = 1; i < nsegs; i++) {
		if ((segs[i].phys & ((u64)mps - 1)) != 0) {
			drivers_log("[NVME] PRP seg %u not page aligned: "
			    "0x%llx\n", i, (unsigned long long)segs[i].phys);
			return (-1);
		}
	}
	for (i = 0; i + 1 < nsegs; i++) {
		if ((segs[i].phys + segs[i].len) % (u64)mps != 0) {
			drivers_log("[NVME] PRP seg %u does not end on a page "
			    "boundary: 0x%llx+%llu\n", i,
			    (unsigned long long)segs[i].phys,
			    (unsigned long long)segs[i].len);
			return (-1);
		}
	}
	*prp1 = segs[0].phys;
	if (nsegs == 1) {
		*prp2 = 0;
		return (0);
	}
	if (nsegs == 2) {
		*prp2 = segs[1].phys;
		return (0);
	}
	if (nsegs - 1 > NVME_PRP_ENTRIES_PER_PAGE) {
		drivers_log("[NVME] %u segments exceed one PRP list page\n",
		    nsegs);
		return (-1);
	}
	list = tr->prp_virt;
	for (i = 1; i < nsegs; i++) {
		list[i - 1] = segs[i].phys;
	}
	*prp2 = tr->prp_phys;
	return (0);
}

static void
nvme_sq_ring(nvme_queue_t *q, const nvme_sqe_t *sqe)
{
	nvme_sqe_t	*slot;

	slot = nvme_sqe_slot(q, q->sq_tail);
	memcpy(slot, sqe, sizeof(*sqe));
	q->sq_tail++;
	if (q->sq_tail == q->qdepth) {
		q->sq_tail = 0;
	}

	__atomic_thread_fence(__ATOMIC_RELEASE);
	*q->sq_tdbl = q->sq_tail;
	q->nsubmit++;
}


int
nvme_admin_cmd(nvme_ctrl_t *ctrl, nvme_sqe_t *sqe, u32 *cdw0, u16 *status)
{
	nvme_queue_t	*q;
	nvme_tracker_t	*tr;
	nvme_done_t	done[NVME_DONE_BATCH];
	u64		deadline, spins;
	u32		ndone;
	u16		cid, cqe_status;
	int		complete;

	if (ctrl == NULL || sqe == NULL) {
		return (-1);
	}
	q = &ctrl->admin_q;
	spin_lock(&q->lock);
	tr = nvme_tracker_get(q);
	if (tr == NULL) {
		q->fail_busy++;
		spin_unlock(&q->lock);
		return (-1);
	}
	cid = tr->cid;
	tr->opc = sqe->opc;
	sqe->cid = cid;
	nvme_sq_ring(q, sqe);
	spin_unlock(&q->lock);

	deadline = nvme_deadline_ns(NVME_ADMIN_TIMEOUT_NS);
	spins = 0;
	for (;;) {
		spin_lock(&q->lock);
		(void)nvme_cq_drain(q, done, &ndone);
		complete = (q->trackers[cid].flags & NVME_TR_F_COMPLETE) != 0;
		spin_unlock(&q->lock);
		if (complete) {
			break;
		}
		if (nvme_wait_expired(deadline, &spins)) {

			drivers_log("[NVME] admin opcode 0x%x cid %u timed "
			    "out\n", sqe->opc, cid);
			return (-1);
		}
		__asm__ volatile("pause");
	}


	spin_lock(&q->lock);
	tr = &q->trackers[cid];
	cqe_status = tr->status;
	if (cdw0 != NULL) {
		*cdw0 = tr->cdw0;
	}
	nvme_tracker_put(q, tr);
	spin_unlock(&q->lock);

	if (status != NULL) {
		*status = cqe_status;
	}
	if (nvme_status_to_bio(cqe_status) != BIO_STATUS_OK) {
		drivers_log("[NVME] admin opcode 0x%x failed: sct=%u sc=0x%x\n",
		    sqe->opc, (cqe_status >> NVME_STATUS_SCT_SHIFT) &
		    NVME_STATUS_SCT_MASK,
		    (cqe_status >> NVME_STATUS_SC_SHIFT) &
		    NVME_STATUS_SC_MASK);
		return (-1);
	}
	return (0);
}


nvme_queue_t *
nvme_queue_for_bio(nvme_ctrl_t *ctrl)
{
	int	cpu;
	u32	nioq, idx;


	nioq = __atomic_load_n(&ctrl->nioq, __ATOMIC_ACQUIRE);
	if (nioq == 0) {
		return (NULL);
	}
	cpu = smp_cpu_index();
	idx = cpu < 0 ? 0U : (u32)cpu % nioq;
	return (&ctrl->io_q[idx]);
}

int
nvme_submit(disk_t *disk, bio_t *bio)
{
	nvme_ctrl_t	*ctrl;
	nvme_queue_t	*q;
	nvme_tracker_t	*tr;
	nvme_ns_t	*ns;
	nvme_sqe_t	sqe;
	u64		prp1, prp2, len;
	u32		dflags;

	if (disk == NULL || bio == NULL || disk->private_data == NULL) {
		return (-1);
	}
	ns = disk->private_data;
	ctrl = ns->ctrl;

	if (ctrl->failed != 0 || ctrl->resetting != 0) {
		return (-1);
	}
	q = nvme_queue_for_bio(ctrl);
	if (q == NULL || q->created == 0) {
		return (-1);
	}

	memset(&sqe, 0, sizeof(sqe));
	sqe.nsid = ns->nsid;
	switch (bio->cmd) {
	case BIO_READ:
		sqe.opc = NVME_NVM_READ;
		break;
	case BIO_WRITE:
		sqe.opc = NVME_NVM_WRITE;
		break;
	case BIO_FLUSH:
		sqe.opc = NVME_NVM_FLUSH;
		break;
	default:
		return (-1);
	}

	spin_lock(&q->lock);
	tr = nvme_tracker_get(q);
	if (tr == NULL) {
		q->fail_busy++;
		spin_unlock(&q->lock);
		return (-1);
	}
	tr->bio = bio;
	tr->opc = sqe.opc;
	bio->driver_priv = tr;
	sqe.cid = tr->cid;

	if (bio->cmd != BIO_FLUSH) {
		len = (u64)bio->nsectors * (u64)ns->lba_size;
		dflags = DMA_F_NOWAIT | (bio->cmd == BIO_READ ? DMA_F_READ :
		    DMA_F_WRITE);
		if (dma_map_load(&tr->map, bio->buf, len, dflags) != 0) {
			nvme_tracker_put(q, tr);
			spin_unlock(&q->lock);
			return (-1);
		}
		tr->flags |= NVME_TR_F_MAPPED;
		if (nvme_prp_build(tr, &prp1, &prp2) != 0) {
			q->fail_prp++;
			dma_map_unload(&tr->map);
			tr->flags &= (u8)~NVME_TR_F_MAPPED;
			nvme_tracker_put(q, tr);
			spin_unlock(&q->lock);
			return (-1);
		}

		dma_sync(&tr->map, bio->cmd == BIO_WRITE ? DMA_SYNC_PREWRITE :
		    DMA_SYNC_PREREAD);
		sqe.prp1 = prp1;
		sqe.prp2 = prp2;
		sqe.cdw10 = (u32)(bio->lba & 0xFFFFFFFFULL);
		sqe.cdw11 = (u32)(bio->lba >> 32);
		sqe.cdw12 = (u32)(bio->nsectors - 1) & 0xFFFFU;
	}
	nvme_sq_ring(q, &sqe);
	spin_unlock(&q->lock);
	return (0);
}


void
nvme_queue_flush_inflight(nvme_queue_t *q, int status)
{
	nvme_done_t	done[NVME_DONE_BATCH];
	nvme_tracker_t	*tr;
	u32		ndone, i;

	if (q == NULL || q->trackers == NULL) {
		return;
	}
	for (;;) {
		ndone = 0;
		spin_lock(&q->lock);
		for (i = 0; i < q->qdepth && ndone < NVME_DONE_BATCH; i++) {
			tr = &q->trackers[i];
			if ((tr->flags & NVME_TR_F_BUSY) == 0 ||
			    tr->bio == NULL) {
				continue;
			}
			nvme_tracker_release(tr);
			done[ndone].bio = tr->bio;
			done[ndone].status = status;
			done[ndone].resid = tr->bio->nsectors;
			ndone++;
			nvme_tracker_put(q, tr);
		}
		spin_unlock(&q->lock);
		for (i = 0; i < ndone; i++) {
			bio_done(done[i].bio, done[i].status, done[i].resid);
		}
		if (ndone == 0) {
			break;
		}
	}
}


int
nvme_timeout(disk_t *disk, bio_t *bio)
{
	nvme_ctrl_t	*ctrl;
	nvme_queue_t	*q;
	nvme_tracker_t	*tr;
	nvme_sqe_t	sqe;
	nvme_ns_t	*ns;
	u64		deadline, spins;
	u16		cid;
	int		inflight;

	if (disk == NULL || bio == NULL || disk->private_data == NULL) {
		return (-1);
	}
	ns = disk->private_data;
	ctrl = ns->ctrl;

	tr = bio->driver_priv;
	if (tr == NULL || tr->queue == NULL || tr->queue->ctrl != ctrl) {
		return (-1);
	}
	q = tr->queue;

	spin_lock(&q->lock);
	tr = bio->driver_priv;
	inflight = 0;
	if (tr != NULL && tr->queue == q && tr->bio == bio &&
	    (tr->flags & NVME_TR_F_BUSY) != 0) {
		inflight = 1;
		cid = tr->cid;
	} else {
		cid = 0;
	}
	spin_unlock(&q->lock);

	if (inflight == 0) {

		return (-1);
	}
	if (ctrl->resetting != 0) {
		return (-1);
	}

	drivers_log("[NVME] %s: command cid %u timed out, aborting\n",
	    disk->name, cid);
	memset(&sqe, 0, sizeof(sqe));
	sqe.opc = NVME_ADMIN_ABORT;
	sqe.cdw10 = ((u32)cid << 16) | (u32)q->qid;
	if (nvme_admin_cmd(ctrl, &sqe, NULL, NULL) == 0) {
		deadline = nvme_deadline_ns(NVME_ABORT_SETTLE_NS);
		spins = 0;
		for (;;) {
			(void)nvme_queue_service(q);
			if ((__atomic_load_n(&bio->flags, __ATOMIC_ACQUIRE) &
			    BIO_F_DONE) != 0) {
				return (0);
			}
			if (nvme_wait_expired(deadline, &spins)) {
				break;
			}
			__asm__ volatile("pause");
		}
	}

	drivers_log("[NVME] %s: abort ineffective, resetting controller\n",
	    disk->name);
	(void)nvme_ctrl_reset(ctrl);
	if ((__atomic_load_n(&bio->flags, __ATOMIC_ACQUIRE) &
	    BIO_F_DONE) == 0) {

		bio_done(bio, BIO_STATUS_TIMEOUT, bio->nsectors);
	}
	return (0);
}
