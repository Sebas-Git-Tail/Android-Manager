#!/bin/bash
# Revisa los QML con qmllint después de compilar: propiedades o señales que no existen, imports rotos,
# tipos mal escritos, propiedades «required» sin dar… Así esos errores aparecen acá y no recién en el celular.
#
# No revisa los accesos a los objetos de C++ (Minecraft, Descargas, Servidor…): se registran con
# setContextProperty y qmllint no los conoce («unqualified»; serían cientos de avisos falsos).
# Los avisos falsos que quedan están en gui/qmllint-conocidos.txt (archivo + mensaje, sin línea).
#
# Uso: ./Revisar-qml.sh [carpeta-de-compilación]   (por defecto build; tiene que estar configurada)
#      QMLLINT=/ruta/a/qmllint si no está en ~/Qt/6.10.0/gcc_64/bin
# Sale con 1 si hay avisos nuevos.
set -uo pipefail
cd "$(dirname "$(realpath "$0")")"
B=$(realpath "${1:-build}")
RSP="$B/.rcc/qmllint/Android-Manager.rsp"
[[ -f "$RSP" ]] || { echo "Falta $RSP: configurá/compilá primero (./Compilar.sh)"; exit 2; }
QMLLINT="${QMLLINT:-${QT:-$HOME/Qt/6.10.0}/gcc_64/bin/qmllint}"
[[ -x "$QMLLINT" ]] || { echo "No encuentro qmllint en $QMLLINT (usá QMLLINT=…)"; exit 2; }

# El .rsp que arma Qt no incluye la carpeta del módulo Comun.Ui (comun/ui/qml): sin ella, todo lo de
# Comun.Ui sale como «no encontrado»
salida=$("$QMLLINT" "@$RSP" -I "$B/comun/ui/qml" --unqualified disable 2>&1)
raiz="$PWD/"
avisos=$(grep -E '^Warning: ' <<< "$salida" | sed "s|$raiz||")
# «archivo: mensaje» (sin línea ni columna) para comparar con los conocidos
nuevos=$(while IFS= read -r a; do
    [[ -n "$a" ]] || continue
    clave=$(sed -E 's/^Warning: ([^:]+):[0-9]+:[0-9]+: /\1: /' <<< "$a")
    grep -qxF -- "$clave" <(grep -v '^#' gui/qmllint-conocidos.txt) || echo "$a"
done <<< "$avisos")
total=$(grep -c . <<< "$avisos")
if [[ -n "$nuevos" ]]; then
    echo "qmllint: avisos nuevos en los QML:"
    sed 's/^Warning: /  /' <<< "$nuevos"
    echo "(si alguno es falso, agregarlo sin línea ni columna a gui/qmllint-conocidos.txt)"
    exit 1
fi
echo "qmllint: sin avisos nuevos ($total conocidos)"
