#include "HistorialDescargas.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace historialDescargas {

QByteArray guardar(const Historial &historial)
{
    QJsonArray entradas;
    for (const Entrada &e : historial.entradas.mid(0, MaxEntradas))
        entradas << QJsonObject{{QStringLiteral("nombre"), e.nombre}, {QStringLiteral("ruta"), e.ruta},
                                {QStringLiteral("miniatura"), e.miniatura}};
    return QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                     {QStringLiteral("descargados"), entradas},
                                     {QStringLiteral("fallidas"), QJsonArray::fromStringList(historial.fallidas)},
                                     {QStringLiteral("papelera"), QJsonArray::fromStringList(historial.papelera)}})
        .toJson(QJsonDocument::Indented);
}

Historial cargar(const QByteArray &json)
{
    Historial h;
    const QJsonObject raiz = QJsonDocument::fromJson(json).object();
    const QJsonArray entradas = raiz.value(QStringLiteral("descargados")).toArray();
    for (const QJsonValue &v : entradas) {
        const QJsonObject o = v.toObject();
        Entrada e{o.value(QStringLiteral("nombre")).toString(), o.value(QStringLiteral("ruta")).toString(),
                  o.value(QStringLiteral("miniatura")).toString()};
        if (!e.ruta.isEmpty() && h.entradas.size() < MaxEntradas)
            h.entradas << e;
    }
    for (const QJsonValue &v : raiz.value(QStringLiteral("fallidas")).toArray())
        if (!v.toString().trimmed().isEmpty())
            h.fallidas << v.toString().trimmed();
    for (const QJsonValue &v : raiz.value(QStringLiteral("papelera")).toArray())
        if (QFileInfo(v.toString()).fileName().endsWith(QStringLiteral(".borrando")))   // solo lo escondido por la app
            h.papelera << v.toString();
    return h;
}

QString rutaEscondida(const QString &ruta)
{
    const QFileInfo info(ruta);
    return info.dir().filePath(u'.' + info.fileName() + QStringLiteral(".borrando"));
}

} // namespace historialDescargas
