#	$NetBSD: Makefile,v 1.19 2011/08/28 08:20:58 christos Exp $
#	@(#)Makefile	8.3 (Berkeley) 5/8/95

.include <bsd.own.mk>

PROG=	df
#SRCS=	../df.c
.PATH:	..

LDADD+=-lutil -lprop
DPADD+=${LIBUTIL} ${LIBPROP}

CPPFLAGS+=	-DSYSTEM_STANDALONE

.include <bsd.prog.mk>
