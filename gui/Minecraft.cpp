#include "Minecraft.h"

#include "Servidor.h"

#include <QDate>
#include <QRegularExpression>
#include <QVariantMap>

namespace {

constexpr int MaxAvisos = 60;

// «PALABRA resto» → {PALABRA, resto}
std::pair<QString, QString> partir(const QString &linea)
{
    const qsizetype espacio = linea.indexOf(u' ');
    return {linea.left(espacio), espacio < 0 ? QString() : linea.mid(espacio + 1)};
}

} // namespace

// Los servidores se identifican por su ruta dentro de Servidores/ en la PC (<grupo>/<versión>/<servidor>,
// desde el Manager de Linux 4.13.0): se muestran como «26.1.2 SMP-1 (Amigos)»
QString Minecraft::etiqueta(const QString &servidor) const
{
    const qsizetype barra = servidor.indexOf(u'/');
    if (barra < 0)
        return servidor;
    return QStringLiteral("%1 (%2)").arg(servidor.section(u'/', -1), servidor.left(barra));
}

Minecraft::Minecraft(Herramientas *herramientas, Servidor *servidor, QObject *parent)
    : QObject(parent), herramientas(herramientas), servidor(servidor),
      accion(herramientas, servidor->archivoClave()), consulta(herramientas, servidor->archivoClave()),
      registro(herramientas, servidor->archivoClave()), conectados_proc(herramientas, servidor->archivoClave()),
      envio(herramientas, servidor->archivoClave()), buscador(herramientas, servidor->archivoClave())
{
    connect(servidor, &Servidor::activadoCambio, this, &Minecraft::activoCambio);

    connect(&accion, &ComandoServidor::linea, this, &Minecraft::lineaAccion);
    connect(&accion, &ComandoServidor::terminado, this, &Minecraft::terminoAccion);

    connect(&consulta, &ComandoServidor::linea, this, &Minecraft::lineaConsulta);
    connect(&consulta, &ComandoServidor::terminado, this, [this](ComandoServidor::Resultado resultado, int codigo) {
        const bool eraPc = consulta.pedido() == u"pc-estado";
        if (resultado != ComandoServidor::Bien) {
            if (resultado != ComandoServidor::Cancelado) {
                ponerEstado(eraPc || pc_ != PcEncendida ? ComandoServidor::explicar(resultado, codigo)
                                                        : tr("La PC está apagada o no contesta."), 2);
                if (eraPc && pc_ == PcConsultando)
                    ponerPc(PcSinSaber, false);
            }
            emit servidoresCambio();
            return;
        }
        if (eraPc) {
            if (pc_ == PcEncendida)
                consultarServidores();
            emit servidoresCambio();
            return;
        }
        servidores_ = servidoresNuevos;
        emit servidoresCambio();
        // Si el que se mira se prendió o se apagó, la consola vuelve a empezar (cambia si acepta comandos)
        if (!consolaDe_.isEmpty() && encendido(consolaDe_) != (consola_ == ConsolaComandos || consola_ == ConsolaSoloLectura)
            && consola_ != ConsolaNada)
            verConsola(consolaDe_);
    });

    connect(&registro, &ComandoServidor::linea, this, &Minecraft::lineaRegistro);
    connect(&registro, &ComandoServidor::terminado, this, [this](ComandoServidor::Resultado resultado, int) {
        // Se cortó solo (no lo cancelamos): se vuelve a conectar en unos segundos
        if (resultado != ComandoServidor::Cancelado && !consolaDe_.isEmpty()) {
            emit lineaConsola(tr("— se cortó la conexión con la consola; reintentando… —"));
            const QString servidor = consolaDe_;
            QTimer::singleShot(5000, this, [this, servidor] {
                if (consolaDe_ == servidor && !registro.ocupado())
                    registro.iniciar(pedidoMinecraft({QStringLiteral("registro"), servidor}));
            });
            return;
        }
        // Se canceló para mirar otro: arranca el nuevo
        if (!consolaDe_.isEmpty())
            registro.iniciar(pedidoMinecraft({QStringLiteral("registro"), consolaDe_}));
    });

    connect(&conectados_proc, &ComandoServidor::linea, this, [this](const QString &linea) {
        const auto [palabra, resto] = partir(linea);
        if (palabra == u"AVISO") {
            jugadores_ = resto;
            emit jugadoresCambio();
        } else if (palabra == u"JUGADORES") {
            // JUGADORES conectados<tab>máximo<tab>nombre,nombre…
            const QStringList c = resto.split(u'\t');
            if (c.size() < 3)
                return;
            conectados_ = c.at(0).toInt();
            const QStringList nombres = c.at(2).split(u',', Qt::SkipEmptyParts);
            if (conectados_ == 0) {
                jugadores_ = tr("Nadie conectado (entran hasta %1).").arg(c.at(1));
            } else {
                QString lista = nombres.join(QStringLiteral(", "));
                if (nombres.size() < conectados_)
                    lista += (lista.isEmpty() ? QString() : QStringLiteral(" ")) + tr("y %1 más").arg(conectados_ - nombres.size());
                jugadores_ = tr("Conectados (%1 de %2): %3").arg(conectados_).arg(c.at(1), lista);
            }
            emit jugadoresCambio();
        }
    });

    connect(&envio, &ComandoServidor::linea, this, [this](const QString &linea) {
        const auto [palabra, resto] = partir(linea);
        if (palabra == u"ERROR")
            ponerEstado(resto, 2);
    });
    connect(&envio, &ComandoServidor::terminado, this, [this](ComandoServidor::Resultado resultado, int codigo) {
        if (resultado != ComandoServidor::Bien && resultado != ComandoServidor::Fallo)
            ponerEstado(ComandoServidor::explicar(resultado, codigo), 2);
        emit enviandoCambio();
    });

    connect(&buscador, &ComandoServidor::linea, this, [this](const QString &linea) {
        const auto [palabra, resto] = partir(linea);
        if (palabra != u"PLUGIN")
            return;
        // PLUGIN archivo<tab>nombre<tab>versión<tab>nueva<tab>estado<tab>…
        const QStringList c = resto.split(u'\t');
        if (c.size() < 5)
            return;
        pluginsNuevosLista << QVariantMap{{QStringLiteral("archivo"), c.at(0)}, {QStringLiteral("nombre"), c.at(1)},
                                          {QStringLiteral("version"), c.at(2)}, {QStringLiteral("nueva"), c.at(3)},
                                          {QStringLiteral("estado"), c.at(4)}};
    });
    connect(&buscador, &ComandoServidor::terminado, this, [this](ComandoServidor::Resultado resultado, int codigo) {
        plugins_ = pluginsNuevosLista;
        pluginsNuevos_ = 0;
        for (const QVariant &p : std::as_const(plugins_))
            if (p.toMap().value(QStringLiteral("estado")).toString() == u"NUEVA")
                ++pluginsNuevos_;
        if (resultado != ComandoServidor::Bien && resultado != ComandoServidor::Cancelado)
            ponerEstado(ComandoServidor::explicar(resultado, codigo), 2);
        emit pluginsCambio();
    });

    temporizadorJugadores.setInterval(10000);
    connect(&temporizadorJugadores, &QTimer::timeout, this, &Minecraft::consultarJugadores);
    temporizadorPc.setSingleShot(true);
    connect(&temporizadorPc, &QTimer::timeout, this, &Minecraft::consultarPc);
}

