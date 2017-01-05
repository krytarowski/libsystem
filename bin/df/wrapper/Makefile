#	$NetBSD: Makefile,v 1.19 2011/08/28 08:20:58 christos Exp $
#	@(#)Makefile	8.3 (Berkeley) 5/8/95

.include <bsd.own.mk>

PROG=	df
.PATH:	..

LIBSYSTEMDFDIR!= cd ${.CURDIR}/../library && ${PRINTOBJDIR}
LDADD+=-lprop -L${LIBSYSTEMDFDIR} -lsystem_df
DPADD+=${LIBSYSTEMDFDIR}/libsystem_df.a

CPPFLAGS+=	-DSYSTEM_WRAPPER

.include <bsd.prog.mk>
