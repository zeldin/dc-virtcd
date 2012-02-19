#!/bin/sh
set -e
autoconf
( cd target && autoconf )
( cd host && autoconf )

