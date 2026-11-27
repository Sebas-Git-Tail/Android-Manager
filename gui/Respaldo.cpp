#include "Respaldo.h"

#include "ConfigServidor.h"
#include "Plataforma.h"
#include "Rutas.h"

#include <QDateTime>
#include <QVariantMap>
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
    return carpetaPersona() + celular() + u'/';
}

QString Respaldo::carpetaPersona() const
{
    return carpetaRespaldos() + persona() + u'/';
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

void Respaldo::cortarAlSalir()
{
    cancelar();
    if (proceso.state() != QProcess::NotRunning) {
        ::kill(-static_cast<pid_t>(proceso.processId()), SIGKILL);
        proceso.waitForFinished(300);    // SIGKILL no se puede ignorar: alcanza con un instante
    }
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

// Lo que hace falta para cualquier conexión: los nombres de la carpeta y la contraseña
bool Respaldo::revisarNombres(const QString &contrasenaEscrita)
{
    if (trabajando)
        return false;
    if (!disponible()) {
        ponerEstado(tr("Esta versión de la app no incluye rsync ni el cliente SSH."), Error);
        return false;
    }
    if (persona().isEmpty()) {
        ponerEstado(tr("Escribí de quién es el celular: una palabra, con letras, números, punto o guion."), Error);
        emit enfocar(QStringLiteral("persona"));
        return false;
    }
    if (celular().isEmpty()) {
        ponerEstado(tr("Escribí un nombre para el celular, sin barras y sin empezar con un punto."), Error);
        emit enfocar(QStringLiteral("celular"));
        return false;
    }
    if (contrasenaEscrita.isEmpty()) {
        ponerEstado(tr("Escribí la contraseña SSH del servidor."), Error);
        emit enfocar(QStringLiteral("contrasena"));
        return false;
    }
    return true;
}

void Respaldo::empezar(const QString &contrasenaEscrita)
{
    if (!revisarNombres(contrasenaEscrita))
        return;
    // Con --delete, un origen que se ve vacío borraría todo el respaldo del servidor
    if (!Plataforma::tieneAccesoArchivos()
        || QDir(Origen).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        ponerEstado(tr("La app no puede leer el almacenamiento: falta dar acceso a los archivos (botón en la portada)."), Error);
        return;
    }
    ponerEstado({});
    modo = Modo::Respaldar;
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
    if (trabajando || contrasena.isEmpty() || modo != Modo::Respaldar)
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
    anterioresMovidos = restaurados = 0;
    lineasListado.clear();
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
            ponerEstado(textoCancelado());
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
    QStringList argumentos;
    switch (modo) {
    case Modo::Respaldar:
        anotar(tr("%1 → %2:%3").arg(Origen, servidor, rutaEnServidor()));
        ponerEstado(simulando ? tr("Simulando el respaldo (no cambia nada)…") : tr("Respaldando…"));
        argumentos = argumentosRespaldo(servidor);
        break;
    case Modo::Listar:
        ponerProgreso(-1, {});
        ponerEstado(tr("Buscando los respaldos en el servidor…"));
        argumentos = argumentosListado(servidor);
        break;
    case Modo::Restaurar:
        ponerEstado(tr("Restaurando…"));
        argumentos = argumentosRestauracion(servidor);
        break;
    }

    // dbclient toma la contraseña de esta variable: no aparece en la lista de procesos
    QProcessEnvironment variables = herramientas->ambiente();
    variables.insert(QStringLiteral("DROPBEAR_PASSWORD"), contrasena);
    contrasena.clear();
    proceso.setProcessEnvironment(variables);
    proceso.start(herramientas->rsync(), argumentos);
    proceso.setProcessEnvironment(herramientas->ambiente());    // no queda guardada
}

QStringList Respaldo::argumentosRespaldo(const QString &servidor)
{
    versionNueva = analisisRsync::nombreVersion(QDateTime::currentDateTime());
    QStringList argumentos{QStringLiteral("-ah"), QStringLiteral("--info=progress2,name1,backup1"),
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
                           QStringLiteral("--delete"),
                           // Lo que --delete borra o la copia pisa va a <celular>.anteriores/<fecha>/ (fuera del
                           // respaldo, así --delete no lo toca)
                           QStringLiteral("--backup"), analisisRsync::argumentoBackupDir(celular(), QDateTime::currentDateTime())};
    // Y las versiones de más de 30 días se borran antes de empezar (en la misma conexión)
    const QString limpieza = analisisRsync::rsyncPathConLimpieza(carpetaPersona());
    if (!limpieza.isEmpty() && !simulando)
        argumentos << QStringLiteral("--rsync-path=") + limpieza;
    if (simulando) {
        argumentos << QStringLiteral("--dry-run") << QStringLiteral("--itemize-changes") << QStringLiteral("--stats")
                   << QStringLiteral("--info=del1");
    }
    argumentos << Origen << QStringLiteral("%1@%2:%3").arg(usuario(), servidor, rutaEnServidor());
    return argumentos;
}

// Solo las carpetas de primer nivel del respaldo y de cada versión de «anteriores» (no los archivos)
QStringList Respaldo::argumentosListado(const QString &servidor) const
{
    QStringList argumentos{QStringLiteral("--list-only"), QStringLiteral("-r"), QStringLiteral("-8"),
                           QStringLiteral("-e"), herramientas->dbclient(), QStringLiteral("--protect-args")};
    argumentos << analisisRsync::filtrosListado(celular());
    argumentos << QStringLiteral("%1@%2:%3").arg(usuario(), servidor, carpetaPersona());
    return argumentos;
}

// Del servidor al celular, sin --delete: no se borra nada del celular, y --update no pisa lo que acá sea
// más nuevo. Sin -a: en /sdcard no se pueden poner dueños ni permisos (daría «algunos archivos fallaron»)
QStringList Respaldo::argumentosRestauracion(const QString &servidor) const
{
    QString origen = restaurarVersion.isEmpty()
        ? rutaEnServidor()
        : carpetaPersona() + analisisRsync::carpetaAnteriores(celular()) + u'/' + restaurarVersion + u'/';
    QString destino = Origen;
    if (!restaurarCarpeta.isEmpty()) {
        origen += restaurarCarpeta + u'/';
        destino += restaurarCarpeta + u'/';
    }
    return {QStringLiteral("-rth"), QStringLiteral("--update"), QStringLiteral("--info=progress2,name1"), QStringLiteral("-8"),
            QStringLiteral("-e"), herramientas->dbclient(), QStringLiteral("--protect-args"),
            QStringLiteral("--exclude"), QStringLiteral("Android/obb"), QStringLiteral("--exclude"), QStringLiteral("Android/data"),
            QStringLiteral("%1@%2:%3").arg(usuario(), servidor, origen), destino};
}

void Respaldo::listar(const QString &contrasenaEscrita)
{
    if (!revisarNombres(contrasenaEscrita))
        return;
    modo = Modo::Listar;
    opciones = {};
    emit restauracionCambio();
    contrasena = contrasenaEscrita;
    simulando = false;
    continuar();
}

QVariantList Respaldo::versiones() const
{
    QVariantList lista;
    for (const QString &nombre : opciones.versiones)
        lista << QVariantMap{{QStringLiteral("nombre"), nombre}, {QStringLiteral("texto"), analisisRsync::textoVersion(nombre)}};
    return lista;
}

QStringList Respaldo::carpetas(const QString &version) const
{
    return version.isEmpty() ? opciones.carpetasActual : opciones.carpetasVersion.value(version);
}

QString Respaldo::descripcionRestauracion(const QString &version, const QString &carpeta) const
{
    const QString desde = version.isEmpty()
        ? tr("del respaldo de «%1»").arg(celular())
        : tr("de lo que el respaldo del %1 sacó o reemplazó (en «anteriores»)").arg(analisisRsync::textoVersion(version));
    const QString que = carpeta.isEmpty() ? tr("Todo lo que hay") : tr("La carpeta «%1»").arg(carpeta);
    return tr("%1 %2 vuelve al celular (%3).").arg(que, desde, Origen + carpeta);
}

void Respaldo::restaurar(const QString &contrasenaEscrita, const QString &version, const QString &carpeta)
{
    if (!revisarNombres(contrasenaEscrita))
        return;
    // Solo lo que se vio en el servidor (y nada que se salga de la carpeta)
    if (!opciones.hayRespaldo || (!version.isEmpty() && !opciones.versiones.contains(version))
        || (!carpeta.isEmpty() && !carpetas(version).contains(carpeta))
        || carpeta.contains(u'/') || carpeta == u".." || carpeta == u".") {
        ponerEstado(tr("Primero tocá «Ver qué hay en el servidor»."), Error);
        return;
    }
    if (!Plataforma::tieneAccesoArchivos()) {
        ponerEstado(tr("La app no puede escribir en el almacenamiento: falta dar acceso a los archivos (botón en la portada)."), Error);
        return;
    }
    if (!carpeta.isEmpty())
        QDir().mkpath(Origen + carpeta);
    modo = Modo::Restaurar;
    restaurarVersion = version;
    restaurarCarpeta = carpeta;
    contrasena = contrasenaEscrita;
    simulando = false;
    continuar();
}

QString Respaldo::textoCancelado() const
{
    switch (modo) {
    case Modo::Listar: return tr("Búsqueda cancelada.");
    case Modo::Restaurar: return tr("Restauración cancelada (lo que ya llegó al celular queda).");
    case Modo::Respaldar: break;
    }
    return tr("Respaldo cancelado.");
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
    if (linea.isEmpty())
        return;
    if (simulando && resumen.procesar(linea))
        return;

    const int porcentaje = analisisRsync::porcentaje(linea);   // «2.00MB  99%  2.00GB/s ...»
    if (porcentaje >= 0) {
        ponerProgreso(porcentaje, archivo_);
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
    } else if (modo == Modo::Listar) {
        lineasListado << linea;
    } else if (analisisRsync::esPasoAAnteriores(linea)) {
        ++anterioresMovidos;
    } else if (simulando) {
        // El resto son las estadísticas de rsync, que no son nombres de archivo
    } else if (!analisisRsync::esEncabezado(linea) && !linea.startsWith(QStringLiteral("(new) backup_dir"))
               && !linea.startsWith(QStringLiteral("created ")) && !linea.startsWith(QStringLiteral("deleting "))) {
        if (modo == Modo::Restaurar && !linea.endsWith(u'/'))
            ++restaurados;
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
    // «No such file or directory» al entrar a la carpeta de la persona: todavía no respaldó nunca
    const bool sinCarpeta = modo == Modo::Listar && codigo == 23
                            && avisos_.join(u'\n').contains(QStringLiteral("change_dir"));
    if (cancelado || estado == QProcess::CrashExit) {
        ponerProgreso(-1, {});
        ponerEstado(textoCancelado());
    } else if (modo == Modo::Listar && (codigo == 0 || sinCarpeta)) {
        terminoListado();
    } else if (modo == Modo::Restaurar && (codigo == 0 || codigo == 23 || codigo == 24)) {
        ponerProgreso(100, {});
        Plataforma::avisarArchivoNuevo(Origen + restaurarCarpeta);   // que la galería vuelva a mirar
        if (codigo == 0)
            ponerEstado(tr("Restauración completada: %n archivo(s) traído(s) al celular.", nullptr, restaurados), Exito);
        else
            ponerEstado(tr("Terminó, pero algunos archivos no se pudieron traer (ver los avisos)."), Error);
    } else if (modo == Modo::Respaldar && codigo == 0) {
        if (!simulando)
            anotarRespaldoHecho();
        ponerProgreso(100, {});
        ponerEstado(simulando ? tr("Simulación completada: no se cambió nada.") : tr("Respaldo completado."), Exito);
        if (simulando)
            mostrarResumen();
        else
            anotarAnteriores();
    } else if (modo == Modo::Respaldar && (codigo == 23 || codigo == 24)) {
        if (!simulando) {
            anotarRespaldoHecho();
            anotarAnteriores();
        }
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

// Cuántos archivos pasaron a «anteriores» en este respaldo (los que se borraron o cambiaron en el celular)
void Respaldo::anotarAnteriores()
{
    anotar({});
    if (anterioresMovidos == 0) {
        anotar(tr("No se borró ni se reemplazó nada del respaldo."));
        return;
    }
    anotar(tr("%n archivo(s) que estaban en el respaldo y se borraron o cambiaron en el celular quedaron en "
              "«anteriores» (%1) por 30 días.", nullptr, anterioresMovidos)
               .arg(analisisRsync::carpetaAnteriores(celular()) + u'/' + versionNueva));
}

void Respaldo::terminoListado()
{
    opciones = analisisRsync::opcionesRestauracion(lineasListado, celular());
    lineasListado.clear();
    ponerProgreso(-1, {});
    limpiarAvisos();
    if (!opciones.hayRespaldo)
        ponerEstado(tr("Todavía no hay un respaldo de «%1» en el servidor.").arg(celular()), Error);
    else
        ponerEstado(tr("Elegí qué traer al celular."), Exito);
    emit restauracionCambio();
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
