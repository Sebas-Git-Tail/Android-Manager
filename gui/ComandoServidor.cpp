#include "ComandoServidor.h"

#include "ConfigServidor.h"

#include <QDir>
#include <QFile>
#include <QTcpSocket>
#include <QTimer>

#include <csignal>
#include <unistd.h>

namespace {

constexpr int EsperaServidorMs = 2000;

QStringList servidores() { return ConfigServidor::instancia().ipsSsh(); }

// La última dirección que contestó: se prueba primero (con varios pedidos seguidos no se espera
// cada vez a que falle la de casa estando afuera)
QString &ultimoServidor()
{
    static QString ultimo;
    return ultimo;
}

} // namespace

ComandoServidor::ComandoServidor(Herramientas *herramientas, const QString &archivoClave, QObject *parent)
    : QObject(parent), herramientas(herramientas), archivoClave(archivoClave)
{
    proceso.setProcessChannelMode(QProcess::SeparateChannels);
    proceso.setChildProcessModifier([] { ::setpgid(0, 0); });   // Cancelar corta todo el grupo
    connect(&proceso, &QProcess::readyReadStandardOutput, this, [this] {
        pendiente += proceso.readAllStandardOutput();
        qsizetype corte;
        while ((corte = pendiente.indexOf('\n')) >= 0) {
            const QString texto = QString::fromUtf8(pendiente.left(corte)).trimmed();
            pendiente.remove(0, corte + 1);
            emit linea(texto);
        }
    });
    connect(&proceso, &QProcess::readyReadStandardError, this, [this] { errores += proceso.readAllStandardError(); });
    connect(&proceso, &QProcess::finished, this, [this](int codigo, QProcess::ExitStatus estado) {
        if (!pendiente.trimmed().isEmpty())
            emit linea(QString::fromUtf8(pendiente).trimmed());
        pendiente.clear();
        const QString error = QString::fromUtf8(errores);
        if (cancelado)
            fin(Cancelado);
        else if (estado == QProcess::NormalExit && codigo == 0)
            fin(Bien);
        else if (error.contains(QStringLiteral("No auth methods could be used")) || error.contains(QStringLiteral("Permission denied")))
            fin(Rechazada, codigo);
        else if (error.contains(QStringLiteral("Host key mismatch")) || error.contains(QStringLiteral("not in known_hosts")))
            fin(HuellaDistinta, codigo);
        else if (deCache && (error.contains(QStringLiteral("Connection")) || error.contains(QStringLiteral("No route"))
                             || error.contains(QStringLiteral("Network is unreachable")))) {
            // La dirección de la vez anterior ya no anda (p. ej. el celular salió de casa): se busca de nuevo
            ultimoServidor().clear();
            deCache = false;
            errores.clear();
            probar(0);
        } else
            fin(Fallo, codigo);
    });
    connect(&proceso, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            fin(Fallo, -1);
    });
}

ComandoServidor::~ComandoServidor()
{
    cancelar();
    if (proceso.state() != QProcess::NotRunning) {
        // Al cerrar la app: sin esperar segundos (SIGKILL no se puede ignorar)
        ::kill(-static_cast<pid_t>(proceso.processId()), SIGKILL);
        proceso.waitForFinished(300);
    }
}

void ComandoServidor::iniciar(const QString &pedido)
{
    if (ocupado_)
        return;
    ocupado_ = true;
    cancelado = false;
    pedido_ = pedido;
    pendiente.clear();
    errores.clear();
    deCache = !ultimoServidor().isEmpty() && servidores().contains(ultimoServidor());
    if (deCache)
        conectar(ultimoServidor());
    else
        probar(0);
}

// Prueba el puerto de SSH de cada dirección, en orden (primero la red de casa, después Tailscale)
void ComandoServidor::probar(int indice)
{
    if (cancelado) {
        fin(Cancelado);
        return;
    }
    if (indice >= servidores().size()) {
        fin(SinServidor);
        return;
    }
    prueba = new QTcpSocket(this);
    auto *espera = new QTimer(prueba);
    espera->setSingleShot(true);
    const auto fallo = [this, indice] {
        prueba->disconnect(this);
        prueba->deleteLater();
        prueba = nullptr;
        probar(indice + 1);
    };
    connect(prueba, &QTcpSocket::connected, this, [this, indice] {
        prueba->disconnect(this);
        prueba->abort();
        prueba->deleteLater();
        prueba = nullptr;
        if (cancelado)
            fin(Cancelado);
        else
            conectar(servidores().at(indice));
    });
    connect(prueba, &QTcpSocket::errorOccurred, this, fallo);
    connect(espera, &QTimer::timeout, this, fallo);
    prueba->connectToHost(servidores().at(indice), 22);
    espera->start(EsperaServidorMs);
}

void ComandoServidor::conectar(const QString &servidor)
{
    // known_hosts con las huellas compiladas en la app: dbclient no acepta otro equipo
    const QString home = herramientas->ambiente().value(QStringLiteral("HOME"));
    QDir().mkpath(home + QStringLiteral("/.ssh"));
    QFile huellas(home + QStringLiteral("/.ssh/known_hosts"));
    if (huellas.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        huellas.write(ConfigServidor::huellasSsh(servidores()).join(u'\n').toUtf8() + '\n');
    huellas.close();

    ultimoServidor() = servidor;
    proceso.setProcessEnvironment(herramientas->ambiente());
    proceso.start(herramientas->dbclient(),
                  {QStringLiteral("-i"), archivoClave, QStringLiteral("-T"),
                   QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
                   QStringLiteral("-o"), QStringLiteral("PasswordAuthentication=no"),
                   QStringLiteral("%1@%2").arg(ConfigServidor::instancia().usuario(), servidor), pedido_});
}

void ComandoServidor::cancelar()
{
    if (!ocupado_)
        return;
    cancelado = true;
    if (proceso.state() != QProcess::NotRunning)
        ::kill(-static_cast<pid_t>(proceso.processId()), SIGTERM);
}

void ComandoServidor::fin(Resultado resultado, int codigo)
{
    if (resultado == SinServidor || (resultado == Fallo && codigo == -1))
        ultimoServidor().clear();
    ocupado_ = false;
    emit terminado(resultado, codigo);
}

QString ComandoServidor::explicar(Resultado resultado, int codigo)
{
    switch (resultado) {
    case Bien:
    case Cancelado:
        return {};
    case Rechazada:
        return tr("El servidor todavía no autorizó este celular (sección Servidor: copiá la clave y autorizala con autorizar-equipo.sh).");
    case HuellaDistinta:
        return tr("El equipo que contestó no es el servidor (su huella no coincide). No se mandó nada.");
    case SinServidor:
        return tr("No se encontró el servidor (ni en la red de casa ni por Tailscale).");
    case Fallo:
        return codigo == -1 ? tr("No se pudo ejecutar el cliente SSH.") : tr("No se pudo terminar (código %1).").arg(codigo);
    }
    return {};
}
