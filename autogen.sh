#!/bin/sh

aclocal
automake --force-missing --add-missing --copy
autoconf
