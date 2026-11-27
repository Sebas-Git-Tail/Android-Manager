#include "AnalisisMinecraft.h"

#include <QCoreApplication>

namespace analisisMinecraft {

namespace {

QString tr(const char *texto)
{
    return QCoreApplication::translate("Minecraft", texto);
}

} // namespace

std::pair<QString, QString> partir(const QString &linea)
{
    const qsizetype espacio = linea.indexOf(u' ');
    return {linea.left(espacio), espacio < 0 ? QString() : linea.mid(espacio + 1)};
}

QString pedido(const QStringList &argumentos)
{
    return QStringLiteral("minecraft ") + QString::fromLatin1(argumentos.join(u'\n').toUtf8().toBase64());
}

QString accionDe(const QString &pedido)
{
    return pedido.section(u' ', 0, 0) == u"minecraft"
        ? QString::fromUtf8(QByteArray::fromBase64(pedido.section(u' ', 1).toLatin1())).section(u'\n', 0, 0)
        : pedido;
}

QString etiqueta(const QString &servidor)
{
    const qsizetype barra = servidor.indexOf(u'/');
    if (barra < 0)
        return servidor;
    return QStringLiteral("%1 (%2)").arg(servidor.section(u'/', -1), servidor.left(barra));
}

std::optional<QVariantMap> servidor(const QString &resto, const QDate &hoy)
{
    const QStringList c = resto.split(u'\t');
    if (c.size() < 8)
        return std::nullopt;
    QString ultimo = c.at(5);
    const QDate fecha = QDate::fromString(ultimo, Qt::ISODate);
    if (fecha.isValid())
        ultimo = fecha == hoy ? tr("hoy") : fecha.toString(QStringLiteral("dd/MM/yyyy"));
    const QString &nombre = c.at(1);
    return QVariantMap{{QStringLiteral("nombre"), nombre},
                       {QStringLiteral("titulo"), nombre.section(u'/', -1)},
                       {QStringLiteral("grupo"), nombre.contains(u'/') ? nombre.section(u'/', 0, 0) : QString()},
                       {QStringLiteral("encendido"), c.at(0) == u"ENCENDIDO"},
                       // paper|forge|fabric|vanilla (Fabric y Vanilla desde Arch-Linux-Manager 4.14.0)
                       {QStringLiteral("tipo"), c.at(2).isEmpty() ? QStringLiteral("Paper") : c.at(2).left(1).toUpper() + c.at(2).mid(1)},
                       {QStringLiteral("version"), c.at(3)}, {QStringLiteral("mundo"), c.at(4)},
                       {QStringLiteral("ultimo"), ultimo}, {QStringLiteral("ram"), c.at(6)},
                       {QStringLiteral("jar"), c.at(7)}};
}

std::optional<std::pair<int, QString>> jugadores(const QString &resto)
{
    const QStringList c = resto.split(u'\t');
    if (c.size() < 3)
        return std::nullopt;
    const int conectados = c.at(0).toInt();
    const QStringList nombres = c.at(2).split(u',', Qt::SkipEmptyParts);
    if (conectados == 0)
        return std::pair{0, tr("Nadie conectado (entran hasta %1).").arg(c.at(1))};
    QString lista = nombres.join(QStringLiteral(", "));
    if (nombres.size() < conectados)
        lista += (lista.isEmpty() ? QString() : QStringLiteral(" ")) + tr("y %1 más").arg(conectados - nombres.size());
    return std::pair{conectados, tr("Conectados (%1 de %2): %3").arg(conectados).arg(c.at(1), lista)};
}

std::optional<QVariantMap> plugin(const QString &resto)
{
    const QStringList c = resto.split(u'\t');
    if (c.size() < 5)
        return std::nullopt;
    return QVariantMap{{QStringLiteral("archivo"), c.at(0)}, {QStringLiteral("nombre"), c.at(1)},
                       {QStringLiteral("version"), c.at(2)}, {QStringLiteral("nueva"), c.at(3)},
                       {QStringLiteral("estado"), c.at(4)}};
}

} // namespace analisisMinecraft
