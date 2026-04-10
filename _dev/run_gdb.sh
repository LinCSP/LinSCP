#!/bin/bash
cd /home/alex/devel/build_lincsp-Debug/bin/
exec gdb \
    -ex "set print thread-events off" \
    -ex "set debuginfod enabled on" \
    -ex "run" \
    --args ./linscp
