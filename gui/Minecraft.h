// «Minecraft» (solo Sebastián), sin interfaz (las páginas son PaginaMinecraft.qml y
// PaginaServidorMinecraft.qml): lo mismo que el Panel de Minecraft del Manager de Linux, pero desde el
// celular, y además prender y apagar la PC.
//
// Todo pasa por el servidor con la clave limitada de la sección Servidor (tiene que estar activada y
// autorizada): acciones-servidor.sh acepta «pc-estado», «pc-prender» (Wake-on-LAN desde la red de casa,
// así anda aunque el celular esté afuera), «pc-apagar» (solo si la prendió un WOL y nadie la usa) y
// «minecraft <base64>», que corre Logica/Minecraft.sh de Arch-Linux-Manager en la PC. Los eventos que
// vuelven son los mismos que lee la ventana de Linux (SERVIDOR, PLUGIN, ETAPA, LINEA, JUGADORES…).

#pragma once

#include "ComandoServidor.h"

#include <QStringList>
#include <QTimer>
#include <QVariantList>

class Servidor;

class Minecraft : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool activo READ activo NOTIFY activoCambio)              // la sección Servidor está activada
    Q_PROPERTY(int pc READ pc NOTIFY pcCambio)
    Q_PROPERTY(bool pcApagable READ pcApagable NOTIFY pcCambio)
    Q_PROPERTY(QVariantList servidores READ servidores NOTIFY servidoresCambio)
    Q_PROPERTY(bool cargando READ cargando NOTIFY servidoresCambio)
    Q_PROPERTY(bool ocupado READ ocupado NOTIFY ocupadoCambio)
    Q_PROPERTY(QString accionActual READ accionActual NOTIFY ocupadoCambio)
    Q_PROPERTY(bool cancelable READ cancelable NOTIFY ocupadoCambio)
    Q_PROPERTY(QString estado READ estado NOTIFY estadoCambio)
    Q_PROPERTY(int tipoEstado READ tipoEstado NOTIFY estadoCambio)     // 0 info, 1 éxito, 2 error
    Q_PROPERTY(int progreso READ progreso NOTIFY estadoCambio)         // -1 sin barra
    Q_PROPERTY(QString avisos READ avisos NOTIFY avisosCambio)
    Q_PROPERTY(QString consolaDe READ consolaDe NOTIFY consolaCambio)
    Q_PROPERTY(int consola READ consola NOTIFY consolaCambio)
    Q_PROPERTY(QString jugadores READ jugadores NOTIFY jugadoresCambio)
    Q_PROPERTY(int conectados READ conectados NOTIFY jugadoresCambio)
    Q_PROPERTY(bool enviando READ enviando NOTIFY enviandoCambio)
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsCambio)
    Q_PROPERTY(QString pluginsDe READ pluginsDe NOTIFY pluginsCambio)
    Q_PROPERTY(bool buscandoPlugins READ buscandoPlugins NOTIFY pluginsCambio)
    Q_PROPERTY(int pluginsNuevos READ pluginsNuevos NOTIFY pluginsCambio)

