#!/bin/bash
# Deja en android/libs/arm64-v8a los programas que usa la app. Android solo deja ejecutar
# lo que viene instalado como biblioteca nativa, por eso todos se llaman lib*.so.
#   libpython.so, libqjs.so, libffmpeg.so, libffprobe.so   de youtubedl-android (Maven Central)
#   librsync.so, libdbclient.so  para el respaldo, se copian de nativos/ (rsync y dbclient)
#   libdropbearkey.so  la clave del celular para la sección Servidor (nativos/dropbearkey)
# Y en android/assets el resto, que no se ejecuta (así Android no guarda una copia más al instalar):
#   entorno.tgz        bibliotecas de Python y ffmpeg + yt-dlp; la app lo descomprime al abrirse
#                      (no .tar.gz: el empaquetador de Android descomprime los assets .gz)
#   entorno.firma      su sha256: si cambia, la app lo vuelve a descomprimir
#
# Uso: Preparar-binarios.sh [CARPETA_CON_LOS_AAR]  (sin carpeta, los descarga)
set -euo pipefail
cd "$(dirname "$(realpath "$0")")"
VERSION=0.18.1
MAVEN=https://repo1.maven.org/maven2/io/github/junkfood02/youtubedl-android
SALIDA=android/libs/arm64-v8a
ASSETS=android/assets
TEMPORAL=$(mktemp -d)
trap 'rm -rf "$TEMPORAL"' EXIT
mkdir -p "$SALIDA" "$ASSETS"
rm -f "$SALIDA/libentorno.tar.so"    # hasta la 1.11.2 el entorno iba con las bibliotecas

for modulo in library ffmpeg; do
    aar="${1:-}/$modulo.aar"
    if [[ -z "${1:-}" ]]; then
        aar="$TEMPORAL/$modulo.aar"
        curl -fsSL -o "$aar" "$MAVEN/$modulo/$VERSION/$modulo-$VERSION.aar"
    fi
    mkdir -p "$TEMPORAL/$modulo"
    bsdtar -xf "$aar" -C "$TEMPORAL/$modulo" 'jni/arm64-v8a/*'
done
PY=$TEMPORAL/library/jni/arm64-v8a
FF=$TEMPORAL/ffmpeg/jni/arm64-v8a

curl -fsSL -o "$TEMPORAL/yt-dlp" https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp
curl -fsSL https://github.com/yt-dlp/yt-dlp/releases/latest/download/SHA2-256SUMS \
    | grep ' yt-dlp$' | sed "s|yt-dlp\$|$TEMPORAL/yt-dlp|" | sha256sum -c --quiet

cp "$PY/libpython.so" "$PY/libqjs.so" "$FF/libffmpeg.so" "$FF/libffprobe.so" "$SALIDA/"
python3 -I "nativos/armar-entorno.py" "$PY/libpython.zip.so" "$FF/libffmpeg.zip.so" "$TEMPORAL/yt-dlp" "$ASSETS/entorno.tgz" \
    "$PY/libpython.so" "$FF/libffmpeg.so" "$FF/libffprobe.so"
rm -f "$ASSETS/entorno.tar.gz"
sha256sum "$ASSETS/entorno.tgz" | cut -d' ' -f1 > "$ASSETS/entorno.firma"

for programa in rsync dbclient dropbearkey; do
    if [[ -f "nativos/$programa" ]]; then
        cp "nativos/$programa" "$SALIDA/lib$programa.so"
    else
        echo "AVISO: falta nativos/$programa; el respaldo no va a estar disponible." >&2
    fi
done
ls -la "$SALIDA" "$ASSETS"
