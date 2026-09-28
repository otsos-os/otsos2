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

$define %type hda_widget_t as one codec node with its full capability set
$define %type hda_pcm_caps_t as decoded PCM format capability set
$define %type hda_amp_caps_t as decoded amplifier capability descriptor
$define %type u32 as 32 bit unsigned

$define %func hda_widget_parse_caps as procedure with args hda_widget_t *, u32
$define %func hda_widget_parse_pcm as procedure with args hda_pcm_caps_t *, u32
$define %func hda_widget_parse_amp as procedure with args hda_amp_caps_t *, u32
$define %func hda_widget_parse_conns as procedure with args hda_widget_t *, u32, u32
$define %func hda_pin_sense_present as function with args u32
$define %func hda_pcm_caps_has as function with args const hda_pcm_caps_t *, uint, uint
*/

/* !SPACE!

$space %export hda_widget_parse_caps, hda_widget_parse_pcm
$space %export hda_widget_parse_amp, hda_widget_parse_conns
$space %export hda_pin_sense_present, hda_pcm_caps_has

*/

#include <kernel/audio/hdaudio/hdawidget.h>

void
hda_widget_parse_caps(hda_widget_t *widget, u32 caps)
{
	u32	type;

	if (widget == NULL) {
		return;
	}
	widget->caps = caps;
	type = (caps & HDA_WCAP_TYPE_MASK) >> HDA_WCAP_TYPE_SHIFT;
	switch (type) {
	case 0x0:
		widget->type = HDA_WIDGET_AUDIO_OUTPUT;
		break;
	case 0x1:
		widget->type = HDA_WIDGET_AUDIO_INPUT;
		break;
	case 0x2:
		widget->type = HDA_WIDGET_AUDIO_MIXER;
		break;
	case 0x3:
		widget->type = HDA_WIDGET_AUDIO_SELECTOR;
		break;
	case 0x4:
		widget->type = HDA_WIDGET_PIN_COMPLEX;
		break;
	case 0x5:
		widget->type = HDA_WIDGET_POWER;
		break;
	case 0x6:
		widget->type = HDA_WIDGET_VOLUME_KNOB;
		break;
	case 0x7:
		widget->type = HDA_WIDGET_BEEP_GEN;
		break;
	case 0xF:
		widget->type = HDA_WIDGET_VENDOR;
		break;
	default:
		widget->type = HDA_WIDGET_UNKNOWN;
		break;
	}
}

void
hda_widget_parse_pcm(hda_pcm_caps_t *pcm, u32 response)
{
	if (pcm == NULL) {
		return;
	}
	pcm->bits = (response >> HDA_PCM_BITS_SHIFT) & HDA_PCM_BITS_MASK;
	pcm->rates = response & HDA_PCM_RATES_MASK;
}

void
hda_widget_parse_amp(hda_amp_caps_t *amp, u32 response)
{
	if (amp == NULL) {
		return;
	}
	amp->mute_cap = (response & HDA_AMP_MUTE_CAP) != 0 ? 1 : 0;
	amp->step_size = (response >> HDA_AMP_STEP_SHIFT) &
	    HDA_AMP_STEP_MASK;
	amp->num_steps = (response >> HDA_AMP_NUM_STEPS_SHIFT) &
	    HDA_AMP_NUM_STEPS_MASK;
	amp->offset = (response >> HDA_AMP_OFFSET_SHIFT) &
	    HDA_AMP_OFFSET_MASK;
}


void
hda_widget_parse_conns(hda_widget_t *widget, u32 response, u32 extra)
{
	u32	bit;

	if (widget == NULL) {
		return;
	}
	if ((response & 0x80000000U) == 0) {
		widget->conn_count = 0;
		for (bit = 0; bit < 32; bit++) {
			if ((response & (1u << bit)) != 0 &&
			    widget->conn_count < HDA_MAX_CONN) {
				widget->conn_list[widget->conn_count++] =
				    bit;
			}
		}
		return;
	}


	if (widget->conn_count < HDA_MAX_CONN) {
		widget->conn_list[widget->conn_count++] =
		    response & 0xFF;
	}
	if ((response & 0x00000100U) != 0 && extra != 0) {
		if (widget->conn_count < HDA_MAX_CONN) {
			widget->conn_list[widget->conn_count++] =
			    extra & 0x7F;
		}
	} else if (extra != 0) {
		if (widget->conn_count < HDA_MAX_CONN) {
			widget->conn_list[widget->conn_count++] =
			    extra & 0xFF;
		}
	}
}

int
hda_pin_sense_present(u32 sense)
{
	return ((sense & HDA_PIN_SENSE_PRESENT) != 0 ? 1 : 0);
}

int
hda_pcm_caps_has(const hda_pcm_caps_t *pcm, u32 bits, u32 rate)
{
	if (pcm == NULL) {
		return (0);
	}
	if (bits >= 32) {
		return (0);
	}
	if ((pcm->bits & (1u << bits)) == 0) {
		return (0);
	}
	if (rate >= 32) {
		return (0);
	}
	return ((pcm->rates & (1u << rate)) != 0 ? 1 : 0);
}
