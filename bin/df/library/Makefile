#	$NetBSD: Makefile,v 1.19 2011/08/28 08:20:58 christos Exp $
#	@(#)Makefile	8.3 (Berkeley) 5/8/95

.include <bsd.own.mk>

LIB=	system_df

INCS=		df.h
INCSDIR=	/usr/include/system

.PATH:	..

LDADD+=-lutil -lprop
DPADD+=${LIBUTIL} ${LIBPROP}
# LIBDPLIBS+=     curses  ${.CURDIR}/../libcurses

CPPFLAGS+=	-DSYSTEM_LIBRARY

.include <bsd.lib.mk>
