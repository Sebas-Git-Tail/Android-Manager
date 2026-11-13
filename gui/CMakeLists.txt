# Android-Manager: versión para Android del Manager (descargas de videos y música, y respaldo).
# Se compila con ../Compilar.sh. Los .ui se editan con Qt Designer.

cmake_minimum_required(VERSION 3.21)
project(AndroidManager VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

qt_add_executable(Android-Manager
    main.cpp
    Plataforma.cpp Plataforma.h
    Herramientas.cpp Herramientas.h
    VentanaPrincipal.cpp VentanaPrincipal.h VentanaPrincipal.ui
    PaginaDescargas.cpp PaginaDescargas.h PaginaDescargas.ui
    DialogoPistas.cpp DialogoPistas.h DialogoPistas.ui
    ConsultaVideo.cpp ConsultaVideo.h
    PaginaRespaldo.cpp PaginaRespaldo.h PaginaRespaldo.ui
    ResumenSimulacion.cpp ResumenSimulacion.h
)
qt_add_resources(Android-Manager "recursos" PREFIX "/" FILES Logo.png)
target_link_libraries(Android-Manager PRIVATE Qt6::Widgets Qt6::Network)

set_target_properties(Android-Manager PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../android"
    QT_ANDROID_PACKAGE_NAME "com.sebastian.androidmanager"
    QT_ANDROID_MIN_SDK_VERSION 30
    QT_ANDROID_TARGET_SDK_VERSION 35
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE 2
)
