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
 * and/or other materials provided with the distribution.
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

#include <kernel/drivers/rtc/rtc.h>
#include <kernel/time.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define	CMOS_ADDR	0x70
#define	CMOS_DATA	0x71
#define	RTC_REG_SEC	0x00
#define	RTC_REG_MIN	0x02
#define	RTC_REG_HOUR	0x04
#define	RTC_REG_MDAY	0x07
#define	RTC_REG_MONTH	0x08
#define	RTC_REG_YEAR	0x09
#define	RTC_REG_CENTURY	0x32
#define	RTC_REG_STATUS_A 0x0A
#define	RTC_REG_STATUS_B 0x0B
#define	RTC_STATUS_B_24H	0x02
#define	RTC_STATUS_B_BIN	0x04
#define	RTC_UIP		0x80

static u8
bcd_to_bin(u8 bcd)
{
	return (((bcd >> 4) & 0x0F) * 10) + (bcd & 0x0F);
}

static u8
cmos_read(u8 reg)
{
	outb(CMOS_ADDR, reg);
	return (inb(CMOS_DATA));
}

static int
is_leap_year(u32 year)
{
	return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

static u32
days_in_month(u32 year, u32 month)
{
	static const u32	days[] = { 31, 28, 31, 30, 31, 30,
		    31, 31, 30, 31, 30, 31 };

	if (month == 2 && is_leap_year(year)) {
		return (29);
	}
	return (days[month - 1]);
}

static u64
epoch_seconds_from_date(u32 year, u32 month, u32 day,
    u32 hour, u32 min, u32 sec)
{
	u64	days;
	u32	y;

	days = 0;
	for (y = 1970; y < year; y++) {
		days += is_leap_year(y) ? 366 : 365;
	}
	for (y = 1; y < month; y++) {
		days += days_in_month(year, y);
	}
	days += day - 1;

	return (days * 86400ULL + hour * 3600ULL + min * 60ULL + sec);
}

static void
read_cmos_fields(u8 *sec, u8 *min, u8 *hour,
    u8 *mday, u8 *month, u8 *year, u8 *century)
{
	u8	status_b;
	int	bin_mode;
	int	hour24;

	status_b = cmos_read(RTC_REG_STATUS_B);
	bin_mode = (status_b & RTC_STATUS_B_BIN) ? 1 : 0;
	hour24 = (status_b & RTC_STATUS_B_24H) ? 1 : 0;

	*sec = cmos_read(RTC_REG_SEC);
	*min = cmos_read(RTC_REG_MIN);
	*hour = cmos_read(RTC_REG_HOUR);
	*mday = cmos_read(RTC_REG_MDAY);
	*month = cmos_read(RTC_REG_MONTH);
	*year = cmos_read(RTC_REG_YEAR);
	*century = cmos_read(RTC_REG_CENTURY);

	if (!bin_mode) {
		*sec = bcd_to_bin(*sec);
		*min = bcd_to_bin(*min);
		*hour = bcd_to_bin(*hour);
		*mday = bcd_to_bin(*mday);
		*month = bcd_to_bin(*month);
		*year = bcd_to_bin(*year);
		*century = bcd_to_bin(*century);
	}

	if (!hour24 && (*hour & 0x80)) {
		*hour = ((*hour & 0x7F) + 12) % 24;
	}
}

int
rtc_read_time(struct bintime *bt)
{
	u8	sec, min, hour, mday, month, year, century;
	u32	full_year;
	u64	epoch_sec;
	int	i;

	if (bt == NULL) {
		return (-1);
	}
	while (cmos_read(RTC_REG_STATUS_A) & RTC_UIP) {
		__asm__ volatile("pause");
	}

	read_cmos_fields(&sec, &min, &hour, &mday, &month, &year, &century);

	for (i = 0; i < 100; i++) {
		u8	sec2, min2, hour2, mday2, month2, year2, century2;

		while (cmos_read(RTC_REG_STATUS_A) & RTC_UIP) {
			__asm__ volatile("pause");
		}
		read_cmos_fields(&sec2, &min2, &hour2, &mday2, &month2,
		    &year2, &century2);

		if (sec == sec2 && min == min2 && hour == hour2 &&
		    mday == mday2 && month == month2 && year == year2 &&
		    century == century2) {
			break;
		}
		sec = sec2;
		min = min2;
		hour = hour2;
		mday = mday2;
		month = month2;
		year = year2;
		century = century2;
	}

	full_year = (u32)century * 100 + (u32)year;
	if (full_year < 1970) {
		com1_printf("[RTC] CMOS year %u is before 1970, "
		    "falling back to epoch\n", full_year);
		bt->sec = 0;
		bt->frac = 0;
		return (-1);
	}

	epoch_sec = epoch_seconds_from_date(full_year, month, mday,
	    hour, min, sec);

	bt->sec = epoch_sec;
	bt->frac = 0;

	com1_printf("[RTC] CMOS time: ");
	com1_write_dec(full_year);
	com1_printf("-");
	com1_write_dec(month);
	com1_printf("-");
	com1_write_dec(mday);
	com1_printf(" ");
	com1_write_dec(hour);
	com1_printf(":");
	com1_write_dec(min);
	com1_printf(":");
	com1_write_dec(sec);
	com1_printf(" (epoch ");
	com1_write_dec(epoch_sec);
	com1_printf(")\n");

	return (0);
}
