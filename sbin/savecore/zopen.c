/*	$NetBSD: zopen.c,v 1.3 2012/12/25 09:24:45 mbalmer Exp $	*/

/*
 * Public domain stdio wrapper for libz, written by Johan Danielsson.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: zopen.c,v 1.3 2012/12/25 09:24:45 mbalmer Exp $");
#endif

#include <stdio.h>
#include <zlib.h>

FILE *zopen(const char *fname, const char *mode);

/* convert arguments */
static int
xgzread(void *cookie, char *data, int size)
{
    return gzread(cookie, data, size);
}

static int
xgzwrite(void *cookie, const char *data, int size)
{
    return gzwrite(cookie, __UNCONST(data), size);
}

FILE *
zopen(const char *fname, const char *mode)
{
    gzFile gz = gzopen(fname, mode);
    if(gz == NULL)
	return NULL;

    if(*mode == 'r')
	return funopen(gz, xgzread, NULL, NULL, gzclose);
    else
	return funopen(gz, NULL, xgzwrite, NULL, gzclose);
}
