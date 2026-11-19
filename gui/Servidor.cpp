#include "Servidor.h"

#include "ConfigServidor.h"
#include "Plataforma.h"
#include "Rutas.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QTcpSocket>
#include <QTimer>

#include <csignal>
#include <unistd.h>

namespace {

const QString ClaveActivado = QStringLiteral("servidor/activado");
const QString ClaveAutorizado = QStringLiteral("servidor/autorizado");
constexpr int EsperaServidorMs = 2000;
constexpr int MaxLineas = 200;

// Las que acepta acciones-servidor.sh (el servidor igual rechaza cualquier otra)
const QStringList Acciones{QStringLiteral("miniaturas-bodrio"), QStringLiteral("descargas"),
                           QStringLiteral("indices-instituto")};

QSettings ajustes()
{
    return QSettings(archivoAjustes(), QSettings::IniFormat);
}

QStringList servidores() { return ConfigServidor::instancia().ipsSsh(); }

} // namespace

Servidor::Servidor(Herramientas *herramientas, QObject *parent)
    : QObject(parent), herramientas(herramientas)
{
    proceso.setProcessChannelMode(QProcess::SeparateChannels);
    proceso.setChildProcessModifier([] { ::setpgid(0, 0); });   // Cancelar corta todo el grupo
    connect(&proceso, &QProcess::readyReadStandardOutput, this, [this] {
        pendiente += proceso.readAllStandardOutput();
        qsizetype corte;
        while ((corte = pendiente.indexOf('\n')) >= 0) {
            salida_ << QString::fromUtf8(pendiente.left(corte)).trimmed();
            pendiente.remove(0, corte + 1);
        }
        while (salida_.size() > MaxLineas)
            salida_.removeFirst();
        emit salidaCambio();
    });
    connect(&proceso, &QProcess::readyReadStandardError, this, [this] { errores += proceso.readAllStandardError(); });
    connect(&proceso, &QProcess::finished, this, &Servidor::terminado);
    connect(&proceso, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            ponerEstado(tr("No se pudo ejecutar el cliente SSH: %1").arg(proceso.errorString()), Error);
            terminar();
        }
    });
    if (activado())
        leerClavePublica();
}

QString Servidor::archivoClave() const
{
    return herramientas->ambiente().value(QStringLiteral("HOME")) + QStringLiteral("/.ssh/id_servidor");
}

bool Servidor::activado() const
{
    return ajustes().value(ClaveActivado, false).toBool();
}

bool Servidor::disponible() const
{
    return QFileInfo::exists(herramientas->dbclient()) && QFileInfo::exists(herramientas->dropbearkey());
}

bool Servidor::autorizado() const
{
    return ajustes().value(ClaveAutorizado, false).toBool();
}

// Al crearla, dropbearkey deja también <clave>.pub (con el comentario de -C). Si no está, se le pide
// con «-y», que escribe «Public key portion is:», la clave en una línea y la huella.
void Servidor::leerClavePublica()
{
    QFile pub(archivoClave() + QStringLiteral(".pub"));
    if (pub.open(QIODevice::ReadOnly)) {
        const QByteArray linea = pub.readLine().trimmed();
        if (linea.startsWith("ssh-")) {
            clavePublica_ = QString::fromUtf8(linea);
            return;
        }
    }
    QProcess lector;
    lector.setProcessEnvironment(herramientas->ambiente());
    lector.start(herramientas->dropbearkey(), {QStringLiteral("-y"), QStringLiteral("-f"), archivoClave()});
    lector.waitForFinished(10000);
    clavePublica_.clear();
    const QList<QByteArray> lineas = lector.readAllStandardOutput().split('\n');
    for (const QByteArray &linea : lineas) {
        if (linea.startsWith("ssh-")) {
            clavePublica_ = QString::fromUtf8(linea).trimmed();
            break;
        }
    }
}

void Servidor::activar()
{
    if (!disponible()) {
        ponerEstado(tr("Esta versión de la app no incluye el cliente SSH."), Error);
        return;
    }
    if (!QFileInfo::exists(archivoClave())) {
        QDir().mkpath(QFileInfo(archivoClave()).absolutePath());
        QProcess creador;
        creador.setProcessEnvironment(herramientas->ambiente());
        // El comentario dice de qué celular es (se ve en «autorizar-equipo.sh --lista»)
        QString celular = Plataforma::nombreDispositivo();
        celular.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
        creador.start(herramientas->dropbearkey(), {QStringLiteral("-t"), QStringLiteral("ed25519"),
                                                    QStringLiteral("-f"), archivoClave(),
                                                    QStringLiteral("-C"), QStringLiteral("android-manager@") + celular});
        if (!creador.waitForFinished(30000) || creador.exitCode() != 0) {
            ponerEstado(tr("No se pudo crear la clave del celular: %1")
                            .arg(QString::fromUtf8(creador.readAllStandardError()).trimmed()), Error);
            return;
        }
        QFile::setPermissions(archivoClave(), QFile::ReadOwner | QFile::WriteOwner);
    }
    leerClavePublica();
    if (clavePublica_.isEmpty()) {
        ponerEstado(tr("No se pudo leer la clave del celular."), Error);
        return;
    }
    auto a = ajustes();
    a.setValue(ClaveActivado, true);
    emit activadoCambio();
}

void Servidor::desactivar()
{
    cancelar();
    QFile::remove(archivoClave());
    QFile::remove(archivoClave() + QStringLiteral(".pub"));
    auto a = ajustes();
    a.remove(ClaveActivado);
    a.remove(ClaveAutorizado);
    clavePublica_.clear();
    salida_.clear();
    ponerEstado({});
    emit salidaCambio();
    emit autorizadoCambio();
    emit activadoCambio();
}

