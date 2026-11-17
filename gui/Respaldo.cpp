#include "Respaldo.h"

#include "ConfigServidor.h"
#include "Plataforma.h"
#include "Rutas.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QTcpSocket>
#include <QTimer>

#include <csignal>
#include <unistd.h>

namespace {

// Datos del servidor: los de servidor.json (ConfigServidor, igual que Common/Servidor.sh en Linux).
// Primero la red de casa y, si no contesta, Tailscale.
QStringList servidores() { return ConfigServidor::instancia().ipsSsh(); }
QString usuario() { return ConfigServidor::instancia().usuario(); }
QString carpetaRespaldos() { return ConfigServidor::instancia().carpetaRespaldos(); }
const QString ClaveUltimoRespaldo = QStringLiteral("respaldo/ultimo");
constexpr int DiasRecordatorio = 7;
const QString ClaveNombre = QStringLiteral("respaldo/nombre");
const QString ClaveCelular = QStringLiteral("respaldo/celular");
const QString ClaveDestinoConfirmado = QStringLiteral("respaldo/destinoConfirmado");
const QString Origen = QStringLiteral("/sdcard/");
constexpr int EsperaServidorMs = 2000;

QSettings ajustes()
{
    return QSettings(archivoAjustes(), QSettings::IniFormat);
}

} // namespace

Respaldo::Respaldo(Herramientas *herramientas, QObject *parent)
    : QObject(parent), herramientas(herramientas)
{
    const QSettings guardados(archivoAjustes(), QSettings::IniFormat);
    persona_ = guardados.value(ClaveNombre).toString();
    // Se propone el nombre del dispositivo, pero una vez guardado es fijo: si cambiara solo
    // cuando alguien renombra el celular en Ajustes, el respaldo empezaría en otra carpeta
    celular_ = guardados.value(ClaveCelular, Plataforma::nombreDispositivo()).toString();

    proceso.setProcessChannelMode(QProcess::MergedChannels);
    // rsync y dbclient en su propio grupo: Cancelar los corta a los dos
    proceso.setChildProcessModifier([] { setpgid(0, 0); });
    connect(&proceso, &QProcess::readyReadStandardOutput, this, &Respaldo::leerSalida);
    connect(&proceso, &QProcess::finished, this, &Respaldo::terminoProceso);
    connect(&proceso, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        ponerEstado(tr("No se pudo ejecutar rsync: %1").arg(proceso.errorString()), Error);
        ponerProgreso(-1, {});
        ponerTrabajando(false);
    });
}

void Respaldo::ponerPersona(const QString &texto)
{
    if (texto == persona_)
        return;
    persona_ = texto;
    guardarNombres();
    emit nombresCambio();
}

void Respaldo::ponerCelular(const QString &texto)
{
    if (texto == celular_)
        return;
    celular_ = texto;
    guardarNombres();
    emit nombresCambio();
}

// El nombre es una carpeta del servidor: nada de barras ni «..», que la sacarían de Respaldos
QString Respaldo::persona() const
{
    static const QRegularExpression valida(QStringLiteral("^[a-z0-9][a-z0-9._-]*$"));
    const QString nombre = persona_.trimmed().toLower();    // las carpetas van en minúsculas
    return valida.match(nombre).hasMatch() ? nombre : QString();
}

// Admite espacios y mayúsculas (Galaxy S22), pero no puede salirse de la carpeta de la persona
QString Respaldo::celular() const
{
    static const QRegularExpression valido(QStringLiteral("^[^/\\\\.\\p{Cc}][^/\\\\\\p{Cc}]*$"));
    const QString nombre = celular_.trimmed();
    return valido.match(nombre).hasMatch() ? nombre : QString();
}

// La carpeta de destino lleva la persona y el nombre del celular
QString Respaldo::rutaEnServidor() const
{
    return carpetaRespaldos() + persona() + u'/' + celular() + u'/';
}

QString Respaldo::destino() const
{
    return persona().isEmpty() || celular().isEmpty() ? QString() : rutaEnServidor();
}

// Se guardan apenas se escriben, para que sigan ahí al volver a abrir la app. Haberlos escrito
// no confirma el destino: eso se pregunta antes de la primera copia real.
void Respaldo::guardarNombres()
{
    QSettings guardados = ajustes();
    if (persona().isEmpty())
        guardados.remove(ClaveNombre);
    else
        guardados.setValue(ClaveNombre, persona());
    if (celular().isEmpty())
        guardados.remove(ClaveCelular);
    else
        guardados.setValue(ClaveCelular, celular());
}

void Respaldo::cancelarYEsperar()
{
    cancelar();
    proceso.waitForFinished(3000);
}

