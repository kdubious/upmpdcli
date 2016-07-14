/* Copyright (C) 2016 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* conf_post: manual part included by auto-generated config.h. Avoid
 * being clobbered by autoheader, and undefine some problematic
 * symbols.
 */

// Get rid of macro names which could conflict with other package's
#if defined(UPMPDCLI_NEED_PACKAGE_VERSION) && \
    !defined(UPMPDCLI_PACKAGE_VERSION_DEFINED)
#define UPMPDCLI_PACKAGE_VERSION_DEFINED
static const char *UPMPDCLI_PACKAGE_VERSION = PACKAGE_VERSION;
#endif

#define UPMPDCLI_SOURCE

#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION
