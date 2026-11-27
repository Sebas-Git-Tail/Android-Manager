#!/bin/bash
# Compila la versión final de la app y deja el APK firmado en esta carpeta. Antes hay que
# correr Preparar-binarios.sh. Rutas por defecto: Qt 6.10.0 en ~/Qt y el SDK en ~/Android/Sdk.
#
# La clave de firma se crea la primera vez en ~/.android, fuera del proyecto. Hay que guardarle
# una copia: Android solo acepta una actualización firmada con la misma clave; sin ella habría
# que desinstalar la app en cada celular.
set -euo pipefail
cd "$(dirname "$(realpath "$0")")"
QT="${QT:-$HOME/Qt/6.10.0}"
export ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk}"
NDK=$(ls -d "$ANDROID_HOME"/ndk/* | sort -V | tail -1)
HERRAMIENTAS=$(ls -d "$ANDROID_HOME"/build-tools/* | sort -V | tail -1)
BUILD="${BUILD:-build}"
CLAVE="$HOME/.android/android-manager.keystore"
CONTRASENA="$CLAVE.pass"

if [[ ! -f "$CLAVE" ]]; then
    ( umask 077; head -c 24 /dev/urandom | base64 > "$CONTRASENA" )
    "$JAVA_HOME/bin/keytool" -genkeypair -keystore "$CLAVE" -storepass:file "$CONTRASENA" -alias android-manager \
        -keyalg RSA -keysize 4096 -validity 36500 -dname "CN=Android-Manager"
    chmod 600 "$CLAVE"
fi

# Primero las pruebas de la lógica que no depende de Android (tests/, con el Qt de escritorio)
QT="$QT" tests/correr.sh > /dev/null || { echo "Fallan las pruebas (tests/correr.sh para ver cuáles)" >&2; exit 1; }
"$QT/android_arm64_v8a/bin/qt-cmake" -B "$BUILD" -S gui -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_HOST_PATH="$QT/gcc_64" \
    -DANDROID_SDK_ROOT="$ANDROID_HOME" -DANDROID_NDK_ROOT="$NDK" \
    -DQT_ANDROID_ABIS=arm64-v8a
# Hasta la 1.11.2 el entorno iba con las bibliotecas (ahora va en android/assets). Puede haber quedado
# una copia vieja (Qt no borra lo que ya copió a la compilación, y las copias con rsync tampoco)
rm -f android/libs/arm64-v8a/libentorno.tar.so "$BUILD/android-build/libs/arm64-v8a/libentorno.tar.so"
[[ -f android/assets/entorno.tgz && -f android/assets/entorno.firma ]] || { echo "Falta android/assets/entorno.tgz: corré Preparar-binarios.sh" >&2; exit 1; }
cmake --build "$BUILD" --target apk

SIN_FIRMAR="$BUILD/android-build/build/outputs/apk/release/android-build-release-unsigned.apk"
VERSION=$(grep -oP "project\(\w+ VERSION \K[0-9.]+" gui/CMakeLists.txt)   # el nombre del APK lleva la versión
APK="Salida/Android-Manager-$VERSION.apk"
mkdir -p Salida   # lo compilado queda en Salida/ (la versión anterior pasa a ~/Repo/Legacy)
for viejo in Salida/Android-Manager*.apk; do
    if [ -e "$viejo" ] && [ "$viejo" != "$APK" ]; then mkdir -p ~/Repo/Legacy/Android-Manager && mv "$viejo" ~/Repo/Legacy/Android-Manager/; fi
done
"$HERRAMIENTAS/zipalign" -f -p 4 "$SIN_FIRMAR" "$APK"
"$HERRAMIENTAS/apksigner" sign --ks "$CLAVE" --ks-pass "file:$CONTRASENA" "$APK"
"$HERRAMIENTAS/apksigner" verify "$APK"
rm -f "$APK".idsig
ls -la "$APK"
# Errores de QML que si no recién se verían en el celular (no frena: el APK ya quedó)
./Revisar-qml.sh "$BUILD" || echo "OJO: revisá los avisos de qmllint de arriba"