void Respaldo::ponerTrabajando(bool valor)
{
    if (trabajando == valor)
        return;
    trabajando = valor;
    emit ocupadaCambio();
}

void Respaldo::ponerEstado(const QString &texto, TipoEstado tipo)
{
    estado_ = texto;
    tipoEstado_ = tipo;
    emit estadoCambio();
}

void Respaldo::ponerProgreso(int progreso, const QString &archivo)
{
    progreso_ = progreso;
    archivo_ = archivo;
    emit progresoCambio();
}

void Respaldo::anotar(const QString &texto)
{
    avisos_ << texto;
    emit avisosCambio();
}

void Respaldo::limpiarAvisos()
{
    avisos_.clear();
    emit avisosCambio();
}

void Respaldo::empezar(const QString &contrasenaEscrita)
{
    if (trabajando)
        return;
    if (!disponible()) {
        ponerEstado(tr("Esta versión de la app no incluye rsync ni el cliente SSH."), Error);
        return;
    }
    if (persona().isEmpty()) {
        ponerEstado(tr("Escribí de quién es el celular: una palabra, con letras, números, punto o guion."), Error);
        emit enfocar(QStringLiteral("persona"));
        return;
    }
    if (celular().isEmpty()) {
        ponerEstado(tr("Escribí un nombre para el celular, sin barras y sin empezar con un punto."), Error);
        emit enfocar(QStringLiteral("celular"));
        return;
    }
    if (contrasenaEscrita.isEmpty()) {
        ponerEstado(tr("Escribí la contraseña SSH del servidor."), Error);
        emit enfocar(QStringLiteral("contrasena"));
        return;
    }
    // Con --delete, un origen que se ve vacío borraría todo el respaldo del servidor
    if (!Plataforma::tieneAccesoArchivos()
        || QDir(Origen).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        ponerEstado(tr("La app no puede leer el almacenamiento: falta dar acceso a los archivos (botón en la portada)."), Error);
        return;
    }
    ponerEstado({});
    contrasena = contrasenaEscrita;
    simulando = simular_;
    // Una simulación no toca nada: ni pregunta ni guarda el destino
    if (!simulando && ajustes().value(ClaveDestinoConfirmado).toString() != rutaEnServidor()) {
        emit pedirConfirmacion(rutaEnServidor());
        return;
    }
    continuar();
}

void Respaldo::confirmarDestino(bool aceptado)
{
    if (trabajando || contrasena.isEmpty())
        return;
    if (!aceptado) {
        contrasena.clear();
        return;
    }
    ajustes().setValue(ClaveDestinoConfirmado, rutaEnServidor());
    continuar();
}

void Respaldo::continuar()
{
    emit contrasenaUsada();    // no queda escrita en la página
    cancelado = false;
    errorConexion.clear();
    sinDatos = false;
    pendiente.clear();
    resumen = ResumenSimulacion();
    avisos_.clear();
    emit avisosCambio();
    ponerProgreso(0, {});
    ponerEstado(tr("Buscando el servidor…"));
    ponerTrabajando(true);
    probarServidor(0);
}

// Prueba el puerto de SSH de cada dirección, en orden
void Respaldo::probarServidor(int indice)
{
    if (cancelado || indice >= servidores().size()) {
        if (cancelado)
            ponerEstado(tr("Respaldo cancelado."));
        else
            ponerEstado(tr("No se encontró el servidor (ni en la red de casa ni por Tailscale)."), Error);
        contrasena.clear();
        ponerProgreso(-1, {});
        ponerTrabajando(false);
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
            copiar(servidores().at(indice));
    });
    connect(prueba, &QTcpSocket::errorOccurred, this, fallo);
    connect(espera, &QTimer::timeout, this, fallo);
    prueba->connectToHost(servidores().at(indice), 22);
    espera->start(EsperaServidorMs);
}

