/*	$NetBSD: drvstats.c,v 1.9 2014/06/13 11:26:37 joerg Exp $	*/

/*
 * Copyright (c) 1996 John M. Vinopal
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by John M. Vinopal.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/iostat.h>

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "drvstats.h"

/* Structures to hold the statistics. */
struct _drive	cur, last;

extern int	hz;

/* sysctl hw.drivestats buffer. */
static struct io_sysctl	*drives = NULL;

/* Backward compatibility references. */
size_t		ndrive = 0;
int		*drv_select;
char		**dr_name;

/* Missing from <sys/time.h> */
#define	timerset(tvp, uvp) do {						\
	((uvp)->tv_sec = (tvp)->tv_sec);				\
	((uvp)->tv_usec = (tvp)->tv_usec);				\
} while (/* CONSTCOND */0)

/*
 * Take the delta between the present values and the last recorded
 * values, storing the present values in the 'last' structure, and
 * the delta values in the 'cur' structure.
 */
void
drvswap(void)
{
	u_int64_t tmp;
	size_t	i;

#define	SWAP(fld) do {							\
	tmp = cur.fld;							\
	cur.fld -= last.fld;						\
	last.fld = tmp;							\
} while (/* CONSTCOND */0)

	for (i = 0; i < ndrive; i++) {
		struct timeval	tmp_timer;

		if (!cur.select[i])
			continue;

		/* Delta Values. */
		SWAP(rxfer[i]);
		SWAP(wxfer[i]);
		SWAP(seek[i]);
		SWAP(rbytes[i]);
		SWAP(wbytes[i]);

		/* Delta Time. */
		timerclear(&tmp_timer);
		timerset(&(cur.time[i]), &tmp_timer);
		timersub(&tmp_timer, &(last.time[i]), &(cur.time[i]));
		timerclear(&(last.time[i]));
		timerset(&tmp_timer, &(last.time[i]));
	}
}

void
tkswap(void)
{
	u_int64_t tmp;

	SWAP(tk_nin);
	SWAP(tk_nout);
}

void
cpuswap(void)
{
	double etime;
	u_int64_t tmp;
	int	i, state;

	for (i = 0; i < CPUSTATES; i++)
		SWAP(cp_time[i]);

	etime = 0;
	for (state = 0; state < CPUSTATES; ++state) {
		etime += cur.cp_time[state];
	}
	if (etime == 0)
		etime = 1;
	etime /= hz;
	etime /= cur.cp_ncpu;

	cur.cp_etime = etime;
}
#undef SWAP

/*
 * Read the drive statistics for each drive in the drive list.
 * Also collect statistics for tty i/o and CPU ticks.
 */
void
drvreadstats(void)
{
	size_t		size, i;
	int		mib[3];

	mib[0] = CTL_HW;
	mib[1] = HW_IOSTATS;
	mib[2] = sizeof(struct io_sysctl);

	size = ndrive * sizeof(struct io_sysctl);
	if (sysctl(mib, 3, drives, &size, NULL, 0) < 0)
		err(1, "sysctl hw.iostats failed");
	for (i = 0; i < ndrive; i++) {
		cur.rxfer[i] = drives[i].rxfer;
		cur.wxfer[i] = drives[i].wxfer;
		cur.seek[i] = drives[i].seek;
		cur.rbytes[i] = drives[i].rbytes;
		cur.wbytes[i] = drives[i].wbytes;
		cur.time[i].tv_sec = drives[i].time_sec;
		cur.time[i].tv_usec = drives[i].time_usec;
	}

		mib[0] = CTL_KERN;
	mib[1] = KERN_TKSTAT;
	mib[2] = KERN_TKSTAT_NIN;
	size = sizeof(cur.tk_nin);
	if (sysctl(mib, 3, &cur.tk_nin, &size, NULL, 0) < 0)
		cur.tk_nin = 0;

	mib[2] = KERN_TKSTAT_NOUT;
	size = sizeof(cur.tk_nout);
	if (sysctl(mib, 3, &cur.tk_nout, &size, NULL, 0) < 0)
		cur.tk_nout = 0;

	size = sizeof(cur.cp_time);
	(void)memset(cur.cp_time, 0, size);
	mib[0] = CTL_KERN;
	mib[1] = KERN_CP_TIME;
	if (sysctl(mib, 2, cur.cp_time, &size, NULL, 0) < 0)
		(void)memset(cur.cp_time, 0, sizeof(cur.cp_time));
}

/*
 * Read collect statistics for tty i/o.
 */

