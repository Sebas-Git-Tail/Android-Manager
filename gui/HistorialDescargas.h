// Lo que se bajó (Videos o Música), guardado entre una vez y otra: la lista de descargados, las URL que
// fallaron la última vez («Reintentar») y los archivos borrados que todavía se podían deshacer cuando la app
// se cerró (se terminan de borrar al abrirla). Un JSON en la carpeta de datos de la app. Sin nada de
// Android: se prueba en Linux (tests/tst_analisis.cpp).

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace historialDescargas {

constexpr int MaxEntradas = 300;

struct Entrada
{
    QString nombre, ruta, miniatura;
    bool operator==(const Entrada &otra) const
    {
        return nombre == otra.nombre && ruta == otra.ruta && miniatura == otra.miniatura;
    }
};

struct Historial
{
    QList<Entrada> entradas;      // la más nueva primero
    QStringList fallidas;         // URL
    QStringList papelera;         // archivos escondidos esperando el «Deshacer»
};

QByteArray guardar(const Historial &historial);
Historial cargar(const QByteArray &json);     // lo que no se entienda se ignora

// El nombre escondido que lleva un archivo mientras se puede deshacer su borrado (misma carpeta: es un
// rename, no una copia): «/sdcard/Movies/Canal/Video.mkv» → «/sdcard/Movies/Canal/.Video.mkv.borrando»
QString rutaEscondida(const QString &ruta);

} // namespace historialDescargas
