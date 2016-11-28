#!/bin/bash
#
# Test Build
# This tests several parts of the c-skeleton build-process. This includes a run
# of `make distcheck` among others.
#

set -e

# recursion protection
if [[ -n $SKELETON_RECURSIVE ]] ; then
        exit 0
else
        export SKELETON_RECURSIVE=1
fi

# test `make distcheck`
make distcheck
