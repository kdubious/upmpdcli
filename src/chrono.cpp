#ifndef TEST_CHRONO
/* Copyright (C) 2014 J.F.Dockes
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

// Measure and display time intervals.

#include "chrono.h"

#include <chrono>

using namespace std;

Chrono::TimePoint Chrono::o_now;

void Chrono::refnow()
{
    o_now = chrono::steady_clock::now();
}

Chrono::Chrono()
    : m_orig(chrono::steady_clock::now())
{
}

long Chrono::restart()
{
    auto nnow = chrono::steady_clock::now();
    auto ms =
        chrono::duration_cast<chrono::milliseconds>(nnow - m_orig);
    m_orig = nnow;
    return ms.count();
}

long Chrono::urestart()
{
    auto nnow = chrono::steady_clock::now();
    auto ms =
        chrono::duration_cast<chrono::microseconds>(nnow - m_orig);
    m_orig = nnow;
    return ms.count();
}

long Chrono::millis(bool frozen)
{
    if (frozen) {
        return chrono::duration_cast<chrono::milliseconds>
            (o_now - m_orig).count();
    } else {
        return chrono::duration_cast<chrono::milliseconds>
            (chrono::steady_clock::now() - m_orig).count();
    }
}

long Chrono::micros(bool frozen)
{
    if (frozen) {
        return chrono::duration_cast<chrono::microseconds>
            (o_now - m_orig).count();
    } else {
        return chrono::duration_cast<chrono::microseconds>
            (chrono::steady_clock::now() - m_orig).count();
    }
}

float Chrono::secs(bool frozen)
{
    if (frozen) {
        return chrono::duration_cast<chrono::seconds>
            (o_now - m_orig).count();
    } else {
        return (chrono::duration_cast<chrono::seconds>
                (chrono::steady_clock::now() - m_orig)).count();
    }
}

#else

// Test
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include "chrono.h"

using namespace std;

static char *thisprog;
static void
Usage(void)
{
    fprintf(stderr, "Usage : %s \n", thisprog);
    exit(1);
}

Chrono achrono;
Chrono rchrono;

void
showsecs(long msecs)
{
    fprintf(stderr, "%3.5f S", (double(msecs)) / 1000.0);
}

void
sigint(int sig)
{
    signal(SIGINT, sigint);
    signal(SIGQUIT, sigint);

    fprintf(stderr, "Absolute interval: ");
    showsecs(achrono.millis());
    fprintf(stderr, ". Relative interval: ");
    showsecs(rchrono.restart());
    fprintf(stderr, ".\n");
    if (sig == SIGQUIT) {
        exit(0);
    }
}

int main(int argc, char **argv)
{
    thisprog = argv[0];
    argc--; argv++;
    if (argc != 0) {
        Usage();
    }
    sleep(1);
    fprintf(stderr, "Initial micros: %ld\n", achrono.micros());;
    fprintf(stderr, "Type ^C for intermediate result, ^\\ to stop\n");
    signal(SIGINT, sigint);
    signal(SIGQUIT, sigint);
    achrono.restart();
    rchrono.restart();
    while (1) {
        pause();
    }
}

#endif /*TEST_CHRONO*/
