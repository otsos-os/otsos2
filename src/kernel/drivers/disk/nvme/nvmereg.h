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

$define %type nvme_sqe_t as 64 byte submission queue entry
$define %type nvme_cqe_t as 16 byte completion queue entry
$define %type nvme_id_ctrl_t as Identify Controller data structure
$define %type nvme_id_ns_t as Identify Namespace data structure

$const NVME_REG_* as controller register offsets inside BAR0
$const NVME_CAP_* as shifts and masks of the Controller Capabilities register
$const NVME_CC_* as shifts, masks and bits of the Controller Configuration
    register
$const NVME_CSTS_* as bits of the Controller Status register
$const NVME_AQA_* as shifts of the Admin Queue Attributes register
$const NVME_DBL_BASE as offset of the first doorbell register
$const NVME_ADMIN_* as admin command opcodes
$const NVME_NVM_* as NVM command set opcodes
$const NVME_CNS_* as Identify controller-or-namespace structure selectors
$const NVME_FEAT_NUM_QUEUES as Set Features identifier for queue count
    negotiation
$const NVME_CQ_* as Create I/O Completion Queue command flags
$const NVME_SQ_* as Create I/O Submission Queue command flags
$const NVME_STATUS_* as shifts and masks of the CQE status field
$const NVME_SCT_* as status code types
$const NVME_SC_* as status codes within a type
$const NVME_SQE_SIZE as submission queue entry size in bytes
$const NVME_CQE_SIZE as completion queue entry size in bytes
$const NVME_SQES_SHIFT as log2 of NVME_SQE_SIZE, the value CC.IOSQES carries
$const NVME_CQES_SHIFT as log2 of NVME_CQE_SIZE, the value CC.IOCQES carries
$const NVME_ID_DATA_SIZE as byte size of every Identify result
$const NVME_SERIAL_LEN as Identify Controller serial number field length
$const NVME_MODEL_LEN as Identify Controller model number field length
$const NVME_NSLIST_MAX as namespace identifiers in one Active Namespace List

*/

/* !SPACE!

$space %none

*/

#ifndef KERNEL_DRIVERS_DISK_NVME_NVMEREG_H
#define KERNEL_DRIVERS_DISK_NVME_NVMEREG_H

#include <mlibc/mlibc.h>

