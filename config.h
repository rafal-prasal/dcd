// $Id: config.h.in,v 1.16 2003/01/10 10:58:23 blues Exp $

/* ---------------------------------------------------------------- 
 *  DConnect Damon
 * 
 *      #(@) Copyright (c) 2002, DConnect development team
 *      #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 *  This  program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of
 *  the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY  or  FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You  should  have  received a copy  of  the GNU General Public
 *  License  along  with  this  program;  if  not,  write  to  the
 *  Free  Software  Foundation,  Inc.,  675  Mass  Ave,  Cambridge,
 *  MA 02139, USA. 
 *
 *  config.h: defines some platform and OS dependent macros 
 * ---------------------------------------------------------------- */
 
#if !defined( __vXhz__CONFIG_H__INCLUDED__ )
#define __vXhz__CONFIG_H__INCLUDED__

 
/* UINT_MAX and others need to be defined */
#include <limits.h>

/* 
 * This is checked in configure - no need to have it here... (blues)
 * threads support
 * #undef  HAVE_MITPTHREADS
 */
#undef  HAVE_BROKENMITPTHREADS

/* os & cpu type */
#define HAVE_LINUX       0
#define HAVE_LINUX_LIBC5 0

#define HOST_OS          "unknown"
#define HOST_CPU         "unknown"

/* additional optimization */
#undef  ASM_X86
#undef  ASM_X86_OLD

#if defined( HAVE_BROKENMITPTHREADS )
#define _MIT_POSIX_THREADS
#endif /* HAVE_BROKEN_THREADS */

#if defined( HAVE_LINUX ) || defined( HAVE_LINUX_LIBC5 )
#define _LINUX
#endif /* HAVE_LINUX */

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
#undef CRAY_STACKSEG_END

/* Define if using `alloca.c'. */
#undef C_ALLOCA

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
#undef ENABLE_NLS

/* Define if you have `alloca', as a function or macro. */
#undef HAVE_ALLOCA

/* Define if you have <alloca.h> and it should be used (not on Ultrix). */
#undef HAVE_ALLOCA_H

/* Define if you have the <argz.h> header file. */
#undef HAVE_ARGZ_H

/* Define if you have the <arpa/inet.h> header file. */
#undef HAVE_ARPA_INET_H

/* Define if you have the `bzero' function. */
#undef HAVE_BZERO

/* Define if you have the `dcgettext' function. */
#undef HAVE_DCGETTEXT

/* Define if you have the <dlfcn.h> header file. */
#undef HAVE_DLFCN_H

/* Define if you don't have `vprintf' but do have `_doprnt.' */
#undef HAVE_DOPRNT

/* Define if you have the <errno.h> header file. */
#undef HAVE_ERRNO_H

/* Define if you have the <fcntl.h> header file. */
#undef HAVE_FCNTL_H

/* Define if you have the `feof_unlocked' function. */
#undef HAVE_FEOF_UNLOCKED

/* Define if you have the `fgets_unlocked' function. */
#undef HAVE_FGETS_UNLOCKED

/* Define if you have the `getcwd' function. */
#undef HAVE_GETCWD

/* Define if you have the `getegid' function. */
#undef HAVE_GETEGID

/* Define if you have the `geteuid' function. */
#undef HAVE_GETEUID

/* Define if you have the `getgid' function. */
#undef HAVE_GETGID

/* Define if you have the `getpagesize' function. */
#undef HAVE_GETPAGESIZE

/* Define if the GNU gettext() function is already present or preinstalled. */
#undef HAVE_GETTEXT

/* Define if you have the `getuid' function. */
#undef HAVE_GETUID

/* Define if you have the iconv() function. */
#undef HAVE_ICONV

/* Define if you have the `inet_ntoa' function. */
#undef HAVE_INET_NTOA

/* Define if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#undef HAVE_LANGINFO_CODESET

/* Define if your <locale.h> file defines LC_MESSAGES. */
#undef HAVE_LC_MESSAGES

/* Define if you have the `nsl' library (-lnsl). */
#undef HAVE_LIBNSL

/* Define if you have the `pthread' library (-lpthread). */
#undef HAVE_LIBPTHREAD

/* Define if you have the `socket' library (-lsocket). */
#undef HAVE_LIBSOCKET

/* Define if you have the <limits.h> header file. */
#undef HAVE_LIMITS_H