void
tkreadstats(void)
{
	size_t		size;
	int		mib[3];

	mib[0] = CTL_KERN;
	mib[1] = KERN_TKSTAT;
	mib[2] = KERN_TKSTAT_NIN;
	size = sizeof(cur.tk_nin);
	if (sysctl(mib, 3, &cur.tk_nin, &size, NULL, 0) < 0)
		cur.tk_nin = 0;

	mib[2] = KERN_TKSTAT_NOUT;
	size = sizeof(cur.tk_nout);
	if (sysctl(mib, 3, &cur.tk_nout, &size, NULL, 0) < 0)
		cur.tk_nout = 0;
}

/*
 * Read collect statistics for CPU ticks.
 */

void
cpureadstats(void)
{
	size_t		size;
	int		mib[2];

	size = sizeof(cur.cp_time);
	(void)memset(cur.cp_time, 0, size);
	mib[0] = CTL_KERN;
	mib[1] = KERN_CP_TIME;
	if (sysctl(mib, 2, cur.cp_time, &size, NULL, 0) < 0)
		(void)memset(cur.cp_time, 0, sizeof(cur.cp_time));
}

/*
 * Perform all of the initialization and memory allocation needed to
 * track drive statistics.
 */
int
drvinit(int selected)
{
	struct clockinfo clockinfo;
	size_t		size, i;
	static int	once = 0;
	int		mib[3];

	if (once)
		return (1);

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	size = sizeof(cur.cp_ncpu);
	if (sysctl(mib, 2, &cur.cp_ncpu, &size, NULL, 0) == -1)
		err(1, "sysctl hw.ncpu failed");

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	size = sizeof(clockinfo);
	if (sysctl(mib, 2, &clockinfo, &size, NULL, 0) == -1)
		err(1, "sysctl kern.clockrate failed");
	hz = clockinfo.stathz;
	if (!hz)
		hz = clockinfo.hz;

	mib[0] = CTL_HW;
	mib[1] = HW_IOSTATS;
	mib[2] = sizeof(struct io_sysctl);
	if (sysctl(mib, 3, NULL, &size, NULL, 0) == -1)
		err(1, "sysctl hw.drivestats failed");
	ndrive = size / sizeof(struct io_sysctl);

	if (size == 0) {
		warnx("No drives attached.");
	} else {
		drives = (struct io_sysctl *)malloc(size);
		if (drives == NULL)
			errx(1, "Memory allocation failure.");
	}

	/* Allocate space for the statistics. */
	cur.time = calloc(ndrive, sizeof(struct timeval));
	cur.rxfer = calloc(ndrive, sizeof(u_int64_t));
	cur.wxfer = calloc(ndrive, sizeof(u_int64_t));
	cur.seek = calloc(ndrive, sizeof(u_int64_t));
	cur.rbytes = calloc(ndrive, sizeof(u_int64_t));
	cur.wbytes = calloc(ndrive, sizeof(u_int64_t));
	last.time = calloc(ndrive, sizeof(struct timeval));
	last.rxfer = calloc(ndrive, sizeof(u_int64_t));
	last.wxfer = calloc(ndrive, sizeof(u_int64_t));
	last.seek = calloc(ndrive, sizeof(u_int64_t));
	last.rbytes = calloc(ndrive, sizeof(u_int64_t));
	last.wbytes = calloc(ndrive, sizeof(u_int64_t));
	cur.select = calloc(ndrive, sizeof(int));
	cur.name = calloc(ndrive, sizeof(char *));

	if (cur.time == NULL || cur.rxfer == NULL ||
	    cur.wxfer == NULL || cur.seek == NULL ||
	    cur.rbytes == NULL || cur.wbytes == NULL ||
	    last.time == NULL || last.rxfer == NULL ||
	    last.wxfer == NULL || last.seek == NULL ||
	    last.rbytes == NULL || last.wbytes == NULL ||
	    cur.select == NULL || cur.name == NULL)
		errx(1, "Memory allocation failure.");

	/* Set up the compatibility interfaces. */
	drv_select = cur.select;
	dr_name = cur.name;

	/* Read the drive names and set intial selection. */
	mib[0] = CTL_HW;		/* Should be still set from */
	mib[1] = HW_IOSTATS;		/* ... above, but be safe... */
	mib[2] = sizeof(struct io_sysctl);
	if (sysctl(mib, 3, drives, &size, NULL, 0) == -1)
		err(1, "sysctl hw.iostats failed");
	for (i = 0; i < ndrive; i++) {
		cur.name[i] = drives[i].name;
		cur.select[i] = selected;
	}

	/* Never do this initialization again. */
	once = 1;
	return (1);
}