public:
    enum EstadoPc { PcSinSaber, PcConsultando, PcApagada, PcEncendida, PcPrendiendo, PcApagando };
    Q_ENUM(EstadoPc)
    enum Consola { ConsolaNada, ConsolaApagado, ConsolaSoloLectura, ConsolaComandos };
    Q_ENUM(Consola)

    Minecraft(Herramientas *herramientas, Servidor *servidor, QObject *parent = nullptr);

    bool activo() const;
    int pc() const { return pc_; }
    bool pcApagable() const { return pcApagable_; }
    QVariantList servidores() const { return servidores_; }
    bool cargando() const { return consulta.ocupado(); }
    bool ocupado() const { return accion.ocupado(); }
    QString accionActual() const { return accionActual_; }
    bool cancelable() const { return cancelable_; }
    QString estado() const { return estado_; }
    int tipoEstado() const { return tipoEstado_; }
    int progreso() const { return progreso_; }
    QString avisos() const { return avisos_.join(u'\n'); }
    QString consolaDe() const { return consolaDe_; }
    int consola() const { return consola_; }
    QString jugadores() const { return jugadores_; }
    int conectados() const { return conectados_; }
    bool enviando() const { return envio.ocupado(); }
    QVariantList plugins() const { return plugins_; }
    QString pluginsDe() const { return pluginsDe_; }
    bool buscandoPlugins() const { return buscador.ocupado(); }
    int pluginsNuevos() const { return pluginsNuevos_; }

    // PC
    Q_INVOKABLE void consultarPc();         // y, si está prendida, los servidores
    Q_INVOKABLE void prenderPc();
    Q_INVOKABLE void apagarPc();
    // Servidores (como en Linux; «segundos» > 0: avisa en el chat y espera)
    Q_INVOKABLE void consultarServidores();
    Q_INVOKABLE void iniciar(const QString &servidor);
    Q_INVOKABLE void detener(const QString &servidor, int segundos);
    Q_INVOKABLE void reiniciar(const QString &servidor, int segundos);
    Q_INVOKABLE void actualizarPaper(const QStringList &servidores);
    Q_INVOKABLE void cambiarRam(const QString &servidor, int gb);
    Q_INVOKABLE void reiniciarMundo(const QString &servidor);
    Q_INVOKABLE void consultarPlugins(const QString &servidor);
    Q_INVOKABLE void actualizarPlugins(const QString &servidor);
    Q_INVOKABLE void instalarPlugin(const QString &texto, const QStringList &servidores);
    Q_INVOKABLE void cancelar();
    Q_INVOKABLE void limpiarAvisos();
    // Consola del servidor que se está mirando
    Q_INVOKABLE void verConsola(const QString &servidor);
    Q_INVOKABLE void dejarConsola();
    Q_INVOKABLE void enviarComando(const QString &texto);
    Q_INVOKABLE bool encendido(const QString &servidor) const;
    Q_INVOKABLE QString etiqueta(const QString &servidor) const;   // «26.1.2 SMP-1 (Amigos)»

    void cancelarTodo();

signals:
    void activoCambio();
    void pcCambio();
    void servidoresCambio();
    void ocupadoCambio();
    void estadoCambio();
    void avisosCambio();
    void consolaCambio();
    void lineaConsola(const QString &texto);    // una línea nueva del registro del servidor
    void consolaVaciada();
    void jugadoresCambio();
    void enviandoCambio();
    void pluginsCambio();
    void avisar(const QString &texto);

private:
    QString pedidoMinecraft(const QStringList &argumentos) const;
    void ejecutar(const QString &pedido, const QString &texto, bool esPc = false);
    void ejecutarMinecraft(const QStringList &argumentos, const QString &texto);
    void lineaAccion(const QString &texto);
    void terminoAccion(ComandoServidor::Resultado resultado, int codigo);
    void lineaConsulta(const QString &texto);
    void lineaRegistro(const QString &texto);
    void consultarJugadores();
    void ponerEstado(const QString &texto, int tipo = 0, int progreso = -1);
    void ponerPc(EstadoPc estado, bool apagable);

    Herramientas *herramientas;
    Servidor *servidor;
    ComandoServidor accion;      // una a la vez: prender/apagar la PC, iniciar, Paper, plugins…
    ComandoServidor consulta;    // estado de la PC y lista de servidores
    ComandoServidor registro;    // consola en vivo (tail -F en la PC)
    ComandoServidor conectados_proc;
    ComandoServidor envio;       // comandos para la consola
    ComandoServidor buscador;    // plugins (consulta Modrinth en la PC)
    QTimer temporizadorJugadores;
    QTimer temporizadorPc;       // después de prender/apagar, vuelve a mirar

    EstadoPc pc_ = PcSinSaber;
    bool pcApagable_ = false;
    QVariantList servidores_;
    QVariantList servidoresNuevos;
    QString accionActual_;
    bool cancelable_ = false;
    bool accionEsPc = false;
    QString estado_;
    int tipoEstado_ = 0;
    int progreso_ = -1;
    QString mensajeFinal;
    QStringList avisos_;
    QString consolaDe_;
    int consola_ = ConsolaNada;
    QString jugadores_;
    int conectados_ = 0;
    QVariantList plugins_;
    QVariantList pluginsNuevosLista;
    QString pluginsDe_;
    int pluginsNuevos_ = 0;
    int contadorPc = 0;
};