bool Minecraft::activo() const
{
    return servidor->activado();
}

bool Minecraft::encendido(const QString &nombre) const
{
    for (const QVariant &s : servidores_) {
        const QVariantMap m = s.toMap();
        if (m.value(QStringLiteral("nombre")).toString() == nombre)
            return m.value(QStringLiteral("encendido")).toBool();
    }
    return false;
}

// Los argumentos viajan en base64, separados por saltos de línea: así pasan nombres con espacios y
// comandos con comillas sin que nadie los interprete (acciones-servidor.sh los decodifica)
QString Minecraft::pedidoMinecraft(const QStringList &argumentos) const
{
    return QStringLiteral("minecraft ") + QString::fromLatin1(argumentos.join(u'\n').toUtf8().toBase64());
}

// --- PC ----------------------------------------------------------------------------------------

void Minecraft::consultarPc()
{
    if (!activo() || consulta.ocupado() || accion.ocupado())
        return;
    ponerPc(PcConsultando, pcApagable_);
    consulta.iniciar(QStringLiteral("pc-estado"));
    emit servidoresCambio();
}

void Minecraft::prenderPc()
{
    if (!activo() || accion.ocupado())
        return;
    ponerPc(PcPrendiendo, false);
    ejecutar(QStringLiteral("pc-prender"), tr("Prendiendo la PC (tarda alrededor de medio minuto)…"), true);
}

