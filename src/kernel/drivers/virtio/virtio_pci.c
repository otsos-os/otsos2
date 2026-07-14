/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <kernel/drivers/virtio/virtio_pci.h>
#include <mlibc/stdio.h>

int
virtio_pci_init(virtio_hw_t *hw, pci_device_t *dev,
    u32 accepted_features)
{
	u32	device_features, device_features_hi;
	u8	status;

	if (!hw || !dev || virtio_hw_init(hw, dev) != 0) {
		return (-1);
	}

	device_features = virtio_hw_get_features(hw);
	device_features_hi = virtio_hw_get_features_hi(hw);
	if ((device_features_hi &
	    (1u << (VIRTIO_F_VERSION_1 - 32))) == 0) {
		drivers_log("[VIRTIO] VERSION_1 not offered\n");
		virtio_hw_set_status(hw, VIRTIO_STATUS_FAILED);
		return (-1);
	}

	accepted_features &= device_features;
	virtio_hw_set_features(hw, accepted_features);
	virtio_hw_set_features_hi(hw,
	    1u << (VIRTIO_F_VERSION_1 - 32));
	virtio_hw_set_status(hw, VIRTIO_STATUS_ACKNOWLEDGE |
	    VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
	status = virtio_hw_get_status(hw);
	if ((status & VIRTIO_STATUS_FEATURES_OK) == 0) {
		drivers_log("[VIRTIO] feature negotiation failed\n");
		virtio_hw_set_status(hw, VIRTIO_STATUS_FAILED);
		return (-1);
	}

	hw->features = accepted_features;
	hw->ready = 1;
	return (0);
}

void
virtio_pci_shutdown(virtio_hw_t *hw)
{
	virtio_hw_shutdown(hw);
}
