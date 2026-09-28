/* ---------------------------------------------------------------- 
 *  DConnect Daemon
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
 *  config.h: platform configuration.
 *  All build-time definitions (CONFIGDIR, DEFAULT_USER, PACKAGE_*,
 *  BUILD_TIME, ...) are provided by CMake as compile definitions;
 *  this header only pulls in the system limits required by the
 *  sources.
 * ---------------------------------------------------------------- */

#if !defined( __vXhz__CONFIG_H__INCLUDED__ )
#define __vXhz__CONFIG_H__INCLUDED__

/* PATH_MAX and friends are used by the sources */
#include <limits.h>

#endif /* __vXhz__CONFIG_H__INCLUDED__ */
