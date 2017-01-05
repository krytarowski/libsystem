/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - curses fullscreen output
 *	-------------------------------------
 *
 *	$Id: curses.c,v 1.10 2009/04/16 05:56:32 lukem Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Thu Jun  1 16:24:43 2000]
 *
 *---------------------------------------------------------------------------*/

#ifdef USE_CURSES

#include "isdnd.h"

#define CHPOS(cfgp) (((cfgp)->isdncontrollerused*2) + (cfgp)->isdnchannelused)

static void display_budget(void);
static void display_cards(void);
static void menuexit(WINDOW *menu_w);

static int ncontroller = 0;

/*---------------------------------------------------------------------------*
 *	init curses fullscreen display
 *---------------------------------------------------------------------------*/
void
init_screen(void)
{
	char buffer[512];
	int uheight, lheight;
	int i, j;
	struct cfg_entry *p;
	struct isdn_ctrl_state *ctrl;

	initscr();			/* curses init */
	ncontroller = count_ctrl_states();

	if ((COLS < 80) || (LINES < 24))
	{
		logit(LL_ERR, "ERROR, minimal screensize must be 80x24, is %dx%d, terminating!",COLS, LINES);
		do_exit(1);
	}

	noecho();
	raw();

	uheight = ncontroller * 2; /* cards * b-channels */
	lheight = LINES - uheight - 6 + 1; /* rest of display */

	if ((upper_w = newwin(uheight, COLS, UPPER_B, 0)) == NULL)
	{
		logit(LL_ERR, "ERROR, curses init upper window, terminating!");
		exit(1);
	}

	if ((mid_w = newwin(1, COLS, UPPER_B+uheight+1, 0)) == NULL)
	{
		logit(LL_ERR, "ERROR, curses init mid window, terminating!");
		exit(1);
	}

	if ((lower_w = newwin(lheight, COLS, UPPER_B+uheight+3, 0)) == NULL)
	{
		logit(LL_ERR, "ERROR, curses init lower window, LINES = %d, lheight = %d, uheight = %d, terminating!", LINES, lheight, uheight);
		exit(1);
	}

	scrollok(lower_w, 1);

	snprintf(buffer, sizeof(buffer), "----- isdn controller channel state ------------- isdnd %02d.%02d.%d [pid %d] -", VERSION, REL, STEP, (int)getpid());

	while((int)strlen(buffer) < COLS && strlen(buffer) < sizeof(buffer) - 1)
		strlcat(buffer, "-", sizeof(buffer));

	move(0, 0);
	standout();
	addstr(buffer);
	standend();

	move(1, 0);
	/*      01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
	addstr("# tei b remote                 iface  dir outbytes   obps inbytes    ibps  units");

	snprintf(buffer, sizeof(buffer), "----- isdn userland interface state ------------------------------------------");
	while((int)strlen(buffer) < COLS && strlen(buffer) < sizeof(buffer) - 1)
		strlcat(buffer, "-", sizeof(buffer));

	move(uheight+2, 0);
	standout();
	addstr(buffer);
	standend();

	snprintf(buffer, sizeof(buffer), "----- isdnd logfile display --------------------------------------------------");
	while((int)strlen(buffer) < COLS && strlen(buffer) < sizeof(buffer) - 1)
		strlcat(buffer, "-", sizeof(buffer));

	move(uheight+4, 0);
	standout();
	addstr(buffer);
	standend();

	refresh();

	for (ctrl = get_first_ctrl_state(), i=j=0; ctrl; ctrl = NEXT_CTRL(ctrl), i++, j+=2)
	{
		if (ctrl->tei == -1)
			mvwprintw(upper_w, j,   H_CNTL, "%d --- 1 ", i);
		else
			mvwprintw(upper_w, j,   H_CNTL, "%d %3d 1 ", i, ctrl->tei);
		mvwprintw(upper_w, j+1, H_CNTL, "  L12 2 ");
	}
	wrefresh(upper_w);

	for (p = get_first_cfg_entry(), j=0; p; p = NEXT_CFE(p)) {
		mvwprintw(mid_w, 0, j, "%s%d ", p->usrdevicename, p->usrdeviceunit);

		p->fs_position = j;

		j += ((strlen(p->usrdevicename) + (p->usrdeviceunit > 9 ? 2 : 1) + 1));
	}
	wrefresh(mid_w);

	wmove(lower_w, 0, 0);
	wrefresh(lower_w);

	curses_ready = 1;
}

/*---------------------------------------------------------------------------*
 *	curses menu for fullscreen command mode
 *---------------------------------------------------------------------------*/
void
do_menu(void)
{
	static const char *menu[WMITEMS] =
	{
		"1 - (D)isplay refresh",
		"2 - (H)angup (choose a channel)",
		"3 - (R)eread config file",
		"4 - (S)how card types",
		"5 - (B)udget information",
		"6 - (Q)uit the program",
	};

	WINDOW *menu_w;
	int c;
	int mpos;
	struct pollfd set[1];

	/* create a new window in the lower screen area */

	if ((menu_w = newwin(WMENU_HGT, WMENU_LEN, WMENU_POSLN, WMENU_POSCO )) == NULL)
	{
		logit(LL_WRN, "ERROR, curses init menu window!");
		return;
	}

	/* create a border around the window */

	box(menu_w, '|', '-');

	/* add a title */

	wstandout(menu_w);
	mvwaddstr(menu_w, 0, (WMENU_LEN / 2) - (strlen(WMENU_TITLE) / 2), WMENU_TITLE);
	wstandend(menu_w);

	/* fill the window with the menu options */

	for (mpos=0; mpos <= (WMITEMS-1); mpos++)
		mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);

	/* highlight the first menu option */

	mpos = 0;
	wstandout(menu_w);
	mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);
	wstandend(menu_w);

	/* input loop */

	set[0].fd = STDIN_FILENO;
	set[0].events = POLLIN;
	for (;;)
	{
		wrefresh(menu_w);

		/* if no char is available within timeout, exit menu*/

		if ((poll(set, 1, WMTIMEOUT * 1000)) <= 0)
			goto mexit;

		c = wgetch(menu_w);

		switch (c)
		{
		case ' ':
		case '\t':	/* hilite next option */
			mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);
			mpos++;
			if (mpos >= WMITEMS)
				mpos = 0;
			wstandout(menu_w);
			mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);
			wstandend(menu_w);
			break;

		case ('0'+WBUDGET+1):	/* display budget info */
		case 'B':
		case 'b':
			display_budget();
			goto mexit;

		case ('0'+WREFRESH+1):	/* display refresh */
		case 'D':
		case 'd':
			wrefresh(curscr);
			goto mexit;

		case ('0'+WQUIT+1):	/* quit program */
		case 'Q':
		case 'q':
			menuexit(menu_w);
			do_exit(0);
			goto mexit;

		case ('0'+WHANGUP+1):	/* hangup connection */
		case 'H':
		case 'h':
			display_chans();
			goto mexit;

		case ('0'+WREREAD+1):	/* reread config file */
		case 'R':
		case 'r':
			rereadconfig(42);
			goto mexit;

		case ('0'+WSHOW+1):	/* reread config file */
		case 'S':
		case 's':
			display_cards();
			goto mexit;

		case '\n':
		case '\r':	/* exec highlighted option */
			switch (mpos)
			{
			case WREFRESH:
				wrefresh(curscr);
				break;

			case WQUIT:
				menuexit(menu_w);
				do_exit(0);
				break;

			case WHANGUP:
				display_chans();
				break;

			case WREREAD:
				rereadconfig(42);
				break;

			case WBUDGET:
				display_budget();
				break;

			case WSHOW:
				display_cards();
				break;
			}
			goto mexit;
			break;

		default:
			goto mexit;
			break;
		}
	}

