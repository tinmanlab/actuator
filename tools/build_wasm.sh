#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/build_bench_model.py
OUT=${1:-_site}
mkdir -p "$OUT/wasm"
em++ -O3 -std=c++17 -fexceptions -Iinclude web/live.cpp src/control.cpp src/plant.cpp src/protection.cpp \
 --no-entry -sMODULARIZE=1 -sEXPORT_NAME=createQdd -sENVIRONMENT=worker \
 -sSINGLE_FILE=1 -sALLOW_MEMORY_GROWTH=1 -sDISABLE_EXCEPTION_CATCHING=0 \
 -sEXPORTED_FUNCTIONS='["_lab_reset","_lab_set","_lab_step","_lab_get"]' \
 -o "$OUT/wasm/qdd.js"

em++ -O3 -std=c++17 -fexceptions -Iinclude web/experiments.cpp src/control.cpp src/plant.cpp \
 --no-entry -sMODULARIZE=1 -sEXPORT_NAME=createExperiments -sENVIRONMENT=worker \
 -sSINGLE_FILE=1 -sALLOW_MEMORY_GROWTH=1 -sDISABLE_EXCEPTION_CATCHING=0 \
 -sEXPORTED_FUNCTIONS='["_exp_run","_exp_rows","_exp_get"]' \
 -o "$OUT/wasm/experiments.js"
