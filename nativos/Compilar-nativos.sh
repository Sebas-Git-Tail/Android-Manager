#!/bin/bash
# Compila con el NDK los dos programas del respaldo y los deja en esta carpeta. Después hay
# que correr ../Preparar-binarios.sh y ../Compilar.sh para que entren en el APK.
#   rsync     sin dependencias externas
#   dbclient  el cliente SSH de Dropbear: rsync lo usa en lugar de ssh, y toma la contraseña
#             de la variable DROPBEAR_PASSWORD
set -euo pipefail
cd "$(dirname "$(realpath "$0")")"
RSYNC=3.5.2
DROPBEAR=DROPBEAR_2026.94
SALIDA=$PWD
TEMPORAL=$(mktemp -d)
trap 'rm -rf "$TEMPORAL"' EXIT

NDK=$(ls -d "${ANDROID_HOME:-$HOME/Android/Sdk}"/ndk/* | sort -V | tail -1)
T=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin
export CC=$T/aarch64-linux-android30-clang AR=$T/llvm-ar RANLIB=$T/llvm-ranlib STRIP=$T/llvm-strip

cd "$TEMPORAL"
# El paquete de la release (no el del tag) ya trae el configure generado
curl -fsSL "https://github.com/RsyncProject/rsync/releases/download/v$RSYNC/rsync-$RSYNC.tar.gz" | tar xz
curl -fsSL "https://github.com/mkj/dropbear/archive/refs/tags/$DROPBEAR.tar.gz" | tar xz

cd "$TEMPORAL/rsync-$RSYNC"
./configure --host=aarch64-linux-android --disable-md2man --disable-xxhash --disable-zstd \
    --disable-lz4 --disable-openssl --disable-iconv --disable-acl-support --disable-xattr-support \
    --disable-locale --disable-idn --with-included-popt --with-included-zlib
make -j"$(nproc)" rsync
"$STRIP" -o "$SALIDA/rsync" rsync

cd "$TEMPORAL/dropbear-$DROPBEAR"
# Solo se usa el cliente, y la autenticación del servidor pide crypt(), que Android no tiene
echo '#define DROPBEAR_SVR_PASSWORD_AUTH 0' > localoptions.h
# Android tampoco tiene getpass(); no se llega a usar, la contraseña viene de DROPBEAR_PASSWORD
echo '#define getpass(mensaje) ((char *)0)' >> localoptions.h
./configure --host=aarch64-linux-android --disable-zlib --disable-utmp --disable-utmpx --disable-wtmp \
    --disable-wtmpx --disable-lastlog --disable-syslog
make -j"$(nproc)" PROGRAMS=dbclient
"$STRIP" -o "$SALIDA/dbclient" dbclient

ls -la "$SALIDA/rsync" "$SALIDA/dbclient"
