```
exec gdb \
    -ex "set print thread-events off" \
    -ex "set debuginfod enabled on" \
    -ex "run" \
    --args ./linscp
```

set logging on

set logging off

# Пересоздать тег

git tag -d v0.1.0a1 && git push origin :v0.1.0a1

git tag v0.1.0a1 && git push origin v0.1.0a1

# Создать тег с аннотацией

git tag -a v0.1.0a1 -m "Alpha 1"

git push origin v0.1.0a1
