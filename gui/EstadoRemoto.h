// Lo que contesta el servidor sobre una descarga («Bajar con el servidor», GET api/trabajos/ID de
// descargas_web.py), leído sin nada de Android: así se prueba en Linux (tests/tst_analisis.cpp).
//   {"estado": "descargando", "actual": "Video.mkv", "item": "2 de 5", "porcentaje": 40,
//    "urls": [{"url": "...", "estado": "listo|parcial|error|…", "error": "..."}],
//    "carpetas": [{"nombre": "Canal", "archivos": [{"nombre": "Video.mkv", "tamano": 123, "miniatura": true}]}]}

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace trabajoServidor {

// listo, error, cancelado o perdido: el servidor ya no va a hacer nada más con ese trabajo
bool terminado(const QString &estado);

// En /sdcard Android no deja usar algunos caracteres que en Linux sí valen: se cambian por «_»
QString nombreSeguro(QString nombre);

struct Archivo
{
    QString carpeta, nombre;
    qint64 tamano = 0;
    bool miniatura = false;
};

struct UrlFallida
{
    QString url, error;
};

struct Estado
{
    QString estado;             // en cola, descargando, listo, error, cancelado, perdido
    QString actual, item;       // el archivo que está bajando el servidor y «2 de 5»
    int porcentaje = 0;
    int hechasUrls = 0;         // las que ya terminó (bien, a medias o mal)
    int sinTerminar = 0;        // las que todavía no terminaron
    QList<Archivo> archivos;    // los terminados, en el orden en que vienen
    QList<UrlFallida> fallidas; // las que terminaron con error o a medias
};

Estado leer(const QJsonObject &trabajo);

} // namespace trabajoServidor
