#include "VentanaPrincipal.h"
#include "ui_VentanaPrincipal.h"

#include "PanelApps.h"
#include "ConfigServidor.h"
#include "PaginaDescargas.h"
#include "PaginaRespaldo.h"
#include "Plataforma.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QTimer>
#include <QUrl>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPixmap>
#include <QWindow>

VentanaPrincipal::VentanaPrincipal(QWidget *parent)
    : QWidget(parent), ui(new Ui::VentanaPrincipal),
      videos(new PaginaDescargas(PaginaDescargas::Tipo::Videos, &herramientas, this)),
      musica(new PaginaDescargas(PaginaDescargas::Tipo::Musica, &herramientas, this)),
      respaldo(new PaginaRespaldo(&herramientas, this))
{
    ui->setupUi(this);

    QFont fuente = ui->etiquetaTitulo->font();
    fuente.setPointSizeF(fuente.pointSizeF() * 1.4);
    fuente.setBold(true);
    ui->etiquetaTitulo->setFont(fuente);
    ui->etiquetaLogo->setPixmap(QPixmap(QStringLiteral(":/Logo.png")));
    ui->botonVolver->hide();

    ui->paginas->addWidget(videos);
    ui->paginas->addWidget(musica);
    ui->paginas->addWidget(respaldo);
    // «Apps»: actualizar Android-Manager e instalar las otras apps (comun/apps)
    auto *paginaApps = new apps::PanelApps(QStringLiteral("Android-Manager"), this);
    ui->paginas->addWidget(paginaApps);
    connect(ui->botonApps, &QPushButton::clicked, this, [this, paginaApps] { mostrar(paginaApps); });

    connect(ui->botonVideos, &QPushButton::clicked, this, [this] { mostrar(videos); });
    connect(ui->botonMusica, &QPushButton::clicked, this, [this] { mostrar(musica); });
    connect(ui->botonRespaldo, &QPushButton::clicked, this, [this] { mostrar(respaldo); });
    connect(ui->botonVolver, &QPushButton::clicked, this, &VentanaPrincipal::volver);
    connect(ui->botonAcceso, &QPushButton::clicked, this, [] { Plataforma::pedirAccesoArchivos(); });

    connect(videos, &PaginaDescargas::ocupacionCambiada, this, &VentanaPrincipal::actualizarServicio);
    connect(musica, &PaginaDescargas::ocupacionCambiada, this, &VentanaPrincipal::actualizarServicio);
    connect(respaldo, &PaginaRespaldo::ocupacionCambiada, this, &VentanaPrincipal::actualizarServicio);

    // Al volver de Ajustes, el permiso puede haber cambiado
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState estado) {
        if (estado == Qt::ApplicationActive)
            actualizarAcceso();
    });

    connect(&herramientas, &Herramientas::preparacionTerminada, this, [this](bool bien, const QString &error) {
        ui->etiquetaEstado->setText(bien ? QString() : QStringLiteral("× ") + error);
        ui->grupoHerramientas->setEnabled(bien);
    });
    ui->grupoHerramientas->setEnabled(false);
    ui->etiquetaEstado->setText(tr("Preparando las herramientas (solo tarda la primera vez)…"));
    actualizarAcceso();
    herramientas.preparar();

    // Datos del servidor (IPs, carpeta de respaldos) desde servidor.json, una vez por día
    ConfigServidor::instancia().actualizar();

    // ¿Hay una versión nueva? (una vez por día): pregunta, la baja y abre el instalador de Android
    apps::avisarActualizaciones(this, QStringLiteral("Android-Manager"));

    // Abierta desde la notificación del recordatorio: directo a Respaldo
    if (Plataforma::paginaPedida() == u"respaldo")
        QTimer::singleShot(0, this, [this] { mostrar(respaldo); });
}

VentanaPrincipal::~VentanaPrincipal()
{
    delete ui;
}

void VentanaPrincipal::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    respetarBarrasDelSistema();
}

// La app se dibuja detrás de las barras del sistema: el contenido se corre para no quedar tapado
void VentanaPrincipal::respetarBarrasDelSistema()
{
    QWindow *ventana = windowHandle();
    if (!ventana)
        return;
    connect(ventana, &QWindow::safeAreaMarginsChanged, this, &VentanaPrincipal::aplicarMargenes, Qt::UniqueConnection);
    aplicarMargenes(ventana->safeAreaMargins());
}

void VentanaPrincipal::aplicarMargenes(const QMargins &margenes)
{
    layout()->setContentsMargins(margenes.left() + 8, margenes.top() + 8, margenes.right() + 8, margenes.bottom() + 8);
}

void VentanaPrincipal::actualizarAcceso()
{
    const bool acceso = Plataforma::tieneAccesoArchivos();
    ui->grupoAcceso->setVisible(!acceso);
}

void VentanaPrincipal::mostrar(QWidget *pagina)
{
    ui->paginas->setCurrentWidget(pagina);
    ui->etiquetaTitulo->setText(pagina->windowTitle());
    ui->botonVolver->show();
}

void VentanaPrincipal::volver()
{
    ui->paginas->setCurrentWidget(ui->paginaPortada);
    ui->etiquetaTitulo->setText(windowTitle());
    ui->botonVolver->hide();
}

void VentanaPrincipal::keyReleaseEvent(QKeyEvent *evento)
{
    if (evento->key() == Qt::Key_Back && ui->paginas->currentWidget() != ui->paginaPortada) {
        evento->accept();
        volver();
        return;
    }
    QWidget::keyReleaseEvent(evento);
}

bool VentanaPrincipal::hayTarea() const
{
    return videos->ocupada() || musica->ocupada() || respaldo->ocupada();
}

// Una tarea sigue aunque se vuelva a la portada o se cambie de app; al cerrar se pregunta
void VentanaPrincipal::closeEvent(QCloseEvent *evento)
{
    if (hayTarea()) {
        QMessageBox caja(QMessageBox::Question, windowTitle(), tr("Hay una descarga o un respaldo en curso. ¿Cancelarlo y salir?"), QMessageBox::NoButton, this);
        const QPushButton *salir = caja.addButton(tr("Cancelar y salir"), QMessageBox::AcceptRole);
        caja.addButton(tr("Seguir"), QMessageBox::RejectRole);
        caja.exec();
        if (caja.clickedButton() != salir) {
            evento->ignore();
            return;
        }
        videos->cancelarYEsperar();
        musica->cancelarYEsperar();
        respaldo->cancelarYEsperar();
    }
    evento->accept();
}

void VentanaPrincipal::actualizarServicio()
{
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
