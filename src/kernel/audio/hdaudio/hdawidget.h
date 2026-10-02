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
$define %type hda_widget_type_t as coarse function of a widget
$define %type hda_pcm_caps_t as decoded PCM format capability set
$define %type hda_amp_caps_t as decoded amplifier capability descriptor

$const HDA_WIDGET_* as widget type tags from AUDIO WIDGET CAPS
$const HDA_WCAP_* as widget capability bits
$const HDA_PIN_CAP_* as pin capability bits
$const HDA_PIN_SENSE_* as GET_PIN_SENSE bits
$const HDA_PCM_* as PWM format capability bits from PARAM_PCM
$const HDA_MAX_WIDGETS as widgets one codec may expose
$const HDA_MAX_CONN as connections one widget may list

$define %func hda_widget_parse_caps as procedure with args hda_widget_t *, u32
$define %func hda_widget_parse_pcm as procedure with args hda_pcm_caps_t *, u32
$define %func hda_widget_parse_amp as procedure with args hda_amp_caps_t *, u32
$define %func hda_widget_parse_conns as procedure with args hda_widget_t *, u32, u32
$define %func hda_pin_sense_present as function with args u32
$define %func hda_pcm_caps_has as function with args const hda_pcm_caps_t *, uint, uint, uint
*/

/* !SPACE!

$space %export hda_widget_parse_caps, hda_widget_parse_pcm
$space %export hda_widget_parse_amp, hda_widget_parse_conns
$space %export hda_pin_sense_present, hda_pcm_caps_has

*/

#ifndef KERNEL_AUDIO_HDAUDIO_HDAWIDGET_H
#define KERNEL_AUDIO_HDAUDIO_HDAWIDGET_H

#include <mlibc/mlibc.h>

#define	HDA_MAX_WIDGETS		128
#define	HDA_MAX_CONN		32

typedef enum hda_widget_type {
	HDA_WIDGET_AUDIO_OUTPUT = 0,
	HDA_WIDGET_AUDIO_INPUT = 1,
	HDA_WIDGET_AUDIO_MIXER = 2,
	HDA_WIDGET_AUDIO_SELECTOR = 3,
	HDA_WIDGET_PIN_COMPLEX = 4,
	HDA_WIDGET_POWER = 5,
	HDA_WIDGET_VOLUME_KNOB = 6,
	HDA_WIDGET_BEEP_GEN = 7,
	HDA_WIDGET_VENDOR = 15,
	HDA_WIDGET_UNKNOWN = 0xFF
} hda_widget_type_t;

#define	HDA_WCAP_STEREO		0x00000001U
#define	HDA_WCAP_IN_AMP		0x00000002U
#define	HDA_WCAP_OUT_AMP	0x00000004U
#define	HDA_WCAP_AMP_OVR	0x00000008U
#define	HDA_WCAP_FORMAT_OVR	0x00000010U
#define	HDA_WCAP_STRIPE		0x00000020U
#define	HDA_WCAP_PROC_WIDGET	0x00000040U
#define	HDA_WCAP_UNSOL_CAP	0x00000080U
#define	HDA_WCAP_CONN_LIST	0x00000100U
#define	HDA_WCAP_DIGITAL	0x00000200U
#define	HDA_WCAP_POWER_CTRL	0x00000400U
#define	HDA_WCAP_LR_SWAP	0x00000800U
#define	HDA_WCAP_CP_CAPS	0x00001000U
#define	HDA_WCAP_CHAN_CNT_MASK	0x0000E000U
#define	HDA_WCAP_CHAN_CNT_SHIFT	13
#define	HDA_WCAP_DELAY_MASK	0x000F0000U
#define	HDA_WCAP_DELAY_SHIFT	16
#define	HDA_WCAP_TYPE_MASK	0x00F00000U
#define	HDA_WCAP_TYPE_SHIFT	20
#define	HDA_PIN_CAP_IMP_SENSE	0x00000001U
#define	HDA_PIN_CAP_TRIG_REQ	0x00000002U
#define	HDA_PIN_CAP_PRES_DET	0x00000004U
#define	HDA_PIN_CAP_HP_DRV	0x00000008U
#define	HDA_PIN_CAP_OUT		0x00000010U
#define	HDA_PIN_CAP_IN		0x00000020U
#define	HDA_PIN_CAP_BALANCED	0x00000040U
#define	HDA_PIN_CAP_HDMI	0x00000080U
#define	HDA_PIN_CAP_VREF_MASK	0x0000FF00U
#define	HDA_PIN_CAP_VREF_50	0x00000100U
#define	HDA_PIN_CAP_VREF_80	0x00000200U
#define	HDA_PIN_CAP_VREF_100	0x00000400U
#define	HDA_PIN_CAP_VREF_GRD	0x00000800U
#define	HDA_PIN_CAP_EAPD	0x00010000U
#define	HDA_PIN_CAP_DISABLE	0x00020000U
#define	HDA_PIN_CAP_DPLB_PIN	0x00040000U
#define	HDA_PIN_SENSE_PRESENT	0x80000000U
#define	HDA_PIN_SENSE_ELDV	0x40000000U
#define	HDA_PCM_BITS_MASK	0x00000007U
#define	HDA_PCM_BITS_SHIFT	16
#define	HDA_PCM_RATES_MASK	0x000007FFU
#define	HDA_PCM_RATES_SHIFT	0
#define	HDA_AMP_MUTE_CAP	0x80000000U
#define	HDA_AMP_LOW_CAP	0x40000000U
#define	HDA_AMP_STEP_SHIFT	16
#define	HDA_AMP_STEP_MASK	0x7FU
#define	HDA_AMP_OFFSET_SHIFT	8
#define	HDA_AMP_OFFSET_MASK	0x7FU
#define	HDA_AMP_NUM_STEPS_SHIFT	0
#define	HDA_AMP_NUM_STEPS_MASK	0x7FU
#define	HDA_AMP_SET_MUTE	0x8000U

typedef struct hda_pcm_caps {
	u32	bits;
	u32	rates;
} hda_pcm_caps_t;

typedef struct hda_amp_caps {
	u32	mute_cap;
	u32	step_size;
	u32	num_steps;
	u32	offset;
} hda_amp_caps_t;

typedef struct hda_widget {
	u32			nid;
	u32			caps;
	u32			pin_caps;
	u32			config_default;
	u32			conn_list[HDA_MAX_CONN];
	u32			conn_count;
	u32			conn_sel;
	hda_widget_type_t	type;
	hda_pcm_caps_t		pcm;
	hda_amp_caps_t		amp_in;
	hda_amp_caps_t		amp_out;
	u32			eapd;
	u32			power_state;
} hda_widget_t;

void
hda_widget_parse_caps(hda_widget_t *widget, u32 caps);

void
hda_widget_parse_pcm(hda_pcm_caps_t *pcm, u32 response);

void
hda_widget_parse_amp(hda_amp_caps_t *amp, u32 response);

void
hda_widget_parse_conns(hda_widget_t *widget, u32 response, u32 extra);

int
hda_pin_sense_present(u32 sense);

int
hda_pcm_caps_has(const hda_pcm_caps_t *pcm, u32 bits, u32 rate);

#endif
