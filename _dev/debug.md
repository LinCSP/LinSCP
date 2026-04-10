exec gdb \
    -ex "set print thread-events off" \
    -ex "set debuginfod enabled on" \
    -ex "run" \
    --args ./linscp

set logging on

set logging off