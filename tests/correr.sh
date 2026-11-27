#!/bin/bash
# Compila y corre las pruebas de tests/ con el Qt de escritorio (por defecto ~/Qt/6.10.0/gcc_64; QT_ESCRITORIO=…
# para otro). Deja todo en build-pruebas/. Sale con error si alguna falla.
set -euo pipefail
cd "$(dirname "$(realpath "$0")")/.."
QT_ESCRITORIO="${QT_ESCRITORIO:-${QT:-$HOME/Qt/6.10.0}/gcc_64}"
cmake -S tests -B build-pruebas -G Ninja -DCMAKE_PREFIX_PATH="$QT_ESCRITORIO" -DCMAKE_BUILD_TYPE=Debug > /dev/null
cmake --build build-pruebas
QT_QPA_PLATFORM=offscreen ctest --test-dir build-pruebas --output-on-failure
