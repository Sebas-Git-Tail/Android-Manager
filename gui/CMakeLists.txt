# Android-Manager: versión para Android del Manager (descargas de videos y música, y respaldo).
# Se compila con ../Compilar.sh. Los .ui se editan con Qt Designer.

cmake_minimum_required(VERSION 3.21)
project(AndroidManager VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

# Código compartido (repo comun-qt, traído con git subtree a ../comun): diálogo de pistas, consulta a
# yt-dlp, resumen de rsync, datos del servidor (servidor.json) y aviso de versiones nuevas.
# Su parte en Java (ClienteHttp) está enlazada en android/src/com/sebastian/comun.
set(COMUN_DESCARGAS ON)
set(COMUN_RED ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../comun ${CMAKE_CURRENT_BINARY_DIR}/comun)

qt_add_executable(Android-Manager
    main.cpp
    Plataforma.cpp Plataforma.h
    Herramientas.cpp Herramientas.h
    VentanaPrincipal.cpp VentanaPrincipal.h VentanaPrincipal.ui
    PaginaDescargas.cpp PaginaDescargas.h PaginaDescargas.ui
    PaginaRespaldo.cpp PaginaRespaldo.h PaginaRespaldo.ui
)
qt_add_resources(Android-Manager "recursos" PREFIX "/" FILES Logo.png)
target_link_libraries(Android-Manager PRIVATE Qt6::Widgets Qt6::Network comun_descargas comun_red)
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
