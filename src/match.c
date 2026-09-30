/* $Id: match.c,v 1.14 2006/03/20 01:03:55 rav Exp $ */

/* Originaly written by:
 *
 * Chris Fuller (aka Fred1@IRC & Fwitz@IRC)
 *	 crf@cfox.bchs.uh.edu
 *
 * I hereby release this code into the public domain */

#include "pch.h"

/* The quoting character -- what overrides wildcards (do not undef) */
#define QUOTE '\\'
/* The "matches ANYTHING" wildcard (do not undef) */
#define WILDS '*'
/* The "matches ANY NUMBER OF NON-SPACE CHARS" wildcard (do not undef) */
#define WILDP '%'
/* The "matches EXACTLY ONE CHARACTER" wildcard (do not undef) */
#define WILDQ '?'

#define UNQUOTED (0x7FFF)
#define QUOTED (0x8000)

int match(char *m, char *n) {
	char *ma = m, *na = n, *lsm = 0, *lsn = 0;
	int match = 1;
	register int sofar = 0;

	/* take care of null strings (should never match) */
	if ((ma == 0) || (na == 0) || (!*ma) || (!*na))
		return 0;
	/* find the end of each string */
	while (*(++m));
	m--;
	while (*(++n));
	n--;

	while (n >= na) {
		/* The wildcard-zap below can move m one before the pattern start
		   (e.g. a pattern that is just "*"); reading *m there is out of
		   bounds. The pattern is then fully consumed -- fall through to the
		   tail, which returns a match. */
		if (m < ma) break;
		if ((m <= ma) || (m[-1] != QUOTE)) {	/* Only look if no quote */
			switch (*m) {
				case WILDS:		/* Matches anything */
					do
						m--;	/* Zap redundant wilds */
					while ((m >= ma) && ((*m == WILDS) || (*m == WILDP)));
					if ((m >= ma) && (*m == '\\'))
						m++;	/* Keep quoted wildcard! */
					lsm = m;
					lsn = n;
					match += sofar;
					sofar = 0;	/* Update fallback pos */
					continue;	/* Next char, please */
				case WILDQ:
					m--;
					n--;
					continue;	/* '?' always matches */
			}
			sofar &= UNQUOTED;	/* Remember not quoted */
		} else
			sofar |= QUOTED;	/* Remember quoted */
		if (!strncasecmp(m,n,1)) {	/* If matching char */
			m--;
			n--;
			sofar++;		/* Tally the match */
			if (sofar & QUOTED)
				m--;		/* Skip the quote char */
			continue;		/* Next char, please */
		}
		if (lsm) {			/* To to fallback on '*' */
			n = --lsn;
			m = lsm;
			if (n < na)
				lsm = 0;	/* Rewind to saved pos */
			sofar = 0;
			continue;		/* Next char, please */
		}
		return 0;			/* No fallback=No match */
	}
	while ((m >= ma) && ((*m == WILDS) || (*m == WILDP)))
		m--;				/* Zap leftover %s & *s */
	return (m >= ma) ? 0 : 1;		/* Start of both = match */
}

/* VIM Settings {{{
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * soft-stop-width: 4
 * c indent on
 * End:
 * vim600: sw=4 ts=4 sts=4 cindent fdm=marker
 * vim<600: sw=4 ts=4
 * }}} */

