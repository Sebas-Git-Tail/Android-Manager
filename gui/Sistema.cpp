#include "Sistema.h"

#include "Descargas.h"
#include "Plataforma.h"
#include "Respaldo.h"

#include <QGuiApplication>

Sistema::Sistema(Descargas *videos, Descargas *musica, Respaldo *respaldo, QObject *parent)
    : QObject(parent), videos(videos), musica(musica), respaldo(respaldo), pagina(Plataforma::paginaPedida())
{
    notificacion.setSingleShot(true);
    notificacion.setInterval(1000);
    connect(&notificacion, &QTimer::timeout, this, &Sistema::mostrarProgreso);
    const auto cambioProgreso = [this] {
        if (servicioActivo && !notificacion.isActive())
            notificacion.start();
    };
    for (Descargas *d : {videos, musica}) {
        connect(d, &Descargas::ocupadaCambio, this, [this, d] {
            actualizarServicio();
            if (!d->ocupada())
                tareaTerminada(d);
        });
        connect(d, &Descargas::progresoCambio, this, cambioProgreso);
        connect(d, &Descargas::estadoCambio, this, cambioProgreso);
    }
    connect(respaldo, &Respaldo::ocupadaCambio, this, [this, respaldo] {
        actualizarServicio();
        if (!respaldo->ocupada())
            tareaTerminada(respaldo);
    });
    connect(respaldo, &Respaldo::progresoCambio, this, cambioProgreso);
    connect(respaldo, &Respaldo::estadoCambio, this, cambioProgreso);

    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState estado) {
        emit aLaVistaCambio();
        if (estado != Qt::ApplicationActive)
            return;
        actualizarAcceso();     // al volver de Ajustes, el permiso puede haber cambiado
        if (Plataforma::servicioVencido()) {
            servicioActivo = false;
            if (hayTarea())
                emit avisar(tr("Android frenó la tarea en segundo plano (límite de horas por día). Mientras la app esté abierta, sigue."));
        }
        // Si Android no dejó empezar el servicio (la app estaba en segundo plano), se prueba de nuevo ahora
        if (hayTarea() && !servicioActivo)
            iniciarServicio();
    });
    acceso = Plataforma::tieneAccesoArchivos();
}

QString Sistema::version() const
{
    return QCoreApplication::applicationVersion();
}

bool Sistema::aLaVista() const
{
    return QGuiApplication::applicationState() == Qt::ApplicationActive;
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

// Antes esperaba hasta 3 s por cada tarea con la app cerrándose: Android la daba por colgada
void Sistema::cancelarTodo()
{
    videos->cortarAlSalir();
    musica->cortarAlSalir();
    respaldo->cortarAlSalir();
}

// Una tarea sigue aunque se vuelva a la portada o se cambie de app: el servicio en primer plano
// (con su notificación) evita que Android cierre la app mientras tanto
void Sistema::actualizarServicio()
{
    emit tareaCambio();
    const bool tarea = hayTarea();
    if (tarea == hayTareaAntes)
        return;
    hayTareaAntes = tarea;
    if (tarea) {
        if (!notificacionesPedidas) {
            notificacionesPedidas = true;
            Plataforma::pedirNotificaciones();
        }
        iniciarServicio();
    } else {
        notificacion.stop();
        servicioActivo = false;
        Plataforma::detenerServicio();
    }
}

void Sistema::iniciarServicio()
{
    const QString error = Plataforma::iniciarServicio(respaldo->ocupada() ? tr("Respaldo en curso") : tr("Descarga en curso"));
    servicioActivo = error.isEmpty();
    if (servicioActivo)
        mostrarProgreso();
    else
        emit avisar(tr("Android no dejó seguir la tarea en segundo plano: dejá la app abierta hasta que termine."));
}

// La notificación dice qué se está haciendo y cuánto falta del archivo actual
void Sistema::mostrarProgreso()
{
    if (!servicioActivo)
        return;
    if (respaldo->ocupada()) {
        Plataforma::actualizarServicio(respaldo->restaurando() ? tr("Restaurando") : tr("Respaldo"),
                                       respaldo->estado(), respaldo->progreso());
        return;
    }
    for (Descargas *d : {videos, musica}) {
        if (!d->ocupada())
            continue;
        QString texto = d->estado();
        if (d->total() > 1)
            texto = tr("%1 de %2 · %3").arg(qMin(d->hechas() + 1, d->total())).arg(d->total()).arg(texto);
        Plataforma::actualizarServicio(d->musica() ? tr("Música") : tr("Videos"), texto, d->progresoArchivo());
        return;
    }
}

// Si la app no está a la vista, queda un aviso con cómo terminó (el texto de estado ya está puesto un
// instante después de que deja de estar ocupada)
void Sistema::tareaTerminada(QObject *tarea)
{
    QTimer::singleShot(0, this, [this, tarea] {
        if (QGuiApplication::applicationState() == Qt::ApplicationActive)
            return;
        QString titulo, estado;
        int tipo = 0;    // 0 info (cancelada), 1 bien, 2 mal
        int id = 2;
        if (tarea == respaldo) {
            titulo = tr("Respaldo");
            estado = respaldo->estado();
            tipo = respaldo->tipoEstado();
            id = 4;
        } else if (auto *d = qobject_cast<Descargas *>(tarea)) {
            titulo = d->musica() ? tr("Música") : tr("Videos");
            estado = d->estado();
            tipo = d->tipoEstado();
            id = d->musica() ? 3 : 2;
        }
        if (estado.isEmpty())
            return;
        if (tipo == 1)
            titulo = tr("%1: terminó").arg(titulo);
        else if (tipo == 2)
            titulo = tr("%1: falló").arg(titulo);
        Plataforma::avisarFinTarea(id, titulo, estado);
    });
}
