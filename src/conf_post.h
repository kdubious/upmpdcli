
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

// conf_post: manual part included by auto-generated config.h. Avoid
// being clobbered by autoheader, and undefine some problematic
// symbols.

// Get rid of macro names which could conflict with other package's
#if defined(UPMPDCLI_NEED_PACKAGE_VERSION) && \
    !defined(UPMPDCLI_PACKAGE_VERSION_DEFINED)
#define UPMPDCLI_PACKAGE_VERSION_DEFINED
static const char *UPMPDCLI_PACKAGE_VERSION = PACKAGE_VERSION;
#endif
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION

#define HAVE_CXX0X_UNORDERED
#define HAVE_SHARED_PTR_STD
// upmpdcli: requires c++0x but we still need the defs for c++
// versions below because of execmd et al.

#ifdef  HAVE_CXX0X_UNORDERED
#  define UNORDERED_MAP_INCLUDE <unordered_map>
#  define UNORDERED_SET_INCLUDE <unordered_set>
#  define STD_UNORDERED_MAP std::unordered_map
#  define STD_UNORDERED_SET std::unordered_set
#elif defined(HAVE_TR1_UNORDERED)
#  define UNORDERED_MAP_INCLUDE <tr1/unordered_map>
#  define UNORDERED_SET_INCLUDE <tr1/unordered_set>
#  define STD_UNORDERED_MAP std::tr1::unordered_map
#  define STD_UNORDERED_SET std::tr1::unordered_set
#else
#  define UNORDERED_MAP_INCLUDE <map>
#  define UNORDERED_SET_INCLUDE <set>
#  define STD_UNORDERED_MAP std::map
#  define STD_UNORDERED_SET std::set
#endif

#ifdef HAVE_SHARED_PTR_STD
#  define MEMORY_INCLUDE <memory>
#  define STD_SHARED_PTR    std::shared_ptr
#elif defined(HAVE_SHARED_PTR_TR1)
#  define MEMORY_INCLUDE <tr1/memory>
#  define STD_SHARED_PTR    std::tr1::shared_ptr
#else
#  define MEMORY_INCLUDE "refcntr.h"
#  define STD_SHARED_PTR    RefCntr
#endif
