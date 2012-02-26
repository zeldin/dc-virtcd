#!/bin/sh
set -e
autoconf
( cd target && autoconf )
( cd host && autoheader && autoconf )

