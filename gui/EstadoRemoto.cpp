#include "EstadoRemoto.h"

#include <QJsonArray>
#include <QRegularExpression>

namespace trabajoServidor {

bool terminado(const QString &estado)
{
    return estado == u"listo" || estado == u"error" || estado == u"cancelado" || estado == u"perdido";
}

QString nombreSeguro(QString nombre)
{
    static const QRegularExpression prohibidos(QStringLiteral("[\\x00-\\x1f\"*:<>?\\\\|/]"));
    nombre.replace(prohibidos, QStringLiteral("_"));
    nombre = nombre.trimmed();
    if (nombre.isEmpty() || nombre == u"." || nombre == u"..")
        nombre = QStringLiteral("_");
    return nombre;
}

Estado leer(const QJsonObject &trabajo)
{
    Estado e;
    e.estado = trabajo.value(QStringLiteral("estado")).toString();
    e.actual = trabajo.value(QStringLiteral("actual")).toString();
    e.item = trabajo.value(QStringLiteral("item")).toString();
    e.porcentaje = trabajo.value(QStringLiteral("porcentaje")).toInt();

    const QJsonArray carpetas = trabajo.value(QStringLiteral("carpetas")).toArray();
    for (const QJsonValue &valorCarpeta : carpetas) {
        const QJsonObject carpeta = valorCarpeta.toObject();
        const QString nombreCarpeta = carpeta.value(QStringLiteral("nombre")).toString();
        const QJsonArray archivos = carpeta.value(QStringLiteral("archivos")).toArray();
        for (const QJsonValue &valorArchivo : archivos) {
            const QJsonObject archivo = valorArchivo.toObject();
            Archivo a{nombreCarpeta, archivo.value(QStringLiteral("nombre")).toString(),
                      qint64(archivo.value(QStringLiteral("tamano")).toDouble()),
                      archivo.value(QStringLiteral("miniatura")).toBool()};
            if (!a.nombre.isEmpty())
                e.archivos << a;
        }
    }

    const QJsonArray urls = trabajo.value(QStringLiteral("urls")).toArray();
    for (const QJsonValue &valor : urls) {
        const QJsonObject url = valor.toObject();
        const QString estado = url.value(QStringLiteral("estado")).toString();
        if (estado == u"listo" || estado == u"parcial" || estado == u"error")
            ++e.hechasUrls;
        if (estado == u"error" || estado == u"parcial")
            e.fallidas << UrlFallida{url.value(QStringLiteral("url")).toString(), url.value(QStringLiteral("error")).toString()};
        else if (estado != u"listo")
            ++e.sinTerminar;
    }
    return e;
}

} // namespace trabajoServidor