mexit:
	menuexit(menu_w);
}

static void
menuexit(WINDOW *menu_w)
{
	int uheight = ncontroller * 2; /* cards * b-channels */
	char buffer[512];

	/* delete the menu window */

	delwin(menu_w);

	/* re-display the original lower window contents */

	touchwin(mid_w);
	wrefresh(mid_w);

	touchwin(lower_w);
	wrefresh(lower_w);

	touchwin(upper_w);
	wrefresh(upper_w);

	move(1, 0);
	/*      01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
	addstr("# tei b remote                 iface  dir outbytes   obps inbytes    ibps  units");

	snprintf(buffer, sizeof(buffer), "----- isdn userland interface state ------------------------------------------");
	while((int)strlen(buffer) < COLS)
		strlcat(buffer, "-", sizeof(buffer));

	move(uheight+2, 0);
	standout();
	addstr(buffer);
	standend();

	snprintf(buffer, sizeof(buffer), "----- isdnd logfile display --------------------------------------------------");
	while((int)strlen(buffer) < COLS)
		strlcat(buffer, "-", sizeof(buffer));

	move(uheight+4, 0);
	standout();
	addstr(buffer);
	standend();

	refresh();
}

/*---------------------------------------------------------------------------*
 *	display the charge in units
 *---------------------------------------------------------------------------*/
void
display_charge(struct cfg_entry *cep)
{
	mvwprintw(upper_w, CHPOS(cep), H_UNITS, "%d", cep->charge);
	wclrtoeol(upper_w);
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display the calculated charge in units
 *---------------------------------------------------------------------------*/
void
display_ccharge(struct cfg_entry *cep, int units)
{
	mvwprintw(upper_w, CHPOS(cep), H_UNITS, "(%d)", units);
	wclrtoeol(upper_w);
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display accounting information
 *---------------------------------------------------------------------------*/
void
display_acct(struct cfg_entry *cep)
{
	mvwprintw(upper_w, CHPOS(cep), H_OUT,    "%-10d", cep->outbytes);
	mvwprintw(upper_w, CHPOS(cep), H_OUTBPS, "%-4d", cep->outbps);
	mvwprintw(upper_w, CHPOS(cep), H_IN,     "%-10d", cep->inbytes);
	mvwprintw(upper_w, CHPOS(cep), H_INBPS,  "%-4d", cep->inbps);
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display connect information
 *---------------------------------------------------------------------------*/
void
display_connect(struct cfg_entry *cep)
{
	char buffer[256];

	/* remote telephone number */

	if (aliasing)
	{
		if (cep->direction == DIR_IN)
			snprintf(buffer, sizeof(buffer), "%s", get_alias(cep->real_phone_incoming));
		else
			snprintf(buffer, sizeof(buffer), "%s", get_alias(cep->remote_phone_dialout));
	}
	else
	{
		if (cep->direction == DIR_IN)
			snprintf(buffer, sizeof(buffer), "%s/%s", cep->name, cep->real_phone_incoming);
		else
			snprintf(buffer, sizeof(buffer), "%s/%s", cep->name, cep->remote_phone_dialout);
	}

	buffer[H_IFN - H_TELN - 1] = '\0';

	mvwprintw(upper_w, CHPOS(cep), H_TELN, "%s", buffer);

	/* interface */

	mvwprintw(upper_w, CHPOS(cep), H_IFN, "%s%d ",
			cep->usrdevicename, cep->usrdeviceunit);

	mvwprintw(upper_w, CHPOS(cep), H_IO,
		cep->direction == DIR_OUT ? "out" : "in");

	mvwprintw(upper_w, CHPOS(cep), H_OUT,    "-");
	mvwprintw(upper_w, CHPOS(cep), H_OUTBPS, "-");
	mvwprintw(upper_w, CHPOS(cep), H_IN,     "-");
	mvwprintw(upper_w, CHPOS(cep), H_INBPS,  "-");

	if (do_bell)
		display_bell();

	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	erase line at disconnect time
 *---------------------------------------------------------------------------*/
void
display_disconnect(struct cfg_entry *cep)
{
	wmove(upper_w, CHPOS(cep),
		 H_TELN);
	wclrtoeol(upper_w);
	wrefresh(upper_w);

	if (do_bell)
		display_bell();

}

/*---------------------------------------------------------------------------*
 *	display interface up/down information
 *---------------------------------------------------------------------------*/
void
display_updown(struct cfg_entry *cep, int updown)
{
	if (updown)
		wstandend(mid_w);
	else
		wstandout(mid_w);

	mvwprintw(mid_w, 0, cep->fs_position, "%s%d ",
			cep->usrdevicename, cep->usrdeviceunit);

	wstandend(mid_w);
	wrefresh(mid_w);
}

/*---------------------------------------------------------------------------*
 *	display interface up/down information
 *---------------------------------------------------------------------------*/
void
display_l12stat(int controller, int layer, int state)
{
	if (controller > ncontroller)
		return;
	if (!(layer == 1 || layer == 2))
		return;

	if (state)
		wstandout(upper_w);
	else
		wstandend(upper_w);

	if (layer == 1)
	{
		mvwprintw(upper_w, (controller*2)+1, H_TEI+1, "1");
		if (!state)
			mvwprintw(upper_w, (controller*2)+1, H_TEI+2, "2");
	}
	else if (layer == 2)
	{
		mvwprintw(upper_w, (controller*2)+1, H_TEI+2, "2");
		if (state)
			mvwprintw(upper_w, (controller*2)+1, H_TEI+1, "1");
	}

	wstandend(upper_w);
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display TEI
 *---------------------------------------------------------------------------*/
void
display_tei(int controller, int tei)
{
	if (controller > ncontroller)
		return;

	if (tei == -1)
		mvwprintw(upper_w, controller*2, H_TEI, "---");
	else
		mvwprintw(upper_w, controller*2, H_TEI, "%3d", tei);

	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display bell :-)
 *---------------------------------------------------------------------------*/
void
display_bell(void)
{
	static char bell[1] = { 0x07 };
	write(STDOUT_FILENO, &bell[0], 1);
}

/*---------------------------------------------------------------------------*
 *	display channel information for shutdown
 *---------------------------------------------------------------------------*/
void
display_chans(void)
{
	char buffer[80];
	int i, j, cnt = 0;
	WINDOW *chan_w;
	int nlines, ncols, pos_x, pos_y;
	struct pollfd set[1];
	struct cfg_entry *cep = NULL;
	struct isdn_ctrl_state *ctrl;

	/* need this later to close the connection */
	struct ctlr_chan {
		int cntl;
		int chn;
	} *cc = NULL;

	for (ctrl = get_first_ctrl_state(); ctrl; ctrl = NEXT_CTRL(ctrl)) {
		if ((get_controller_state(ctrl)) != CTRL_UP)
			continue;
		for (j = 0; j < ctrl->nbch; j++)
			if ((ret_channel_state(ctrl, j)) == CHAN_RUN)
				cnt++;
	}

	if (cnt > 0)
	{
		if ((cc = (struct ctlr_chan *)malloc (cnt *
			sizeof (struct ctlr_chan))) == NULL)
		{
			return;
		}
		nlines = cnt + 4;
		ncols = 60;
	}
	else
	{
		nlines = 5;
		ncols = 22;
	}

	pos_y = WMENU_POSLN + 4;
	pos_x = WMENU_POSCO + 10;

	/* create a new window in the lower screen area */

	if ((chan_w = newwin(nlines, ncols, pos_y, pos_x )) == NULL)
	{
		logit(LL_WRN, "ERROR, curses init channel window!");
		if (cnt > 0)
			free(cc);
		return;
	}

	/* create a border around the window */

	box(chan_w, '|', '-');

	/* add a title */

	wstandout(chan_w);
	mvwaddstr(chan_w, 0, (ncols / 2) - (strlen("Channels") / 2), "Channels");
	wstandend(chan_w);

	/* no active channels */
	if (cnt == 0)
	{
		mvwaddstr(chan_w, 2, 2, "No active channels");
		wrefresh(chan_w);
		sleep(1);

		/* delete the channels window */

		delwin(chan_w);
		return;
	}

	nlines = 2;
	ncols = 1;

	for (ctrl = get_first_ctrl_state(), i = 0; ctrl; ctrl = NEXT_CTRL(ctrl), i++) {
		if ((get_controller_state(ctrl)) != CTRL_UP)
			continue;

		for (j = 0; j < ctrl->nbch; i++)
		{
			if ((ret_channel_state(ctrl, j)) == CHAN_RUN)
			{
				snprintf(buffer, sizeof(buffer), "%d - Controller %d channel B%d", ncols, i, j);
				mvwaddstr(chan_w, nlines, 2, buffer);
				cc[ncols - 1].cntl = i;
				cc[ncols - 1].chn = j;
				nlines++;
				ncols++;
			}
		}
	}

	set[0].fd = STDIN_FILENO;
	set[0].events = POLLIN;
	for (;;)
	{
		wrefresh(chan_w);

		/* if no char is available within timeout, exit menu*/

		if ((poll(set, 1, WMTIMEOUT * 1000)) <= 0)
			break;

		ncols = wgetch(chan_w);

		if (!(isdigit(ncols)))
		{
			display_bell();
			continue;
		}

		nlines = ncols - '0';

		if ((nlines == 0) || (nlines > cnt))
		{
			display_bell();
			continue;
		}

		if ((cep = get_cep_by_cc(cc[nlines-1].cntl, cc[nlines-1].chn))
			!= NULL)
		{
			logit(LL_CHD, "%05d %s manual disconnect (fullscreen menu)", cep->cdid, cep->name);
			cep->hangup = 1;
			break;
		}
	}

	free(cc);

	/* delete the channels window */

	delwin(chan_w);
}

/*---------------------------------------------------------------------------*
 *	display card type information
 *---------------------------------------------------------------------------*/
static void
display_cards(void)
{
	WINDOW *chan_w;
	int nlines, ncols, pos_x, pos_y;
	struct pollfd set[1];
	int i;
	struct isdn_ctrl_state *ctrl;

	nlines = 6+ncontroller;
	ncols = 60;
	pos_y = WMENU_POSLN;
	pos_x = WMENU_POSCO;

	/* create a new window in the lower screen area */

	if ((chan_w = newwin(nlines, ncols, pos_y, pos_x )) == NULL)
	{
		logit(LL_WRN, "ERROR, curses init channel window!");
		return;
	}

	/* create a border around the window */

	box(chan_w, '|', '-');

	/* add a title */

	wstandout(chan_w);
	mvwaddstr(chan_w, 0, (ncols / 2) - (strlen("Cards") / 2), "Cards");
	wstandend(chan_w);

	mvwprintw(chan_w, 2, 2, "ctrl device");
	mvwprintw(chan_w, 3, 2, "---- ----------------------------------------------");
	for (i = 0, ctrl = get_first_ctrl_state(); ctrl; ctrl = NEXT_CTRL(ctrl), i++)
	{
		mvwprintw(chan_w, 4+i, 2, " #%d  %s: %s", i,
		    ctrl->device_name,
		    ctrl->controller);
	}

	wrefresh(chan_w);

	set[0].fd = STDIN_FILENO;
	set[0].events = POLLIN;

	if ((poll(set, 1, WMTIMEOUT*2 * 1000)) <= 0)
	{
		delwin(chan_w);
		return;
	}

	wgetch(chan_w);
	delwin(chan_w);
}

/*---------------------------------------------------------------------------*
 *	display budget info
 *---------------------------------------------------------------------------*/
static void
display_budget(void)
{
	WINDOW *bud_w;
	int nlines, ncols, pos_x, pos_y;
	struct pollfd set[1];
	int j;
	struct cfg_entry *cep;
	time_t now;
	double uptime;
	int minutes;
	int hours;
	int days;

	nlines = 0;
	ncols = 73;
	pos_y = WMENU_POSLN;
	pos_x = WMENU_POSCO-3;

	for (cep = get_first_cfg_entry(), j=0; cep; cep = NEXT_CFE(cep)) {
		if (cep->budget_callbackperiod && cep->budget_callbackncalls)
			nlines++;
		if (cep->budget_calloutperiod && cep->budget_calloutncalls)
			nlines++;
	}

	if (nlines == 0)
		return;

	nlines += 6;

	/* create a new window in the lower screen area */

	if ((bud_w = newwin(nlines, ncols, pos_y, pos_x )) == NULL)
	{
		logit(LL_WRN, "ERROR, curses init budget window!");
		return;
	}

	now = time(NULL);
	uptime = difftime(now, starttime);

	minutes = (time_t) (uptime / 60) % 60;
	hours = (time_t) (uptime / (60*60)) % (60*60);
	days = (time_t) (uptime / (60*60*24)) % (60*60*24);

	uptime = uptime / (60*60);

	/* create a border around the window */

	box(bud_w, '|', '-');

	/* add a title */

	wstandout(bud_w);
	mvwaddstr(bud_w, 0, (ncols / 2) - (strlen("Budget") / 2), "Budget");
	wstandend(bud_w);

	mvwprintw(bud_w, 1, 2, "isdnd uptime: %d %s - %d %s - %d %s",
		days,
		days == 1 ? "day" : "days",
		hours,
		hours == 1 ? "hour" : "hours",
		minutes,
		minutes == 1 ? "minute" : "minutes");

	mvwprintw(bud_w, 2, 2, "name     t period rest   ncall rest  rqsts /hr  rdone /hr  rrjct /hr ");
	mvwprintw(bud_w, 3, 2, "-------- - ------ ------ ----- ----- ----- ---- ----- ---- ----- ----");

	for (cep = get_first_cfg_entry(), j=4; cep; cep = NEXT_CFE(cep)) {
		if (cep->budget_calloutperiod && cep->budget_calloutncalls)
		{
			mvwprintw(bud_w, j, 2, "%-8s %c %-6d %-6ld %-5d %-5d %-5d %-4.1f %-5d %-4.1f %-5d %-4.1f",
				cep->name,
				'o',
				cep->budget_calloutperiod,
				(long)(cep->budget_calloutperiod_time - now),
				cep->budget_calloutncalls,
				cep->budget_calloutncalls_cnt,
				cep->budget_callout_req,
				(double)cep->budget_callout_req / uptime,
				cep->budget_callout_done,
				(double)cep->budget_callout_done / uptime,
				cep->budget_callout_rej,
				(double)cep->budget_callout_rej / uptime);
			j++;
		}
		if (cep->budget_callbackperiod && cep->budget_callbackncalls)
		{
			mvwprintw(bud_w, j, 2, "%-8s %c %-6d %-6ld %-5d %-5d %-5d %-4.1f %-5d %-4.1f %-5d %-4.1f",
				(cep->budget_calloutperiod && cep->budget_calloutncalls) ? "" : cep->name,
				'b',
				cep->budget_callbackperiod,
				(long)(cep->budget_callbackperiod_time - now),
				cep->budget_callbackncalls,
				cep->budget_callbackncalls_cnt,
				cep->budget_callback_req,
				(double)cep->budget_callback_req / uptime,
				cep->budget_callback_done,
				(double)cep->budget_callback_done / uptime,
				cep->budget_callback_rej,
				(double)cep->budget_callback_rej / uptime);
			j++;
		}
	}

	wrefresh(bud_w);

	set[0].fd = STDIN_FILENO;
	set[0].events = POLLIN;

	if ((poll(set, 1, WMTIMEOUT*3 * 1000)) <= 0)
	{
		delwin(bud_w);
		return;
	}

	wgetch(bud_w);
	delwin(bud_w);
}

#endif

/* EOF */
