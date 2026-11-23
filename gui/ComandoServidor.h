// Un pedido al servidor con la clave limitada del celular (la misma de la sección Servidor): busca el
// servidor (primero la red de casa, después Tailscale), corre dbclient con el pedido y avisa cada línea
// que contesta. Lo que corre allá lo decide acciones-servidor.sh (command= en authorized_keys): el
// pedido es solo el nombre de la acción y sus datos. Lo usa Minecraft, que tiene varios a la vez
// (la acción en curso, la consola en vivo, quién está conectado y los comandos).

#pragma once

#include "Herramientas.h"

#include <QProcess>

class QTcpSocket;

class ComandoServidor : public QObject
{
    Q_OBJECT

public:
    enum Resultado { Bien, Fallo, Rechazada, HuellaDistinta, SinServidor, Cancelado };
    Q_ENUM(Resultado)

    ComandoServidor(Herramientas *herramientas, const QString &archivoClave, QObject *parent = nullptr);
    ~ComandoServidor() override;

    void iniciar(const QString &pedido);
    void cancelar();
    bool ocupado() const { return ocupado_; }
    QString pedido() const { return pedido_; }

    // Texto para mostrar cuando no salió bien (vacío si salió bien o se canceló)
    static QString explicar(Resultado resultado, int codigo);

signals:
    void linea(const QString &texto);
    void terminado(ComandoServidor::Resultado resultado, int codigo);

private:
    void probar(int indice);
    void conectar(const QString &servidor);
    void fin(Resultado resultado, int codigo = 0);

    Herramientas *herramientas;
    QString archivoClave;
    QProcess proceso;
    QTcpSocket *prueba = nullptr;
    QString pedido_;
    QByteArray pendiente;
    QByteArray errores;
    bool ocupado_ = false;
    bool cancelado = false;
    bool deCache = false;      // se conectó a la dirección de la vez anterior, sin probarla
};