/* Define if you have the <locale.h> header file. */
#undef HAVE_LOCALE_H

/* Define if your system has a working `malloc' function. */
#undef HAVE_MALLOC

/* Define if you have the <malloc.h> header file. */
#undef HAVE_MALLOC_H

/* Define if you have the <memory.h> header file. */
#undef HAVE_MEMORY_H

/* Define if you have the `mempcpy' function. */
#undef HAVE_MEMPCPY

/* Define if you have a working `mmap' system call. */
#undef HAVE_MMAP

/* Define if you have the `munmap' function. */
#undef HAVE_MUNMAP

/* Define if you have the <netdb.h> header file. */
#undef HAVE_NETDB_H

/* Define if you have the <netinet/in.h> header file. */
#undef HAVE_NETINET_IN_H

/* Define if you have the <nl_types.h> header file. */
#undef HAVE_NL_TYPES_H

/* Process all options of libwrap */
#undef HAVE_PROCESS_OPTIONS

/* Define if you have the `putenv' function. */
#undef HAVE_PUTENV

/* Define if you have the `select' function. */
#undef HAVE_SELECT

/* Define if you have the `setenv' function. */
#undef HAVE_SETENV

/* Define if you have the `setlocale' function. */
#undef HAVE_SETLOCALE

/* Define if you have the `socket' function. */
#undef HAVE_SOCKET

/* Define if you have the <stddef.h> header file. */
#undef HAVE_STDDEF_H

/* Define if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define if you have the <stdlib.h> header file. */
#undef HAVE_STDLIB_H

/* Define if you have the `stpcpy' function. */
#undef HAVE_STPCPY

/* Define if you have the `strcasecmp' function. */
#undef HAVE_STRCASECMP

/* Define if you have the `strchr' function. */
#undef HAVE_STRCHR

/* Define if you have the `strdup' function. */
#undef HAVE_STRDUP

/* Define if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define if you have the <string.h> header file. */
#undef HAVE_STRING_H

/* Define if you have the `strtoul' function. */
#undef HAVE_STRTOUL

/* Define if you have the <sys/ioctl.h> header file. */
#undef HAVE_SYS_IOCTL_H

/* Define if you have the <sys/param.h> header file. */
#undef HAVE_SYS_PARAM_H

/* Define if you have the <sys/socket.h> header file. */
#undef HAVE_SYS_SOCKET_H

/* Define if you have the <sys/stat.h> header file. */
#undef HAVE_SYS_STAT_H

/* Define if you have the <sys/time.h> header file. */
#undef HAVE_SYS_TIME_H

/* Define if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define if you have the `tsearch' function. */
#undef HAVE_TSEARCH

/* Define if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define if you have the `vprintf' function. */
#undef HAVE_VPRINTF

/* Define if you have the `__argz_count' function. */
#undef HAVE___ARGZ_COUNT

/* Define if you have the `__argz_next' function. */
#undef HAVE___ARGZ_NEXT

/* Define if you have the `__argz_stringify' function. */
#undef HAVE___ARGZ_STRINGIFY

/* Define as const if the declaration of iconv() needs const. */
#undef ICONV_CONST

/* Name of package */
#undef PACKAGE

/* Define as the return type of signal handlers (`int' or `void'). */
#undef RETSIGTYPE

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
        STACK_DIRECTION > 0 => grows toward higher addresses
        STACK_DIRECTION < 0 => grows toward lower addresses
        STACK_DIRECTION = 0 => direction of growth unknown */
#undef STACK_DIRECTION

/* Define if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Define if you can safely include both <sys/time.h> and <time.h>. */
#undef TIME_WITH_SYS_TIME

/* Version number of package */
#undef VERSION

/* Define to empty if `const' does not conform to ANSI C. */
#undef const

/* Define as `__inline' if that's what the C compiler calls it, or to nothing
   if it is not supported. */
#undef inline

/* Define to `long' if <sys/types.h> does not define. */
#undef off_t

/* Define to `unsigned' if <sys/types.h> does not define. */
#undef size_t

/* -- dependencies -- */

#if defined( HAVE_UNISTD_H )
#define RD_F_BINARY O_RDONLY
#elif defined( HAVE_IO_H )
#define RD_F_BINARY O_RDONLY | O_BINARY
#endif /* HAVE_IO_H */

/* -- datatypes -- */

#if UCHAR_MAX == 0xff

