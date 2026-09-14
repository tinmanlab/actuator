#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=${1:-_site}
mkdir -p "$OUT/wasm"
em++ -O3 -std=c++17 -fexceptions -Iinclude web/live.cpp src/control.cpp src/plant.cpp src/protection.cpp \
 --no-entry -sMODULARIZE=1 -sEXPORT_NAME=createQdd -sENVIRONMENT=worker \
 -sSINGLE_FILE=1 -sALLOW_MEMORY_GROWTH=1 -sDISABLE_EXCEPTION_CATCHING=0 \
 -sEXPORTED_FUNCTIONS='["_lab_reset","_lab_set","_lab_step","_lab_get"]' \
 -o "$OUT/wasm/qdd.js"
