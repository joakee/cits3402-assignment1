#!/usr/bin/env bash

# compiles a lil helper against `toy_hash.c` (so the hash is, hashes both
# files as they sit on disk, and compares. Meant for the files  serial_implementation 
# writes to solved/.
#
# usage: ./verify_collision.sh <solved_pdf_1> <solved_pdf_2>
# exits 0 on match, 1 on no match, 2 on error
#------------------------------
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <solved_pdf_1> <solved_pdf_2>" >&2
    exit 2
fi

for f in "$1" "$2"; do
    if [ ! -f "$f" ]; then
        echo "error: no such file: $f" >&2
        exit 2
    fi
done

# the repo root has gotta be wherever this script lives
# (args stay relative to $PWD)
repo="$(cd "$(dirname "$0")" && pwd)"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cat > "$work/hash_one.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "toy_hash.h"

// hash one file and print the 48-bit toy_hash as 12 hex digits
int main(int argc, char *argv[])
{
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    if (fseek(f, 0, SEEK_END) != 0) { perror(argv[1]); return 2; }
    long len = ftell(f);
    if (len < 0) { perror(argv[1]); return 2; }
    rewind(f);
    unsigned char *buf = malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "%s: read failed\n", argv[1]);
        return 2;
    }
    fclose(f);
    printf("%012llx\n", (unsigned long long)toy_hash(buf, (size_t)len));
    return 0;
}
EOF

gcc -O2 -std=c11 -o "$work/hash_one" "$work/hash_one.c" "$repo/toy_hash.c" -I"$repo"

h1="$("$work/hash_one" "$1")"
h2="$("$work/hash_one" "$2")"

echo "$1: $h1"
echo "$2: $h2"

if [ "$h1" = "$h2" ]; then
    echo "MATCH"
    exit 0
else
    echo "NO MATCH"
    exit 1
fi