void Minecraft::apagarPc()
{
    if (!activo() || accion.ocupado())
        return;
    ejecutar(QStringLiteral("pc-apagar"), tr("Apagando la PC…"), true);
}

void Minecraft::ponerPc(EstadoPc estado, bool apagable)
{
    pc_ = estado;
    pcApagable_ = apagable;
    if (estado == PcApagada) {
        servidores_.clear();
        emit servidoresCambio();
    }
    emit pcCambio();
}

// --- Servidores ---------------------------------------------------------------------------------

void Minecraft::consultarServidores()
{
    if (!activo() || consulta.ocupado())
        return;
    servidoresNuevos.clear();
    consulta.iniciar(pedidoMinecraft({QStringLiteral("servidores")}));
    emit servidoresCambio();
}

void Minecraft::lineaConsulta(const QString &linea)
{
    const auto [palabra, resto] = partir(linea);
    if (palabra == u"PC") {
        ponerPc(resto == u"ENCENDIDA" ? PcEncendida : PcApagada, false);
    } else if (palabra == u"APAGABLE") {
        pcApagable_ = resto == u"si";
        emit pcCambio();
    } else if (palabra == u"SERVIDOR") {
        // SERVIDOR estado<tab>nombre<tab>tipo<tab>versión<tab>tamaño<tab>último uso<tab>RAM<tab>jar
        const QStringList c = resto.split(u'\t');
        if (c.size() < 8)
            return;
        QString ultimo = c.at(5);
        const QDate fecha = QDate::fromString(ultimo, Qt::ISODate);
        if (fecha.isValid())
            ultimo = fecha == QDate::currentDate() ? tr("hoy") : fecha.toString(QStringLiteral("dd/MM/yyyy"));
        const QString &nombre = c.at(1);
        servidoresNuevos << QVariantMap{{QStringLiteral("nombre"), nombre},
                                        {QStringLiteral("titulo"), nombre.section(u'/', -1)},
                                        {QStringLiteral("grupo"), nombre.contains(u'/') ? nombre.section(u'/', 0, 0) : QString()},
                                        {QStringLiteral("encendido"), c.at(0) == u"ENCENDIDO"},
                                        // paper|forge|fabric|vanilla (Fabric y Vanilla desde Arch-Linux-Manager 4.14.0)
                                        {QStringLiteral("tipo"), c.at(2).isEmpty() ? QStringLiteral("Paper") : c.at(2).left(1).toUpper() + c.at(2).mid(1)},
                                        {QStringLiteral("version"), c.at(3)}, {QStringLiteral("mundo"), c.at(4)},
                                        {QStringLiteral("ultimo"), ultimo}, {QStringLiteral("ram"), c.at(6)},
                                        {QStringLiteral("jar"), c.at(7)}};
        if (pc_ != PcEncendida)
            ponerPc(PcEncendida, pcApagable_);
    } else if (palabra == u"ERROR") {
        ponerEstado(resto, 2);
        if (resto.contains(QStringLiteral("apagada")))
            ponerPc(PcApagada, false);
    }
}

void Minecraft::iniciar(const QString &servidor)
{
    ejecutarMinecraft({QStringLiteral("iniciar"), servidor}, tr("Iniciando %1…").arg(etiqueta(servidor)));
}

void Minecraft::detener(const QString &servidor, int segundos)
{
    cancelable_ = segundos > 0;
    ejecutarMinecraft({QStringLiteral("detener"), servidor, QString::number(qBound(0, segundos, 999))},
                      tr("Deteniendo %1…").arg(etiqueta(servidor)));
}

void Minecraft::reiniciar(const QString &servidor, int segundos)
{
    cancelable_ = segundos > 0;
    ejecutarMinecraft({QStringLiteral("reiniciar"), servidor, QString::number(qBound(0, segundos, 999))},
                      tr("Reiniciando %1…").arg(etiqueta(servidor)));
}

void Minecraft::actualizarPaper(const QStringList &servidores)
{
    if (servidores.isEmpty())
        return;
    ejecutarMinecraft(QStringList{QStringLiteral("paper")} + servidores, tr("Actualizando Paper…"));
}

