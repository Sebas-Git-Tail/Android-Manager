// Lo que la interfaz QML necesita del celular y de la app en general: el permiso de acceso a los
// archivos, el servicio en primer plano mientras hay una tarea y la salida con una tarea en curso.

#pragma once

#include <QObject>

class Descargas;
class Respaldo;

class Sistema : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool accesoArchivos READ accesoArchivos NOTIFY accesoCambio)
    Q_PROPERTY(bool hayTarea READ hayTarea NOTIFY tareaCambio)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString paginaPedida READ paginaPedida CONSTANT)   // «respaldo» si la abrió el recordatorio

public:
    Sistema(Descargas *videos, Descargas *musica, Respaldo *respaldo, QObject *parent = nullptr);

    bool accesoArchivos() const { return acceso; }
    bool hayTarea() const;
    QString version() const;
    QString paginaPedida() const { return pagina; }

    Q_INVOKABLE void pedirAcceso();          // abre la pantalla de Ajustes de esta app
    Q_INVOKABLE void cancelarTodo();         // antes de salir con una tarea en curso

signals:
    void accesoCambio();
    void tareaCambio();

private:
    void actualizarAcceso();
    void actualizarServicio();

    Descargas *videos;
    Descargas *musica;
    Respaldo *respaldo;
    QString pagina;
    bool acceso = false;
    bool servicioActivo = false;
    bool notificacionesPedidas = false;
};
