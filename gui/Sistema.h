// Lo que la interfaz QML necesita del celular y de la app en general: el permiso de acceso a los
// archivos, el servicio en primer plano mientras hay una tarea (con su notificación: cómo va y, al
// terminar, si salió bien) y la salida con una tarea en curso.

#pragma once

#include <QObject>
#include <QTimer>

class Descargas;
class Respaldo;

class Sistema : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool accesoArchivos READ accesoArchivos NOTIFY accesoCambio)
    Q_PROPERTY(bool hayTarea READ hayTarea NOTIFY tareaCambio)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString paginaPedida READ paginaPedida CONSTANT)   // «respaldo» si la abrió el recordatorio
    Q_PROPERTY(bool aLaVista READ aLaVista NOTIFY aLaVistaCambio)  // la app está en primer plano

public:
    Sistema(Descargas *videos, Descargas *musica, Respaldo *respaldo, QObject *parent = nullptr);

    bool accesoArchivos() const { return acceso; }
    bool hayTarea() const;
    QString version() const;
    QString paginaPedida() const { return pagina; }
    bool aLaVista() const;

    Q_INVOKABLE void pedirAcceso();          // abre la pantalla de Ajustes de esta app
    Q_INVOKABLE void cancelarTodo();         // antes de salir con una tarea en curso (corta sin esperar)

signals:
    void accesoCambio();
    void tareaCambio();
    void avisar(const QString &texto);       // mensajito abajo de la pantalla
    void aLaVistaCambio();

private:
    void actualizarAcceso();
    void actualizarServicio();
    void iniciarServicio();
    void mostrarProgreso();
    void tareaTerminada(QObject *tarea);

    Descargas *videos;
    Descargas *musica;
    Respaldo *respaldo;
    QString pagina;
    QTimer notificacion;             // la notificación se actualiza como mucho una vez por segundo
    bool acceso = false;
    bool servicioActivo = false;     // se pidió el servicio y Android lo aceptó
    bool hayTareaAntes = false;
    bool notificacionesPedidas = false;
};