// known_hosts de dbclient (en $HOME/.ssh de la app) con las huellas del servidor para sus IPs
static void escribirHuellas(const QString &home)
{
    QDir().mkpath(home + QStringLiteral("/.ssh"));
    QFile archivo(home + QStringLiteral("/.ssh/known_hosts"));
    if (archivo.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        archivo.write(ConfigServidor::huellasSsh(servidores()).join(u'\n').toUtf8() + '\n');
}

void Respaldo::copiar(const QString &servidor)
{
    escribirHuellas(herramientas->ambiente().value(QStringLiteral("HOME")));
    if (servidor != servidores().constFirst())
        anotar(tr("Conectando por Tailscale (%1).").arg(servidor));
    anotar(tr("%1 → %2:%3").arg(Origen, servidor, rutaEnServidor()));
    ponerEstado(simulando ? tr("Simulando el respaldo (no cambia nada)…") : tr("Respaldando…"));

    QStringList argumentos{QStringLiteral("-ah"), QStringLiteral("--info=progress2,name1"),
                           // sin esto, rsync escribe las tildes de los nombres como \\#303\\#261
                           QStringLiteral("-8"),
                           // Sin -y: dbclient solo acepta al servidor si su huella está en known_hosts, y
                           // ese archivo lo escribe escribirHuellas() con las huellas compiladas en la app
                           // (antes la primera vez se aceptaba cualquiera y otro equipo podía recibir la contraseña)
                           QStringLiteral("-e"), herramientas->dbclient(),
                           // la ruta puede tener espacios (el nombre del celular)
                           QStringLiteral("--protect-args"),
                           // la carpeta de la persona puede no existir todavía
                           QStringLiteral("--mkpath"),
                           QStringLiteral("--exclude"), QStringLiteral("Android/obb"),
                           QStringLiteral("--exclude"), QStringLiteral("Android/data"),
                           QStringLiteral("--delete")};
    if (simulando) {
        argumentos << QStringLiteral("--dry-run") << QStringLiteral("--itemize-changes") << QStringLiteral("--stats")
                   << QStringLiteral("--info=del1");
    }
    argumentos << Origen << QStringLiteral("%1@%2:%3").arg(usuario(), servidor, rutaEnServidor());

    // dbclient toma la contraseña de esta variable: no aparece en la lista de procesos
    QProcessEnvironment variables = herramientas->ambiente();
    variables.insert(QStringLiteral("DROPBEAR_PASSWORD"), contrasena);
    contrasena.clear();
    proceso.setProcessEnvironment(variables);
    proceso.start(herramientas->rsync(), argumentos);
    proceso.setProcessEnvironment(herramientas->ambiente());    // no queda guardada
}

void Respaldo::cancelar()
{
    if (!trabajando)
        return;
    cancelado = true;
    ponerEstado(tr("Cancelando…"));
    if (proceso.state() != QProcess::NotRunning)
        ::kill(-static_cast<pid_t>(proceso.processId()), SIGTERM);
}

// rsync separa las actualizaciones de progreso con \r y el resto con \n
void Respaldo::leerSalida()
{
    pendiente += proceso.readAll();
    qsizetype inicio = 0;
    for (qsizetype i = 0; i < pendiente.size(); ++i) {
        if (pendiente.at(i) == '\r' || pendiente.at(i) == '\n') {
            procesarLinea(QString::fromUtf8(pendiente.mid(inicio, i - inicio)).trimmed());
            inicio = i + 1;
        }
    }
    pendiente.remove(0, inicio);
}

void Respaldo::procesarLinea(const QString &linea)
{
    // «2.00MB  99%  2.00GB/s ...»: anclada, porque un archivo puede llamarse «99% seguro.txt»
    static const QRegularExpression progreso(QStringLiteral("^\\S+\\s+(\\d+)%\\s"));
    if (linea.isEmpty())
        return;
    if (simulando && resumen.procesar(linea))
        return;

    const QRegularExpressionMatch coincidencia = progreso.match(linea);
    if (coincidencia.hasMatch()) {
        ponerProgreso(coincidencia.captured(1).toInt(), archivo_);
    } else if (linea.startsWith(herramientas->dbclient())) {
        // dbclient empieza sus mensajes con su ruta, que es larguísima
        const QString mensaje = linea.mid(herramientas->dbclient().size() + 1).trimmed();
        if (!mensaje.isEmpty()) {
            errorConexion = mensaje;
            anotar(mensaje);
        }
    } else if (linea.startsWith(QStringLiteral("IO error"))) {
        anotar(linea);
    } else if (linea.startsWith(QStringLiteral("rsync"))) {
        if (linea.contains(QStringLiteral("(0 bytes received so far)")))
            sinDatos = true;
        anotar(linea);
    } else if (simulando) {
        // El resto son las estadísticas de rsync, que no son nombres de archivo
    } else if (linea != QStringLiteral("sending incremental file list") && !linea.startsWith(QStringLiteral("sent "))
               && !linea.startsWith(QStringLiteral("total size")) && linea != QStringLiteral("./")) {
        ponerProgreso(progreso_, linea);    // la página lo recorta por el medio
    }
}

void Respaldo::terminoProceso(int codigo, QProcess::ExitStatus estado)
{
    leerSalida();
    ponerTrabajando(false);
    archivo_.clear();

    // Tras varias contraseñas malas OpenSSH corta con «Disconnect received», antes de que llegue
    // ningún dato de rsync. Si corta en medio de la copia, en cambio, no es por la contraseña.
    const bool huellaDistinta = errorConexion.contains(QStringLiteral("Host key verification"))
                                || errorConexion.contains(QStringLiteral("host key"), Qt::CaseInsensitive);
    const bool contrasenaMala = errorConexion.contains(QStringLiteral("auth"), Qt::CaseInsensitive)
                             || errorConexion.contains(QStringLiteral("password"), Qt::CaseInsensitive)
                             || (sinDatos && errorConexion.contains(QStringLiteral("Disconnect received")));
    if (cancelado || estado == QProcess::CrashExit) {
        ponerProgreso(-1, {});
        ponerEstado(tr("Respaldo cancelado."));
    } else if (codigo == 0) {
        if (!simulando)
            anotarRespaldoHecho();
        ponerProgreso(100, {});
        ponerEstado(simulando ? tr("Simulación completada: no se cambió nada.") : tr("Respaldo completado."), Exito);
        if (simulando)
            mostrarResumen();
    } else if (codigo == 23 || codigo == 24) {
        if (!simulando)
            anotarRespaldoHecho();
        // Archivos que no se pudieron leer o que desaparecieron durante la copia
        ponerProgreso(100, {});
        ponerEstado(simulando ? tr("Simulación completada, pero algunos archivos no se podrían copiar (ver los avisos).")
                              : tr("Terminó, pero algunos archivos no se pudieron copiar (ver los avisos)."), Error);
        if (simulando)
            mostrarResumen();
    } else if (huellaDistinta) {
        ponerProgreso(-1, {});
        ponerEstado(tr("Del otro lado contestó un equipo que no es el servidor (la huella no coincide). No se mandó la "
                       "contraseña. Si el servidor se reinstaló, hace falta una versión nueva del programa."), Error);
    } else if (contrasenaMala) {
        ponerProgreso(-1, {});
        ponerEstado(tr("Contraseña incorrecta, volvé a escribirla."), Error);
        emit enfocar(QStringLiteral("contrasena"));
    } else {
        ponerProgreso(-1, {});
        ponerEstado(tr("rsync terminó con errores (código %1). Revisá la conexión con el servidor y volvé a intentarlo.").arg(codigo), Error);
    }
}

void Respaldo::mostrarResumen()
{
    anotar({});
    anotar(tr("Se copiarían %n archivo(s)", nullptr, int(resumen.archivosACopiar()))
           + (resumen.tamanoACopiar().isEmpty() ? QString() : tr(" (%1).").arg(resumen.tamanoACopiar())));

    if (resumen.borradosOmitidos()) {
        // Con errores de lectura, rsync no borra nada: la lista de abajo no sería la de una copia sin errores
        anotar(tr("No se sabe qué se borraría: por los errores de lectura, rsync no evaluó los borrados."));
    } else if (resumen.archivosABorrar() == 0) {
        anotar(tr("No se borraría ningún archivo del respaldo."));
    } else {
        anotar(tr("Se borrarían %n archivo(s) del respaldo:", nullptr, int(resumen.archivosABorrar())));
        const QStringList lista = resumen.primerosABorrar();
        for (const QString &ruta : lista)
            anotar(QStringLiteral("  ") + ruta);
        if (resumen.archivosABorrar() > lista.size())
            anotar(tr("  …y %1 más.").arg(resumen.archivosABorrar() - lista.size()));
    }
}

// Después de un respaldo (completo o con algunos archivos que no se pudieron leer): se guarda la
// fecha y se programa el recordatorio para dentro de DiasRecordatorio días
void Respaldo::anotarRespaldoHecho()
{
    const QDateTime ahora = QDateTime::currentDateTime();
    ajustes().setValue(ClaveUltimoRespaldo, ahora);
    Plataforma::programarRecordatorio(ahora.toMSecsSinceEpoch(), DiasRecordatorio);
    emit ultimoCambio();
}

// «Último respaldo: hace 3 días» (o nunca)
QString Respaldo::textoUltimoRespaldo() const
{
    const QDateTime ultimo = ajustes().value(ClaveUltimoRespaldo).toDateTime();
    if (!ultimo.isValid())
        return tr("Todavía no hiciste ningún respaldo desde esta app.");
    const qint64 dias = ultimo.daysTo(QDateTime::currentDateTime());
    if (dias == 0)
        return tr("Último respaldo: hoy.");
    if (dias == 1)
        return tr("Último respaldo: ayer.");
    return tr("Último respaldo: hace %1 días.").arg(dias);
}
