#include "AnalisisRsync.h"

#include <QRegularExpression>

namespace analisisRsync {

int porcentaje(const QString &linea)
{
    // Anclada y con el tamaño adelante («13B», «2.00MB», «1,5G»), porque un archivo puede llamarse
    // «99% seguro.txt» o «DCIM/foto 50% .jpg»
    static const QRegularExpression progreso(QStringLiteral("^[\\d.,]+[kKMGTPE]?B?\\s+(\\d+)%\\s"));
    const QRegularExpressionMatch m = progreso.match(linea);
    return m.hasMatch() ? m.captured(1).toInt() : -1;
}

bool esPasoAAnteriores(const QString &linea)
{
    return linea.startsWith(QStringLiteral("backed up "));
}

bool esEncabezado(const QString &linea)
{
    return linea == QStringLiteral("sending incremental file list") || linea == QStringLiteral("receiving incremental file list")
        || linea.startsWith(QStringLiteral("sent ")) || linea.startsWith(QStringLiteral("total size"))
        || linea == QStringLiteral("./");
}

QString carpetaAnteriores(const QString &celular)
{
    return celular + QStringLiteral(".anteriores");
}

QString nombreVersion(const QDateTime &cuando)
{
    return cuando.toString(QStringLiteral("yyyy-MM-dd_HH-mm"));
}

bool esNombreVersion(const QString &nombre)
{
    static const QRegularExpression version(QStringLiteral("^\\d{4}-\\d\\d-\\d\\d_\\d\\d-\\d\\d$"));
    return version.match(nombre).hasMatch();
}

QString textoVersion(const QString &nombre)
{
    const QDateTime cuando = QDateTime::fromString(nombre, QStringLiteral("yyyy-MM-dd_HH-mm"));
    return cuando.isValid() ? cuando.toString(QStringLiteral("dd/MM/yyyy HH:mm")) : QString();
}

QString argumentoBackupDir(const QString &celular, const QDateTime &cuando)
{
    return QStringLiteral("--backup-dir=../%1/%2").arg(carpetaAnteriores(celular), nombreVersion(cuando));
}

QString rsyncPathConLimpieza(const QString &carpetaPersona, int dias)
{
    // Solo rutas sencillas (la de servidor.json y el nombre de la persona, que ya se valida): nada que el
    // shell pueda interpretar dentro de las comillas simples
    static const QRegularExpression segura(QStringLiteral("^/[A-Za-z0-9/._-]+$"));
    QString carpeta = carpetaPersona;
    while (carpeta.size() > 1 && carpeta.endsWith(u'/'))
        carpeta.chop(1);
    if (!segura.match(carpeta).hasMatch() || carpeta.contains(QStringLiteral("..")) || dias < 1)
        return {};
    // Profundidad 2 desde la persona: <persona>/<algo>.anteriores/<versión>. La fecha que cuenta es la de la
    // carpeta de la versión (los archivos de adentro conservan la suya, que puede ser vieja)
    return QStringLiteral("find '%1' -mindepth 2 -maxdepth 2 -type d -path '*.anteriores/*' -mtime +%2 "
                          "-exec rm -rf '{}' + 2>/dev/null; rsync").arg(carpeta).arg(dias);
}

QString escaparPatron(const QString &nombre)
{
    QString r;
    for (const QChar c : nombre) {
        if (c == u'*' || c == u'?' || c == u'[' || c == u'\\')
            r += u'\\';
        r += c;
    }
    return r;
}

QStringList filtrosListado(const QString &celular)
{
    const QString c = escaparPatron(celular);
    const QString a = escaparPatron(carpetaAnteriores(celular));
    return {QStringLiteral("--include=/%1/").arg(c), QStringLiteral("--include=/%1/*/").arg(c),
            QStringLiteral("--include=/%1/").arg(a), QStringLiteral("--include=/%1/*/").arg(a),
            QStringLiteral("--include=/%1/*/*/").arg(a), QStringLiteral("--exclude=*")};
}

std::optional<EntradaListado> entradaListado(const QString &linea)
{
    // permisos, tamaño (con separador de miles), fecha, hora y el nombre (que puede tener espacios)
    static const QRegularExpression entrada(QStringLiteral(
        "^([dlcbps-])[rwxsStTl-]{9}\\s+[\\d,.]+\\s+\\d{4}/\\d\\d/\\d\\d\\s+\\d\\d:\\d\\d:\\d\\d\\s(.+)$"));
    const QRegularExpressionMatch m = entrada.match(linea);
    if (!m.hasMatch())
        return std::nullopt;
    EntradaListado e;
    e.carpeta = m.captured(1) == u"d";
    e.ruta = m.captured(2);
    if (e.ruta.endsWith(u'/'))
        e.ruta.chop(1);
    return e;
}

OpcionesRestauracion opcionesRestauracion(const QStringList &lineas, const QString &celular)
{
    OpcionesRestauracion o;
    const QString actual = celular + u'/';
    const QString anteriores = carpetaAnteriores(celular) + u'/';
    for (const QString &linea : lineas) {
        const std::optional<EntradaListado> e = entradaListado(linea);
        if (!e || !e->carpeta)
            continue;
        if (e->ruta == celular) {
            o.hayRespaldo = true;
        } else if (e->ruta.startsWith(actual)) {
            const QString resto = e->ruta.mid(actual.size());
            if (!resto.isEmpty() && !resto.contains(u'/'))
                o.carpetasActual << resto;
        } else if (e->ruta.startsWith(anteriores)) {
            const QStringList partes = e->ruta.mid(anteriores.size()).split(u'/');
            if (partes.isEmpty() || !esNombreVersion(partes.at(0)))
                continue;
            if (!o.versiones.contains(partes.at(0)))
                o.versiones << partes.at(0);
            if (partes.size() == 2 && !partes.at(1).isEmpty())
                o.carpetasVersion[partes.at(0)] << partes.at(1);
        }
    }
    o.carpetasActual.sort(Qt::CaseInsensitive);
    std::sort(o.versiones.begin(), o.versiones.end(), std::greater<QString>());    // AAAA-MM-DD: la más nueva primero
    for (auto it = o.carpetasVersion.begin(); it != o.carpetasVersion.end(); ++it)
        it->sort(Qt::CaseInsensitive);
    return o;
}

} // namespace analisisRsync