typedef   signed char   _int8b;
typedef unsigned char   uint8b;

#else
#error "this machine has no 8-bit integer type"
#endif

#if UINT_MAX == 0xffff

typedef   signed int    _int16b;
typedef unsigned int    uint16b;

#elif USHRT_MAX == 0xffff

typedef   signed short  _int16b;
typedef unsigned short  uint16b;

#else
#error "this machine has no 16-bit integer type"
#endif

#if UINT_MAX == 0xfffffffful

typedef          int	_int32b;
typedef unsigned int	uint32b;

#elif ULONG_MAX == 0xfffffffful

typedef          long	_int32b;
typedef unsigned long	uint32b;

#elif USHRT_MAX == 0xfffffffful

typedef          short _int32b;
typedef unsigned short uint32b;

#else
#error "this machine has no 32-bit integer type"
#endif

typedef double real;

/* -- compatibility -- */

#define BYTE        uint8b
#define WORD        uint16b
#define LONGINT     _int32b
#define DWORD       uint32b

#define UINT        DWORD
#define ULONG       unsigned long

#undef  TRUE
#if defined( __cplusplus )
#define TRUE        true
#else /* __cplusplus */
#define TRUE        1
#endif /* !__cplusplus */

#undef  FALSE
#if defined( __cplusplus )
#define FALSE       false
#else /* __cplusplus */
#define FALSE       0
#endif /* !__cplusplus */

#undef  BOOL
#if defined( __cplusplus )
#define BOOL        bool
#else /* __cplusplus */
#define BOOL        uint16b
#endif /* !__cplusplus */

#if !defined( WIN32 )
#define LRESULT     uint32b
#endif /* !WIN32 */

#undef NULL
#if defined( __cplusplus )
#define NULL        0
#else /* __cplusplus */
#define NULL        ((void *)0)
#endif /* !__cplusplus */

#if defined( WIN32 )
#define _EOL        "\r\n"
#else /* WIN32 */
#define _EOL        "\n"
#endif /* !WIN32 */

#if defined( WIN32 )
#define _PATH       '\\'
#define _PATH_STR   "\\"
#else /* WIN32 */
#define _PATH       '/'
#define _PATH_STR   "/"
#endif /* !WIN32 */

/* -- helper macros -- */

#define DELETE_PTR( _p ) \
if ( _p != NULL ) { \
	free( _p ); _p = NULL; \
} \

/* -- just in case definitions -- */

// const arguments
#define _vXhz_CONST        const

// standard function call
#if defined( WIN32 )
#define _vXhz_STDCALL  __stdcall
#else /* WIN32 */
#define _vXhz_STDCALL
#endif /* !WIN32 */

// inline functions
#if defined( WIN32 )
#define _vXhz_INLINE  __inline
#else /* WIN32 */
#define _vXhz_INLINE    inline
#endif /* !WIN32 */

// library export/import
#if defined( WIN32 )

/* if target platform is Win32, part of the application is 
   compiled as dll. we then need compiler dependent export 
   and import specifiers */

#if defined( __BORLANDC__ )

/* Bornald C/C++ */
#define _vXhz_EXPORT \
  __export _vXhz_STDCALL

#define _vXhz_IMPORT \
  __export _vXhz_STDCALL

#elif defined( _MSC_VER )

/* M$ Visual C/C++ */
#define _vXhz_EXPORT \
  __declspec( dllexport ) _vXhz_STDCALL

#define _vXhz_IMPORT \
  __declspec( dllimport ) _vXhz_STDCALL

#endif /* __BORLANDC__ && _MSC_VER */

/* if we are building library, functions should be declared
   as exportable, otherwise there should be import specifiers */

#if defined( _DLL )

#if defined( _vXhz_LIBRARY_ )
#define _vXhz_LIB_ENTRY     _vXhz_EXPORT
#else	/* _vXhz_LIBRARY */
#define _vXhz_LIB_ENTRY     _vXhz_IMPORT
#endif /* !_vXhz_LIBRARY_ */

#else /* _DLL */ 
#define _vXhz_LIB_ENTRY
#endif /* !_DLL */

#else /* WIN32 */
#define _vXhz_LIB_ENTRY

#endif /* !WIN32 */

/* just in case further modifications... */
typedef char * char_ptr;
typedef _vXhz_CONST char * const_char_ptr;

#endif /* __vXhz__CONFIG_H__INCLUDED__ */

