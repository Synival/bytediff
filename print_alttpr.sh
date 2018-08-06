#!/bin/sh
if [ $# -eq 0 ]; then
    echo "Usage: $0 ROM-FILE"
    exit 1
fi
./bytediff $1 -M alttpr_map.txt -o 0xe0000 -O 0xe51dd -p
