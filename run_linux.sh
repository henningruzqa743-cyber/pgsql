#!/bin/sh
set -eu

log="$(date +%m%d)_1.log"

{
    printf 'Date: %s\nDirectory: %s\n' "$(date '+%Y-%m-%d %H:%M:%S %Z')" "$PWD"
    printf '\nCPU\n'
    lscpu
    printf '\nCompiler\n'
    gcc --version | head -1
    printf '\nCompile and test\n'
    gcc -std=c11 -O2 -Wall -Wextra -Werror -o test_link test_link.c
    ./test_link
    gcc -std=c11 -O2 -Wall -Wextra -Werror -o link_search link_search.c
    printf '\nSearch benchmark\n'
    ./link_search 10000 64 1000
} > "$log" 2>&1 || {
    cat "$log"
    exit 1
}

cat "$log"