#define	NVME_REG_CAP		0x00
#define	NVME_REG_VS		0x08
#define	NVME_REG_INTMS		0x0C
#define	NVME_REG_INTMC		0x10
#define	NVME_REG_CC		0x14
#define	NVME_REG_CSTS		0x1C
#define	NVME_REG_NSSR		0x20
#define	NVME_REG_AQA		0x24
#define	NVME_REG_ASQ		0x28
#define	NVME_REG_ACQ		0x30
#define	NVME_DBL_BASE		0x1000
#define	NVME_CAP_MQES_MASK	0xFFFFULL
#define	NVME_CAP_CQR		(1ULL << 16)
#define	NVME_CAP_TO_SHIFT	24
#define	NVME_CAP_TO_MASK	0xFFULL
#define	NVME_CAP_DSTRD_SHIFT	32
#define	NVME_CAP_DSTRD_MASK	0xFULL
#define	NVME_CAP_CSS_SHIFT	37
#define	NVME_CAP_CSS_MASK	0xFFULL
#define	NVME_CAP_CSS_NVM	0x01
#define	NVME_CAP_MPSMIN_SHIFT	48
#define	NVME_CAP_MPSMAX_SHIFT	52
#define	NVME_CAP_MPS_MASK	0xFULL
#define	NVME_CAP_TO_UNIT_NS	500000000ULL
#define	NVME_CC_EN		0x00000001U
#define	NVME_CC_CSS_SHIFT	4
#define	NVME_CC_CSS_MASK	0x7U
#define	NVME_CC_CSS_NVM		0x0U
#define	NVME_CC_MPS_SHIFT	7
#define	NVME_CC_MPS_MASK	0xFU
#define	NVME_CC_AMS_SHIFT	11
#define	NVME_CC_AMS_MASK	0x7U
#define	NVME_CC_AMS_RR		0x0U
#define	NVME_CC_SHN_SHIFT	14
#define	NVME_CC_SHN_MASK	0x3U
#define	NVME_CC_SHN_NORMAL	0x1U
#define	NVME_CC_IOSQES_SHIFT	16
#define	NVME_CC_IOCQES_SHIFT	20
#define	NVME_MPS_SHIFT_BIAS	12
#define	NVME_CSTS_RDY		0x00000001U
#define	NVME_CSTS_CFS		0x00000002U
#define	NVME_CSTS_SHST_SHIFT	2
#define	NVME_CSTS_SHST_MASK	0x3U
#define	NVME_CSTS_SHST_DONE	0x2U
#define	NVME_AQA_ASQS_SHIFT	0
#define	NVME_AQA_ACQS_SHIFT	16
#define	NVME_AQA_QS_MASK	0xFFFU
#define	NVME_ADMIN_DELETE_IO_SQ	0x00
#define	NVME_ADMIN_CREATE_IO_SQ	0x01
#define	NVME_ADMIN_DELETE_IO_CQ	0x04
#define	NVME_ADMIN_CREATE_IO_CQ	0x05
#define	NVME_ADMIN_IDENTIFY	0x06
#define	NVME_ADMIN_ABORT	0x08
#define	NVME_ADMIN_SET_FEATURES	0x09
#define	NVME_ADMIN_GET_FEATURES	0x0A
#define	NVME_NVM_FLUSH		0x00
#define	NVME_NVM_WRITE		0x01
#define	NVME_NVM_READ		0x02
#define	NVME_CNS_NAMESPACE	0x00
#define	NVME_CNS_CONTROLLER	0x01
#define	NVME_CNS_ACTIVE_NSLIST	0x02
#define	NVME_FEAT_NUM_QUEUES	0x07
#define	NVME_CQ_PC		0x00000001U
#define	NVME_CQ_IEN		0x00000002U
#define	NVME_SQ_PC		0x00000001U
#define	NVME_STATUS_PHASE	0x0001U
#define	NVME_STATUS_SC_SHIFT	1
#define	NVME_STATUS_SC_MASK	0xFFU
#define	NVME_STATUS_SCT_SHIFT	9
#define	NVME_STATUS_SCT_MASK	0x7U
#define	NVME_STATUS_DNR		0x8000U
#define	NVME_SCT_GENERIC	0x0
#define	NVME_SCT_COMMAND	0x1
#define	NVME_SCT_MEDIA		0x2
#define	NVME_SCT_PATH		0x3
#define	NVME_SC_SUCCESS			0x00
#define	NVME_SC_INVALID_OPCODE		0x01
#define	NVME_SC_INVALID_FIELD		0x02
#define	NVME_SC_DATA_TRANSFER_ERROR	0x04
#define	NVME_SC_ABORTED_BY_REQUEST	0x07
#define	NVME_SC_ABORTED_SQ_DELETION	0x08
#define	NVME_SC_LBA_OUT_OF_RANGE	0x80
#define	NVME_SC_CAPACITY_EXCEEDED	0x81
#define	NVME_SC_NAMESPACE_NOT_READY	0x82
#define	NVME_SC_WRITE_FAULT		0x80
#define	NVME_SC_UNRECOVERED_READ	0x81
#define	NVME_SC_GUARD_CHECK_ERROR	0x82
#define	NVME_SC_APPTAG_CHECK_ERROR	0x83
#define	NVME_SC_REFTAG_CHECK_ERROR	0x84
#define	NVME_SC_ACCESS_DENIED		0x86
#define	NVME_SQE_SIZE		64
#define	NVME_CQE_SIZE		16
#define	NVME_SQES_SHIFT		6
#define	NVME_CQES_SHIFT		4
#define	NVME_ID_DATA_SIZE	4096
#define	NVME_SERIAL_LEN		20
#define	NVME_MODEL_LEN		40
#define	NVME_NSLIST_MAX		1024

