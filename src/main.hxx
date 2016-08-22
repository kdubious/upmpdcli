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
#ifndef _MAIN_H_X_INCLUDED_
#define _MAIN_H_X_INCLUDED_

#include <string>
#include <unordered_set>
#include <mutex>

extern std::string g_configfilename;
extern std::string g_datadir;
class ConfSimple;
extern std::mutex g_configlock;
extern ConfSimple *g_config;
extern std::string g_protocolInfo;
extern std::unordered_set<std::string> g_supportedFormats;

typedef struct ohInfoDesc {
    std::string name;
    std::string info;
    std::string url;
    std::string imageUri;
} ohInfoDesc_t;

typedef struct ohProductDesc {
    ohInfoDesc_t manufacturer;
    ohInfoDesc_t model;
    ohInfoDesc_t product;
    std::string room;
} ohProductDesc_t;


#endif /* _MAIN_H_X_INCLUDED_ */
