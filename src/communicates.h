/* $Id: communicates.h,v 1.4 2006/03/20 01:07:40 rav Exp $ */

#if !defined(__COMMUNICATES_H_INCLUDED__)
#define __COMMUNICATES_H_INCLUDED__

	#ifdef USE_GETTEXT

		#include <libintl.h>
		#include <locale.h>
		#define _(String) gettext(String)

	#else

		#define _(String) String

	#endif

#endif