typedef struct nvme_sqe {
	u8	opc;
	u8	fuse_psdt;
	u16	cid;
	u32	nsid;
	u32	cdw2;
	u32	cdw3;
	u64	mptr;
	u64	prp1;
	u64	prp2;
	u32	cdw10;
	u32	cdw11;
	u32	cdw12;
	u32	cdw13;
	u32	cdw14;
	u32	cdw15;
} __attribute__((packed)) nvme_sqe_t;

_Static_assert(sizeof(nvme_sqe_t) == NVME_SQE_SIZE,
    "NVMe submission queue entry must be exactly 64 bytes");

typedef struct nvme_cqe {
	u32	cdw0;
	u32	rsvd;
	u16	sqhd;
	u16	sqid;
	u16	cid;
	u16	status;
} __attribute__((packed)) nvme_cqe_t;

_Static_assert(sizeof(nvme_cqe_t) == NVME_CQE_SIZE,
    "NVMe completion queue entry must be exactly 16 bytes");

typedef struct nvme_id_ctrl {
	u16	vid;
	u16	ssvid;
	char	sn[NVME_SERIAL_LEN];
	char	mn[NVME_MODEL_LEN];
	char	fr[8];
	u8	rab;
	u8	ieee[3];
	u8	cmic;
	u8	mdts;
	u16	cntlid;
	u32	ver;
	u8	rsvd84[172];
	u16	oacs;
	u8	acl;
	u8	aerl;
	u8	frmw;
	u8	lpa;
	u8	elpe;
	u8	npss;
	u8	rsvd264[248];
	u8	sqes;
	u8	cqes;
	u16	maxcmd;
	u32	nn;
	u16	oncs;
	u16	fuses;
	u8	fna;
	u8	vwc;
	u16	awun;
	u8	rsvd528[3568];
} __attribute__((packed)) nvme_id_ctrl_t;

_Static_assert(sizeof(nvme_id_ctrl_t) == NVME_ID_DATA_SIZE,
    "Identify Controller must fill exactly one 4096 byte page");
_Static_assert(__builtin_offsetof(nvme_id_ctrl_t, mdts) == 77,
    "MDTS moved: the reserved runs above it are wrong");
_Static_assert(__builtin_offsetof(nvme_id_ctrl_t, nn) == 516,
    "NN moved: the reserved runs above it are wrong");
_Static_assert(__builtin_offsetof(nvme_id_ctrl_t, vwc) == 525,
    "VWC moved: the reserved runs above it are wrong");

#define	NVME_ID_CTRL_VWC_PRESENT	0x01
#define	NVME_NS_FLBAS_INDEX_MASK	0x0FU
#define	NVME_NS_LBAF_MS_MASK		0xFFFFU
#define	NVME_NS_LBAF_LBADS_SHIFT	16
#define	NVME_NS_LBAF_LBADS_MASK		0xFFU
#define	NVME_NS_LBAF_COUNT		16

typedef struct nvme_id_ns {
	u64	nsze;
	u64	ncap;
	u64	nuse;
	u8	nsfeat;
	u8	nlbaf;
	u8	flbas;
	u8	mc;
	u8	dpc;
	u8	dps;
	u8	nmic;
	u8	rescap;
	u8	fpi;
	u8	dlfeat;
	u16	nawun;
	u16	nawupf;
	u16	nacwu;
	u8	rsvd40[88];
	u32	lbaf[NVME_NS_LBAF_COUNT];
	u8	rsvd192[3904];
} __attribute__((packed)) nvme_id_ns_t;

_Static_assert(sizeof(nvme_id_ns_t) == NVME_ID_DATA_SIZE,
    "Identify Namespace must fill exactly one 4096 byte page");
_Static_assert(__builtin_offsetof(nvme_id_ns_t, lbaf) == 128,
    "LBAF array moved: the reserved run above it is wrong");

#endif
