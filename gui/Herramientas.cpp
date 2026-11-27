#include "Herramientas.h"
#include "Rutas.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

// En Android, la carpeta del ejecutable es la de las bibliotecas nativas
QString biblioteca(const QString &nombre)
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(nombre);
}

// Va en los assets del APK (no con las bibliotecas): así Android no guarda una copia más al instalar.
// No puede terminar en .gz: el empaquetador de Android descomprime esos assets (quedaría el .tar entero)
const QString archivoTar = QStringLiteral("assets:/entorno.tgz");
constexpr qint64 tamanoTrozo = 1 << 20;     // se le pasa a tar de a 1 MB

QString datos()
{
    return carpetaDatos();
}

QString leer(const QString &ruta);

// Identifica la versión del entorno incluido en la app (su sha256, que anota Preparar-binarios.sh):
// si cambia, hay que descomprimirlo de nuevo
QString firmaTar()
{
    return leer(QStringLiteral("assets:/entorno.firma"));
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
        if (bien)
            consultarVersionYtDlp();
    });
    connect(&descompresor, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            paquete.close();
            emit preparacionTerminada(false, tr("No se pudo ejecutar tar."));
        }
    });
    connect(&descompresor, &QProcess::started, this, &Herramientas::pasarTrozo);
    connect(&descompresor, &QProcess::bytesWritten, this, &Herramientas::pasarTrozo);
}

// Le pasa a tar el siguiente pedazo del paquete (sin cargarlo entero en memoria)
void Herramientas::pasarTrozo()
{
    if (!paquete.isOpen() || descompresor.bytesToWrite() > tamanoTrozo / 4)
        return;
    const QByteArray trozo = paquete.read(tamanoTrozo);
    if (!trozo.isEmpty())
        descompresor.write(trozo);
    if (trozo.size() < tamanoTrozo) {      // terminó (o no se pudo leer más)
        paquete.close();
        descompresor.closeWriteChannel();
    }
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
    const QString firma = firmaTar();
    if (!firma.isEmpty() && leer(marca()) == firma) {
        preparadas = true;
        emit preparacionTerminada(true, {});
        return;
    }
    paquete.setFileName(archivoTar);
    if (firma.isEmpty() || !paquete.open(QIODevice::ReadOnly)) {
        emit preparacionTerminada(false, tr("Falta entorno.tgz en la app."));
        return;
    }
    // Se descomprime en otra carpeta: si se corta a la mitad, no queda un entorno roto
    const QString destino = entorno() + QStringLiteral(".nuevo");
    QDir(destino).removeRecursively();
    QDir().mkpath(destino);
    descompresor.setProcessChannelMode(QProcess::MergedChannels);
    // «-f -»: lo lee de la entrada estándar, que va llenando pasarTrozo()
    descompresor.start(QStringLiteral("/system/bin/tar"),
                       {QStringLiteral("xzf"), QStringLiteral("-"), QStringLiteral("-C"), destino});
}

void Herramientas::terminarPreparacion()
{
    const QString destino = entorno() + QStringLiteral(".nuevo");
    paquete.close();
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
    consultarVersionYtDlp();
}

// «yt-dlp --version», aparte y solo si el archivo cambió (Python tarda en arrancar): se recuerda junto con
// la fecha de yt-dlp
void Herramientas::consultarVersionYtDlp()
{
    const QFileInfo ytDlp(QDir(entorno()).filePath(QStringLiteral("yt-dlp")));
    const QString huella = QString::number(ytDlp.lastModified().toMSecsSinceEpoch()) + u'-' + QString::number(ytDlp.size());
    QSettings ajustes(archivoAjustes(), QSettings::IniFormat);
    if (ajustes.value(QStringLiteral("ytdlp/huella")).toString() == huella) {
        versionYtDlp_ = ajustes.value(QStringLiteral("ytdlp/version")).toString();
        emit versionYtDlpCambio();
        return;
    }
    auto *consulta = new QProcess(this);
    consulta->setProcessEnvironment(ambiente());
    connect(consulta, &QProcess::finished, this, [this, consulta, huella](int codigo, QProcess::ExitStatus estado) {
        const QString version = QString::fromUtf8(consulta->readAllStandardOutput()).trimmed().section(u'\n', -1);
        consulta->deleteLater();
        if (estado != QProcess::NormalExit || codigo != 0 || version.isEmpty() || version.size() > 40)
            return;
        versionYtDlp_ = version;
        QSettings ajustes(archivoAjustes(), QSettings::IniFormat);
        ajustes.setValue(QStringLiteral("ytdlp/huella"), huella);
        ajustes.setValue(QStringLiteral("ytdlp/version"), version);
        emit versionYtDlpCambio();
    });
    connect(consulta, &QProcess::errorOccurred, consulta, [consulta](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            consulta->deleteLater();
    });
    consulta->start(python(), {QDir(entorno()).filePath(QStringLiteral("yt-dlp")), QStringLiteral("--version")});
}

QString Herramientas::rsync() const
{
    return biblioteca(QStringLiteral("librsync.so"));
}

QString Herramientas::dbclient() const
{
    return biblioteca(QStringLiteral("libdbclient.so"));
}

QString Herramientas::dropbearkey() const
{
    return biblioteca(QStringLiteral("libdropbearkey.so"));
}

bool Herramientas::respaldoDisponible() const
{
    return QFileInfo::exists(rsync()) && QFileInfo::exists(dbclient());
}
