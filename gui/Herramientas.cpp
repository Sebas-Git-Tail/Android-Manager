#include "Herramientas.h"
#include "Rutas.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

// En Android, la carpeta del ejecutable es la de las bibliotecas nativas
QString biblioteca(const QString &nombre)
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(nombre);
}

QString archivoTar()
{
    return biblioteca(QStringLiteral("libentorno.tar.so"));
}

QString datos()
{
    return carpetaDatos();
}

// Identifica la versión del entorno incluido en la app: si cambia, hay que descomprimirlo de nuevo
QString firmaTar()
{
    const QFileInfo info(archivoTar());
    return QStringLiteral("%1-%2").arg(info.size()).arg(info.lastModified().toSecsSinceEpoch());
}

QString leer(const QString &ruta)
{
    QFile archivo(ruta);
    return archivo.open(QIODevice::ReadOnly) ? QString::fromUtf8(archivo.readAll()).trimmed() : QString();
}

void escribir(const QString &ruta, const QString &texto)
{
    QFile archivo(ruta);
    if (archivo.open(QIODevice::WriteOnly))
        archivo.write(texto.toUtf8());
}

} // namespace

Herramientas::Herramientas(QObject *parent)
    : QObject(parent)
{
    connect(&descompresor, &QProcess::finished, this, &Herramientas::terminarPreparacion);
    // Las propiedades que mira QML siguen a la señal de siempre
    connect(this, &Herramientas::preparacionTerminada, this, [this](bool bien, const QString &error) {
        preparando_ = false;
        error_ = bien ? QString() : error;
        emit estadoCambio();
    });
    connect(&descompresor, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            emit preparacionTerminada(false, tr("No se pudo ejecutar tar."));
    });
}

QString Herramientas::entorno() const
{
    return QDir(datos()).filePath(QStringLiteral("entorno"));
}

QString Herramientas::marca() const
{
    return QDir(entorno()).filePath(QStringLiteral(".firma"));
}

void Herramientas::preparar()
{
    preparando_ = true;
    error_.clear();
    emit estadoCambio();
    crearEnlaces();
    if (leer(marca()) == firmaTar()) {
        preparadas = true;
        emit preparacionTerminada(true, {});
        return;
    }
    if (!QFileInfo::exists(archivoTar())) {
        emit preparacionTerminada(false, tr("Falta libentorno.tar.so en la app."));
        return;
    }
    // Se descomprime en otra carpeta: si se corta a la mitad, no queda un entorno roto
    const QString destino = entorno() + QStringLiteral(".nuevo");
    QDir(destino).removeRecursively();
    QDir().mkpath(destino);
    descompresor.setProcessChannelMode(QProcess::MergedChannels);
    descompresor.start(QStringLiteral("/system/bin/tar"), {QStringLiteral("xzf"), archivoTar(), QStringLiteral("-C"), destino});
}

void Herramientas::terminarPreparacion()
{
    const QString destino = entorno() + QStringLiteral(".nuevo");
    if (descompresor.exitStatus() != QProcess::NormalExit || descompresor.exitCode() != 0) {
        const QString salida = QString::fromUtf8(descompresor.readAll()).trimmed();
        QDir(destino).removeRecursively();
        emit preparacionTerminada(false, tr("No se pudo preparar la app: %1").arg(salida));
        return;
    }
    QDir(entorno()).removeRecursively();
    if (!QDir().rename(destino, entorno())) {
        emit preparacionTerminada(false, tr("No se pudo preparar la app."));
        return;
    }
    escribir(marca(), firmaTar());
    preparadas = true;
    emit preparacionTerminada(true, {});
}

// yt-dlp busca ffmpeg y ffprobe por su nombre. La carpeta de bibliotecas cambia con cada
// instalación, así que los enlaces se rehacen cada vez que se abre la app.
void Herramientas::crearEnlaces()
{
    const QDir bin(QDir(datos()).filePath(QStringLiteral("bin")));
    QDir().mkpath(bin.path());
    for (const QString &programa : {QStringLiteral("ffmpeg"), QStringLiteral("ffprobe")}) {
        QFile::remove(bin.filePath(programa));
        QFile::link(biblioteca(QStringLiteral("lib%1.so").arg(programa)), bin.filePath(programa));
    }
}

QProcessEnvironment Herramientas::ambiente() const
{
    const QDir base(entorno());
    const QString usrPython = base.filePath(QStringLiteral("python/usr"));
    QProcessEnvironment variables = QProcessEnvironment::systemEnvironment();
    variables.insert(QStringLiteral("HOME"), datos());
    variables.insert(QStringLiteral("TMPDIR"), temporal());
    variables.insert(QStringLiteral("PATH"), QDir(datos()).filePath(QStringLiteral("bin")) + QStringLiteral(":/system/bin"));
    variables.insert(QStringLiteral("LD_LIBRARY_PATH"),
                     QStringList{usrPython + QStringLiteral("/lib"), base.filePath(QStringLiteral("ffmpeg/usr/lib")),
                                 QCoreApplication::applicationDirPath()}.join(u':'));
    variables.insert(QStringLiteral("PYTHONHOME"), usrPython);
    variables.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));    // el avance llega al momento
    variables.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    variables.insert(QStringLiteral("SSL_CERT_FILE"), usrPython + QStringLiteral("/etc/tls/cert.pem"));
    return variables;
}

QString Herramientas::temporal() const
{
    const QString carpeta = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(carpeta);
    return carpeta;
}

QString Herramientas::python() const
{
    return biblioteca(QStringLiteral("libpython.so"));
}

QStringList Herramientas::argumentosYtDlp() const
{
    return {QDir(entorno()).filePath(QStringLiteral("yt-dlp")),
            QStringLiteral("--ffmpeg-location"), QDir(datos()).filePath(QStringLiteral("bin")),
            QStringLiteral("--js-runtimes"), QStringLiteral("quickjs:") + biblioteca(QStringLiteral("libqjs.so")),
            QStringLiteral("--cache-dir"), QDir(temporal()).filePath(QStringLiteral("yt-dlp"))};
}

bool Herramientas::ytDlpDesactualizado() const
{
    const QFileInfo marcaActualizacion(QDir(datos()).filePath(QStringLiteral(".yt-dlp-actualizado")));
    return !marcaActualizacion.exists()
        || marcaActualizacion.lastModified().secsTo(QDateTime::currentDateTime()) > 24 * 60 * 60;
}

void Herramientas::marcarYtDlpActualizado()
{
    escribir(QDir(datos()).filePath(QStringLiteral(".yt-dlp-actualizado")), QDateTime::currentDateTime().toString(Qt::ISODate));
}

QString Herramientas::rsync() const
{
    return biblioteca(QStringLiteral("librsync.so"));
}

QString Herramientas::dbclient() const
{
    return biblioteca(QStringLiteral("libdbclient.so"));
}

bool Herramientas::respaldoDisponible() const
{
    return QFileInfo::exists(rsync()) && QFileInfo::exists(dbclient());
}