void Servidor::copiarClave()
{
    if (clavePublica_.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(clavePublica_);
    emit avisar(tr("Clave copiada"));
}

void Servidor::ejecutar(const QString &accion)
{
    if (ocupado() || !Acciones.contains(accion) || !activado())
        return;
    accion_ = accion;
    cancelado = false;
    salida_.clear();
    pendiente.clear();
    errores.clear();
    emit salidaCambio();
    emit ocupadoCambio();
    ponerEstado(tr("Buscando el servidor…"));
    probarServidor(0);
}

// Prueba el puerto de SSH de cada dirección, en orden (primero la red de casa, después Tailscale)
void Servidor::probarServidor(int indice)
{
    if (cancelado || indice >= servidores().size()) {
        if (cancelado)
            ponerEstado(tr("Cancelado."));
        else
            ponerEstado(tr("No se encontró el servidor (ni en la red de casa ni por Tailscale)."), Error);
        terminar();
        return;
    }
    prueba = new QTcpSocket(this);
    auto *espera = new QTimer(prueba);
    espera->setSingleShot(true);
    const auto fallo = [this, indice] {
        prueba->disconnect(this);
        prueba->deleteLater();
        prueba = nullptr;
        probarServidor(indice + 1);
    };
    connect(prueba, &QTcpSocket::connected, this, [this, indice] {
        prueba->disconnect(this);
        prueba->abort();
        prueba->deleteLater();
        prueba = nullptr;
        if (cancelado)
            probarServidor(int(servidores().size()));
        else
            conectar(servidores().at(indice));
    });
    connect(prueba, &QTcpSocket::errorOccurred, this, fallo);
    connect(espera, &QTimer::timeout, this, fallo);
    prueba->connectToHost(servidores().at(indice), 22);
    espera->start(EsperaServidorMs);
}

void Servidor::conectar(const QString &servidor)
{
    // known_hosts con las huellas compiladas en la app (como el respaldo): dbclient no acepta otro equipo
    const QString home = herramientas->ambiente().value(QStringLiteral("HOME"));
    QDir().mkpath(home + QStringLiteral("/.ssh"));
    QFile huellas(home + QStringLiteral("/.ssh/known_hosts"));
    if (huellas.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        huellas.write(ConfigServidor::huellasSsh(servidores()).join(u'\n').toUtf8() + '\n');
    huellas.close();

    ponerEstado(servidor == servidores().constFirst() ? tr("Trabajando en el servidor…")
                                                       : tr("Trabajando en el servidor (por Tailscale)…"));
    // Lo que se manda es solo el nombre de la acción: el comando lo decide el servidor (command= en
    // authorized_keys). Sin contraseña nunca: BatchMode y PasswordAuthentication=no.
    proceso.setProcessEnvironment(herramientas->ambiente());
    proceso.start(herramientas->dbclient(),
                  {QStringLiteral("-i"), archivoClave(), QStringLiteral("-T"),
                   QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
                   QStringLiteral("-o"), QStringLiteral("PasswordAuthentication=no"),
                   QStringLiteral("%1@%2").arg(ConfigServidor::instancia().usuario(), servidor), accion_});
}

void Servidor::terminado(int codigo, QProcess::ExitStatus estado)
{
    if (!pendiente.trimmed().isEmpty())
        salida_ << QString::fromUtf8(pendiente).trimmed();
    pendiente.clear();
    emit salidaCambio();
    const QString error = QString::fromUtf8(errores);
    QString ultima;
    for (auto it = salida_.crbegin(); it != salida_.crend() && ultima.isEmpty(); ++it)
        ultima = *it;

    const bool rechazada = error.contains(QStringLiteral("No auth methods could be used"))
                           || error.contains(QStringLiteral("Permission denied"));
    auto a = ajustes();
    if (cancelado) {
        ponerEstado(tr("Cancelado."));
    } else if (estado == QProcess::NormalExit && codigo == 0) {
        a.setValue(ClaveAutorizado, true);
        ponerEstado(ultima.isEmpty() ? tr("Listo.") : ultima, Exito);
    } else if (rechazada) {
        a.setValue(ClaveAutorizado, false);
        ponerEstado(tr("El servidor todavía no autorizó este celular. Copiá su clave y autorizala allá con autorizar-equipo.sh."), Error);
    } else if (error.contains(QStringLiteral("Host key mismatch")) || error.contains(QStringLiteral("not in known_hosts"))) {
        ponerEstado(tr("El equipo que contestó no es el servidor (su huella no coincide). No se mandó nada."), Error);
    } else {
        a.setValue(ClaveAutorizado, true);    // entró: el que falló fue el script
        ponerEstado(tr("El servidor no pudo terminar (código %1).").arg(codigo), Error);
    }
    emit autorizadoCambio();
    terminar();
}

void Servidor::cancelar()
{
    if (!ocupado())
        return;
    cancelado = true;
    if (proceso.state() != QProcess::NotRunning)
        ::kill(-static_cast<pid_t>(proceso.processId()), SIGTERM);
}

void Servidor::cancelarYEsperar()
{
    cancelar();
    if (proceso.state() != QProcess::NotRunning)
        proceso.waitForFinished(3000);
}

void Servidor::ponerEstado(const QString &texto, TipoEstado tipo)
{
    estado_ = texto;
    tipoEstado_ = tipo;
    emit estadoCambio();
}

void Servidor::terminar()
{
    accion_.clear();
    emit ocupadoCambio();
}
