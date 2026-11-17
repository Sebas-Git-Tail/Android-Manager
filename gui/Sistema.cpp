#include "Sistema.h"

#include "Descargas.h"
#include "Plataforma.h"
#include "Respaldo.h"

#include <QGuiApplication>

Sistema::Sistema(Descargas *videos, Descargas *musica, Respaldo *respaldo, QObject *parent)
    : QObject(parent), videos(videos), musica(musica), respaldo(respaldo), pagina(Plataforma::paginaPedida())
{
    connect(videos, &Descargas::ocupadaCambio, this, &Sistema::actualizarServicio);
    connect(musica, &Descargas::ocupadaCambio, this, &Sistema::actualizarServicio);
    connect(respaldo, &Respaldo::ocupadaCambio, this, &Sistema::actualizarServicio);
    // Al volver de Ajustes, el permiso puede haber cambiado
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState estado) {
        if (estado == Qt::ApplicationActive)
            actualizarAcceso();
    });
    acceso = Plataforma::tieneAccesoArchivos();
}

QString Sistema::version() const
{
    return QCoreApplication::applicationVersion();
}

bool Sistema::hayTarea() const
{
    return videos->ocupada() || musica->ocupada() || respaldo->ocupada();
}

void Sistema::actualizarAcceso()
{
    const bool ahora = Plataforma::tieneAccesoArchivos();
    if (ahora == acceso)
        return;
    acceso = ahora;
    emit accesoCambio();
}

void Sistema::pedirAcceso()
{
    Plataforma::pedirAccesoArchivos();
}

void Sistema::cancelarTodo()
{
    videos->cancelarYEsperar();
    musica->cancelarYEsperar();
    respaldo->cancelarYEsperar();
}

// Una tarea sigue aunque se vuelva a la portada o se cambie de app: el servicio en primer plano
// (con su notificación) evita que Android cierre la app mientras tanto
void Sistema::actualizarServicio()
{
    emit tareaCambio();
    const bool tarea = hayTarea();
    if (tarea == servicioActivo)
        return;
    servicioActivo = tarea;
    if (tarea) {
        if (!notificacionesPedidas) {
            notificacionesPedidas = true;
            Plataforma::pedirNotificaciones();
        }
        Plataforma::iniciarServicio(respaldo->ocupada() ? tr("Respaldo en curso") : tr("Descarga en curso"));
    } else {
        Plataforma::detenerServicio();
    }
}
