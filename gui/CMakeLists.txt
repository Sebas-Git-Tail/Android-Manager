# Android-Manager: versión para Android del Manager (descargas de videos y música, y respaldo).
# Se compila con ../Compilar.sh. La interfaz es QML (qml/) con el estilo Material, como Ingresos y
# Gastos y el Gestor de Stock; la lógica está en C++ (Descargas, Respaldo, Herramientas, Sistema).

cmake_minimum_required(VERSION 3.21)
project(AndroidManager VERSION 1.7.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Network Svg)   # Svg: los íconos
qt_standard_project_setup(REQUIRES 6.8)

# Código compartido (repo comun-qt, traído con git subtree a ../comun): consulta a yt-dlp y opciones
# de pistas (sin Widgets), resumen de rsync, datos del servidor (servidor.json) y «Apps y
# actualizaciones». Su parte en Java (ClienteHttp, Instalador…) está enlazada en android/src/com/sebastian/comun.
set(COMUN_DESCARGAS_NUCLEO ON)
set(COMUN_RED ON)
set(COMUN_APPS ON)
set(COMUN_UI_QML ON)        # tema y piezas comunes en QML (módulo Comun.Ui: tarjetas, chips, avisos, «Apps»)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../comun ${CMAKE_CURRENT_BINARY_DIR}/comun)

qt_add_executable(Android-Manager
    main.cpp
    Rutas.h
    Plataforma.cpp Plataforma.h
    Herramientas.cpp Herramientas.h
    Descargas.cpp Descargas.h
    ModeloPistas.cpp ModeloPistas.h
    Respaldo.cpp Respaldo.h
    Sistema.cpp Sistema.h
)

qt_add_qml_module(Android-Manager
    URI AndroidManager
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Portada.qml
        qml/PaginaDescargas.qml
        qml/PaginaPistas.qml
        qml/PaginaRespaldo.qml
    RESOURCES
        Logo.png
        iconos/abrir.svg iconos/apps.svg iconos/aviso.svg iconos/borrar.svg iconos/carpeta.svg
        iconos/cerrar.svg iconos/descargar.svg iconos/desplegar.svg iconos/error.svg iconos/escudo.svg
        iconos/listo.svg iconos/musica.svg iconos/pegar.svg iconos/reloj.svg iconos/reproducir.svg
        iconos/respaldo.svg iconos/siguiente.svg iconos/video.svg iconos/volver.svg
)
target_link_libraries(Android-Manager PRIVATE Qt6::Quick comun_ui_qmlplugin Qt6::QuickControls2 Qt6::Network Qt6::Svg
                      comun_descargas_nucleo comun_red comun_apps)
target_compile_definitions(Android-Manager PRIVATE VERSION_PROGRAMA="${PROJECT_VERSION}")

# versionCode sale solo de la versión (1.2.0 → 10200): Android no instala una actualización con un
# versionCode igual o menor, y escribirlo a mano era fácil de olvidar
math(EXPR CODIGO_VERSION "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")

set_target_properties(Android-Manager PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../android"
    QT_ANDROID_PACKAGE_NAME "com.sebastian.androidmanager"
    QT_ANDROID_MIN_SDK_VERSION 30
    QT_ANDROID_TARGET_SDK_VERSION 35
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE ${CODIGO_VERSION}
)
