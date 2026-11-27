# Pruebas de la lógica que no depende de Android (análisis de la salida de rsync, respuestas del servidor
# de descargas, Minecraft, historial de descargas). Corren en Linux con el Qt de escritorio:
#   tests/correr.sh        (o: cmake -S tests -B build-pruebas -G Ninja && cmake --build build-pruebas
#                              && QT_QPA_PLATFORM=offscreen ctest --test-dir build-pruebas)
cmake_minimum_required(VERSION 3.21)
project(PruebasAndroidManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)   # como la app
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Test)
enable_testing()

set(GUI ${CMAKE_CURRENT_SOURCE_DIR}/../gui)
add_executable(tst_analisis tst_analisis.cpp
    ${GUI}/AnalisisRsync.cpp ${GUI}/EstadoRemoto.cpp ${GUI}/AnalisisMinecraft.cpp ${GUI}/HistorialDescargas.cpp)
target_include_directories(tst_analisis PRIVATE ${GUI})
target_link_libraries(tst_analisis PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tst_analisis COMMAND tst_analisis)
set_tests_properties(tst_analisis PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