void Minecraft::cambiarRam(const QString &servidor, int gb)
{
    ejecutarMinecraft({QStringLiteral("ram"), servidor, QString::number(qBound(1, gb, 24)) + u'G'}, tr("Cambiando la RAM…"));
}

void Minecraft::reiniciarMundo(const QString &servidor)
{
    ejecutarMinecraft({QStringLiteral("reiniciar-mundo"), servidor}, tr("Reiniciando el mundo de %1…").arg(etiqueta(servidor)));
}

void Minecraft::consultarPlugins(const QString &servidor)
{
    if (!activo() || servidor.isEmpty())
        return;
    if (buscador.ocupado())
        buscador.cancelar();
    pluginsDe_ = servidor;
    pluginsNuevosLista.clear();
    plugins_.clear();
    pluginsNuevos_ = 0;
    emit pluginsCambio();
    // Si estaba buscando los de otro, espera a que termine de cancelarse
    if (buscador.ocupado()) {
        QTimer::singleShot(300, this, [this, servidor] { if (pluginsDe_ == servidor) consultarPlugins(servidor); });
        return;
    }
    buscador.iniciar(pedidoMinecraft({QStringLiteral("plugins"), servidor}));
    emit pluginsCambio();
}

void Minecraft::actualizarPlugins(const QString &servidor)
{
    ejecutarMinecraft({QStringLiteral("plugins-actualizar"), servidor}, tr("Actualizando plugins de %1…").arg(etiqueta(servidor)));
}

void Minecraft::instalarPlugin(const QString &texto, const QStringList &servidores)
{
    const QString limpio = texto.trimmed();
    if (limpio.isEmpty() || servidores.isEmpty())
        return;
    ejecutarMinecraft(QStringList{QStringLiteral("plugin"), limpio} + servidores, tr("Instalando «%1»…").arg(limpio));
}

void Minecraft::ejecutarMinecraft(const QStringList &argumentos, const QString &texto)
{
    for (const QString &a : argumentos) {
        if (a.contains(u'\n') || a.contains(u'\r')) {
            cancelable_ = false;
            return;
        }
    }
    ejecutar(pedidoMinecraft(argumentos), texto);
}

void Minecraft::ejecutar(const QString &pedido, const QString &texto, bool esPc)
{
    if (!activo() || accion.ocupado()) {
        cancelable_ = false;
        return;
    }
    accionEsPc = esPc;
    accionActual_ = pedido.section(u' ', 0, 0) == u"minecraft"
        ? QString::fromUtf8(QByteArray::fromBase64(pedido.section(u' ', 1).toLatin1())).section(u'\n', 0, 0)
        : pedido;
    mensajeFinal.clear();
    ponerEstado(texto);
    accion.iniciar(pedido);
    emit ocupadoCambio();
}

void Minecraft::lineaAccion(const QString &linea)
{
    if (linea.isEmpty())
        return;
    const auto [palabra, resto] = partir(linea);
    if (palabra == u"ETAPA") {
        ponerEstado(resto + QStringLiteral("…"));
    } else if (palabra == u"ESPERA") {
        ponerEstado(estado_, 0, resto.toInt() * 100 / 120);    // detener: hasta 2 minutos guardando
    } else if (palabra == u"AVISO") {
        avisos_ << resto;
    } else if (palabra == u"LISTO" || palabra == u"ERROR") {
        mensajeFinal = resto;
    } else if (palabra == u"LATIDO") {
        return;
    } else {
        // prender-pc.sh y apagar-pc.sh contestan en texto común
        avisos_ << linea.trimmed();
        mensajeFinal = linea.trimmed().remove(QRegularExpression(QStringLiteral("^-\\s*")));
        ponerEstado(mensajeFinal);
    }
    while (avisos_.size() > MaxAvisos)
        avisos_.removeFirst();
    emit avisosCambio();
}

