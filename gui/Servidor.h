// «Servidor», sin interfaz (la página es PaginaServidor.qml): le pide al servidor que corra una de sus
// acciones (miniaturas de Bodrio, páginas de descargas, páginas del Instituto), lo mismo que la sección
// Servidor del Manager de Linux.
//
// Solo para Sebastián: la sección no aparece hasta que se activa (dejando apretado el logo del menú).
// Al activarla, la app crea su propia clave SSH (dropbearkey, ed25519) en su carpeta privada y muestra
// la parte pública. En el servidor esa clave se autoriza con ~/Repo/Scripts/autorizar-equipo.sh, que la
// limita a acciones-servidor.sh (restrict, command=): con ella solo se pueden pedir esas acciones, ni
// siquiera abrir una terminal. Sin autorizar, el servidor la rechaza; nunca se usa una contraseña.

#pragma once

#include "Herramientas.h"

#include <QProcess>
#include <QStringList>

#include <functional>

class QTcpSocket;

class Servidor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool activado READ activado NOTIFY activadoCambio)
    Q_PROPERTY(bool disponible READ disponible CONSTANT)              // la app trae dbclient y dropbearkey
    Q_PROPERTY(QString clavePublica READ clavePublica NOTIFY activadoCambio)
    Q_PROPERTY(bool autorizado READ autorizado NOTIFY autorizadoCambio)    // la última vez el servidor la aceptó
    Q_PROPERTY(bool ocupado READ ocupado NOTIFY ocupadoCambio)
    Q_PROPERTY(QString accionActual READ accionActual NOTIFY ocupadoCambio)
    Q_PROPERTY(QString estado READ estado NOTIFY estadoCambio)
    Q_PROPERTY(int tipoEstado READ tipoEstado NOTIFY estadoCambio)    // 0 info, 1 éxito, 2 error
    Q_PROPERTY(QString salida READ salida NOTIFY salidaCambio)        // lo que fue diciendo el servidor

public:
    enum TipoEstado { Info, Exito, Error };
    Q_ENUM(TipoEstado)

    explicit Servidor(Herramientas *herramientas, QObject *parent = nullptr);

    bool activado() const;
    bool disponible() const;
    QString clavePublica() const { return clavePublica_; }
    QString archivoClave() const;            // la usa también Minecraft (la misma clave limitada)
    bool autorizado() const;
    bool ocupado() const { return !accion_.isEmpty(); }
    QString accionActual() const { return accion_; }
    QString estado() const { return estado_; }
    int tipoEstado() const { return tipoEstado_; }
    QString salida() const { return salida_.join(u'\n'); }

    Q_INVOKABLE void activar();                       // crea la clave (si no está) y muestra la sección
    Q_INVOKABLE void desactivar();                    // borra la clave: hay que volver a autorizar otra
    Q_INVOKABLE void copiarClave();
    Q_INVOKABLE void ejecutar(const QString &accion); // miniaturas-bodrio, descargas, indices-instituto
    Q_INVOKABLE void cancelar();

    void cortarAlSalir();    // al cerrar la app: corta lo que haya sin esperar

signals:
    void activadoCambio();
    void autorizadoCambio();
    void ocupadoCambio();
    void estadoCambio();
    void salidaCambio();
    void avisar(const QString &texto);

private:
    // dropbearkey corre aparte (sin frenar la interfaz); alTerminar se llama con clavePublica_ ya puesta
    void leerClavePublica(std::function<void()> alTerminar = {});
    void terminarActivacion();
    void probarServidor(int indice);
    void conectar(const QString &servidor);
    void terminado(int codigo, QProcess::ExitStatus estado);
    void ponerEstado(const QString &texto, TipoEstado tipo = Info);
    void terminar();

    Herramientas *herramientas;
    QProcess proceso;
    QTcpSocket *prueba = nullptr;
    QString accion_;
    QString clavePublica_;
    QString estado_;
    int tipoEstado_ = Info;
    QStringList salida_;
    QByteArray pendiente;
    QByteArray errores;
    bool cancelado = false;
    bool creandoClave = false;
};
