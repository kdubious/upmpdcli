/* Copyright (C) 2017-2018 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _SYSVIPC_H_INCLUDED_
#define _SYSVIPC_H_INCLUDED_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

class ShmSeg {
public:
    ShmSeg(key_t ky, size_t size, bool create = false, int perms = 0600);
    ShmSeg(const char*pathname, int proj_id, size_t size,  bool create = false,
           int perms = 0600);
    ShmSeg(int size) : ShmSeg(IPC_PRIVATE, size) {}
    virtual ~ShmSeg();
    virtual void setremove(bool onoff = true);
    virtual void *getseg();
    virtual size_t getsize();
    virtual bool ok();
    virtual int geterrno();
    
    class Internal;
protected:
    Internal *m;
};

// Shm segment with integrated lock. Note that the lock initialization
// is not protected, and that an external method is needed for the
// creating process to be left alone while it is creating the segment
// and initializing the lock.
// 
// You need to use an Accessor object to get to the segment. This
// takes the lock on construction and releases it on deletion
class LockableShmSeg : public ShmSeg {
public:
    LockableShmSeg(key_t ky, size_t size, bool create = false, int perms = 0600);
    LockableShmSeg(const char*pathname, int proj_id, size_t size,
                    bool create = false, int perms = 0600);
    LockableShmSeg(int size) : LockableShmSeg(IPC_PRIVATE, size) {}
    virtual ~LockableShmSeg();
    class Accessor {
    public:
        Accessor(LockableShmSeg&);
        ~Accessor();
        void *getseg();
    private:
        LockableShmSeg& lss;
    };
    friend class Accessor;
private:
    virtual void *getseg();
};

#endif /* _SYSVIPC_H_INCLUDED_ */