void Minecraft::terminoAccion(ComandoServidor::Resultado resultado, int codigo)
{
    const bool eraCancelable = cancelable_;
    const QString hecha = accion.pedido();
    if (resultado == ComandoServidor::Cancelado)
        ponerEstado(eraCancelable ? tr("Cancelado: el servidor sigue prendido (se avisó en el chat).") : tr("Cancelado."));
    else if (resultado == ComandoServidor::Bien)
        ponerEstado(mensajeFinal.isEmpty() ? tr("Listo.") : mensajeFinal, 1);
    else if (resultado == ComandoServidor::Fallo && !mensajeFinal.isEmpty())
        ponerEstado(mensajeFinal, 2);
    else
        ponerEstado(ComandoServidor::explicar(resultado, codigo), 2);
    if (!mensajeFinal.isEmpty() && resultado != ComandoServidor::Cancelado) {
        avisos_ << mensajeFinal;
        emit avisosCambio();
    }
    accionActual_.clear();
    cancelable_ = false;
    emit ocupadoCambio();

    if (accionEsPc) {
        if (hecha == u"pc-prender" && resultado == ComandoServidor::Bien) {
            ponerPc(PcEncendida, !mensajeFinal.contains(QStringLiteral("ya estaba")));
            consultarPc();
        } else if (hecha == u"pc-apagar" && resultado == ComandoServidor::Bien) {
            ponerPc(PcApagando, false);
            temporizadorPc.start(25000);
        } else {
            consultarPc();
        }
        return;
    }
    // El servidor tarda unos segundos en arrancar java: la lista se actualiza ahora y después
    consultarServidores();
    QTimer::singleShot(8000, this, &Minecraft::consultarServidores);
    const QString que = QString::fromUtf8(QByteArray::fromBase64(hecha.section(u' ', 1).toLatin1())).section(u'\n', 0, 0);
    if ((que == u"plugins-actualizar" || que == u"plugin") && !pluginsDe_.isEmpty())
        consultarPlugins(pluginsDe_);
}

void Minecraft::cancelar()
{
    accion.cancelar();
}

void Minecraft::limpiarAvisos()
{
    avisos_.clear();
    emit avisosCambio();
}

// --- Consola -----------------------------------------------------------------------------------

void Minecraft::verConsola(const QString &servidor)
{
    if (!activo())
        return;
    consolaDe_ = servidor;
    consola_ = ConsolaNada;
    conectados_ = 0;
    jugadores_ = encendido(servidor) ? tr("Consultando quién está conectado…") : tr("Apagado: se ve lo último que registró.");
    emit consolaVaciada();
    emit consolaCambio();
    emit jugadoresCambio();
    if (registro.ocupado())
        registro.cancelar();      // al terminar de cancelarse arranca el de este (ver el constructor)
    else
        registro.iniciar(pedidoMinecraft({QStringLiteral("registro"), servidor}));
    consultarJugadores();
    temporizadorJugadores.start();
}

void Minecraft::dejarConsola()
{
    consolaDe_.clear();
    consola_ = ConsolaNada;
    temporizadorJugadores.stop();
    registro.cancelar();
    conectados_proc.cancelar();
    emit consolaCambio();
}

void Minecraft::lineaRegistro(const QString &linea)
{
    const auto [palabra, resto] = partir(linea);
    if (palabra == u"LINEA") {
        emit lineaConsola(resto);
    } else if (palabra == u"CONSOLA") {
        consola_ = resto == u"COMANDOS" ? ConsolaComandos : resto == u"SOLO_LECTURA" ? ConsolaSoloLectura : ConsolaApagado;
        emit consolaCambio();
    } else if (palabra == u"ERROR") {
        emit lineaConsola(resto);
    }
}

void Minecraft::consultarJugadores()
{
    if (consolaDe_.isEmpty() || !encendido(consolaDe_) || conectados_proc.ocupado())
        return;
    conectados_proc.iniciar(pedidoMinecraft({QStringLiteral("jugadores"), consolaDe_}));
}

void Minecraft::enviarComando(const QString &texto)
{
    const QString limpio = texto.trimmed();
    if (limpio.isEmpty() || envio.ocupado() || consola_ != ConsolaComandos || limpio.contains(u'\n'))
        return;
    envio.iniciar(pedidoMinecraft({QStringLiteral("comando"), consolaDe_, limpio}));
    emit enviandoCambio();
}

void Minecraft::ponerEstado(const QString &texto, int tipo, int progreso)
{
    estado_ = texto;
    tipoEstado_ = tipo;
    progreso_ = progreso;
    emit estadoCambio();
}

void Minecraft::cancelarTodo()
{
    temporizadorJugadores.stop();
    temporizadorPc.stop();
    consolaDe_.clear();
    for (ComandoServidor *c : {&accion, &consulta, &registro, &conectados_proc, &envio, &buscador})
        c->cancelar();
}
