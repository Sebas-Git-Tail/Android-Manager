#include "PaginaDescargas.h"
#include "ui_PaginaDescargas.h"

#include "Plataforma.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScroller>
#include <QStandardPaths>
#include <QUrl>

#include <csignal>
#include <unistd.h>

constexpr int RolRuta = Qt::UserRole;

namespace {

const QString Almacenamiento = QStringLiteral("/sdcard");

} // namespace

PaginaDescargas::PaginaDescargas(Tipo tipo, Herramientas *herramientas, QWidget *parent)
    : QWidget(parent), ui(new Ui::PaginaDescargas), tipo(tipo), herramientas(herramientas)
{
    ui->setupUi(this);
    const bool musica = tipo == Tipo::Musica;

    if (musica) {
        setWindowTitle(tr("Descargar Música"));
        ui->grupoUrls->setTitle(tr("URLs de YouTube Music (una por línea)"));
        ui->editUrls->setPlaceholderText(QStringLiteral("https://music.youtube.com/watch?v=..."));
        ui->grupoDescargados->setTitle(tr("Canciones (en Music, una carpeta por artista)"));
        ui->botonVer->setText(tr("Reproducir"));
    } else {
        setWindowTitle(tr("Descargar Vídeos"));
    }
    ui->filaPlataforma->setVisible(!musica);
    ui->checkPlaylist->setVisible(musica);
    ui->botonYoutubeMusic->setVisible(musica);
    ui->editCarpeta->setText(Almacenamiento + QStringLiteral("/Movies"));
    ui->filaCarpeta->setVisible(false);
    ui->editCookies->setText(Almacenamiento + QStringLiteral("/Download/cookies.txt"));
    ui->editCookies->setEnabled(false);
    ui->barraArchivo->hide();
    ui->barraTotal->hide();

    // URLs y rutas: sin mayúscula inicial ni predicción del teclado
    const auto sinPrediccion = Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText;
    ui->editUrls->setInputMethodHints(sinPrediccion | Qt::ImhUrlCharactersOnly);
    ui->editCarpeta->setInputMethodHints(sinPrediccion);
    ui->editCookies->setInputMethodHints(sinPrediccion);

    // Se desplaza arrastrando con el dedo
    QScroller::grabGesture(ui->scrollArea->viewport(), QScroller::LeftMouseButtonGesture);

    connect(ui->comboPlataforma, &QComboBox::currentIndexChanged, this,
            [this](int plataforma) { ui->filaCarpeta->setVisible(plataforma == 1); });    // 1 = Redes sociales
    connect(ui->checkCookies, &QCheckBox::toggled, ui->editCookies, &QLineEdit::setEnabled);
    connect(ui->botonPegar, &QPushButton::clicked, this, [this] {
        const QString texto = QGuiApplication::clipboard()->text().trimmed();
        if (texto.isEmpty())
            return;
        if (!ui->editUrls->toPlainText().trimmed().isEmpty())
            ui->editUrls->appendPlainText({});
        ui->editUrls->insertPlainText(texto);
    });
    connect(ui->botonYoutubeMusic, &QPushButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://music.youtube.com"))); });
    connect(ui->botonDescargar, &QPushButton::clicked, this, &PaginaDescargas::descargar);
    connect(ui->botonCancelar, &QPushButton::clicked, this, &PaginaDescargas::cancelar);
    connect(ui->botonVer, &QPushButton::clicked, this, &PaginaDescargas::ver);
    connect(ui->botonBorrar, &QPushButton::clicked, this, &PaginaDescargas::borrar);
    connect(ui->listaVideos, &QListWidget::itemSelectionChanged, this, [this] {
        const bool hay = !ui->listaVideos->selectedItems().isEmpty();
        ui->botonVer->setEnabled(hay);
        ui->botonBorrar->setEnabled(hay);
    });

    proceso.setProcessChannelMode(QProcess::MergedChannels);
    // En su propio grupo de procesos: Cancelar corta yt-dlp y el ffmpeg que haya lanzado
    proceso.setChildProcessModifier([] { setpgid(0, 0); });
    connect(&proceso, &QProcess::readyReadStandardOutput, this, &PaginaDescargas::leerSalida);
    connect(&proceso, &QProcess::finished, this, &PaginaDescargas::terminoProceso);
    connect(&proceso, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        anotar(tr("× No se pudo ejecutar yt-dlp: %1").arg(proceso.errorString()));
        cola.clear();
        fallidas += qMax(1, total - indice);
        finalizar();
    });
}

PaginaDescargas::~PaginaDescargas()
{
    delete ui;
}

bool PaginaDescargas::ocupada() const
{
    return !ui->botonDescargar->isEnabled();
}

void PaginaDescargas::cancelarYEsperar()
{
    cancelar();
    proceso.waitForFinished(3000);
}

// Música va a Music; YouTube a Movies/Yt-dlp; redes sociales a la carpeta elegida
QString PaginaDescargas::destino() const
{
    if (tipo == Tipo::Musica)
        return Almacenamiento + QStringLiteral("/Music");
    if (ui->comboPlataforma->currentIndex() == 0)
        return Almacenamiento + QStringLiteral("/Movies/Yt-dlp");
    return ui->editCarpeta->text().trimmed();
}

QString PaginaDescargas::carpetaMiniaturas() const
{
    return QDir(herramientas->temporal()).filePath(QStringLiteral("miniaturas"));
}

QString PaginaDescargas::archivoRutas() const
{
    return QDir(herramientas->temporal()).filePath(tipo == Tipo::Musica ? QStringLiteral("rutas-musica.txt")
                                                                        : QStringLiteral("rutas-videos.txt"));
}

void PaginaDescargas::habilitarFormulario(bool habilitado)
{
    ui->grupoUrls->setEnabled(habilitado);
    ui->grupoOpciones->setEnabled(habilitado);
    ui->botonDescargar->setEnabled(habilitado);
    ui->botonCancelar->setEnabled(!habilitado);
    emit ocupacionCambiada();
}

void PaginaDescargas::anotar(const QString &texto)
{
    ui->registro->appendPlainText(texto);
}

void PaginaDescargas::descargar()
{
    QStringList urls;
    const QStringList lineas = ui->editUrls->toPlainText().split(u'\n');
    for (const QString &linea : lineas) {
        if (!linea.trimmed().isEmpty())
            urls << linea.trimmed();
    }
    if (urls.isEmpty()) {
        ui->etiquetaEstado->setText(tr("No se ingresó ninguna URL."));
        return;
    }
    if (!Plataforma::tieneAccesoArchivos()) {
        ui->etiquetaEstado->setText(tr("× Falta dar acceso a los archivos (botón en la portada)."));
        return;
    }
    if (destino().isEmpty()) {
        ui->etiquetaEstado->setText(tr("Elegí una carpeta de destino."));
        return;
    }
    if (ui->checkCookies->isChecked() && !QFileInfo::exists(ui->editCookies->text().trimmed())) {
        ui->etiquetaEstado->setText(tr("× No se encontró el archivo de cookies."));
        return;
    }
    if (!QDir().mkpath(destino())) {
        ui->etiquetaEstado->setText(tr("× No se pudo crear la carpeta %1.").arg(destino()));
        return;
    }
    QDir(carpetaMiniaturas()).removeRecursively();
    QDir().mkpath(carpetaMiniaturas());

    cola = urls;
    total = urls.size();
    indice = listas = fallidas = 0;
    cancelado = false;
    ui->barraArchivo->setValue(0);
    ui->barraTotal->setRange(0, total);
    ui->barraTotal->setValue(0);
    ui->barraArchivo->show();
    ui->barraTotal->show();
    habilitarFormulario(false);

    proceso.setProcessEnvironment(herramientas->ambiente());

    // YouTube cambia seguido y un yt-dlp viejo deja de funcionar
    if (herramientas->ytDlpDesactualizado()) {
        actualizando = true;
        ui->etiquetaEstado->setText(tr("Buscando actualizaciones de yt-dlp…"));
        proceso.start(herramientas->python(), {herramientas->argumentosYtDlp().constFirst(), QStringLiteral("--update")});
        return;
    }
    siguienteUrl();
}

void PaginaDescargas::siguienteUrl()
{
    actualizando = false;
    if (cola.isEmpty() || cancelado) {
        finalizar();
        return;
    }
    const QString url = cola.takeFirst();
    ++indice;
    mensajeError.clear();
    ultimoPorcentaje.clear();
    QFile::remove(archivoRutas());
    ui->barraTotal->setValue(indice - 1);
    ui->barraArchivo->setValue(0);
    ui->etiquetaEstado->setText(tr("URL %1 de %2: %3").arg(indice).arg(total).arg(url));

    // Las opciones de Logica/Descargar.sh
    QStringList argumentos = herramientas->argumentosYtDlp();
    argumentos << QStringLiteral("--write-thumbnail")
               << QStringLiteral("--convert-thumbnails") << QStringLiteral("jpg")    // la lista solo sabe mostrar jpg
               << QStringLiteral("--audio-quality") << QStringLiteral("0")
               << QStringLiteral("--embed-metadata")
               << QStringLiteral("--embed-thumbnail")
               << QStringLiteral("--prefer-free-formats")
               << QStringLiteral("--no-mtime")    // la fecha del archivo es la de la descarga: sale primero en la galería
               << QStringLiteral("-o") << QStringLiteral("%(artist,uploader)s/%(title)s.%(ext)s")
               << QStringLiteral("-o") << QStringLiteral("thumbnail:%1/%(id)s.%(ext)s").arg(carpetaMiniaturas())
               << QStringLiteral("--print-to-file") << QStringLiteral("after_move:filepath") << archivoRutas()
               << QStringLiteral("--newline")
               << QStringLiteral("--progress")
               << QStringLiteral("-P") << destino();
    if (tipo == Tipo::Musica) {
        argumentos << QStringLiteral("-x");
        if (!ui->checkPlaylist->isChecked())
            argumentos << QStringLiteral("--no-playlist");
    }
    if (ui->checkCookies->isChecked())
        argumentos << QStringLiteral("--cookies") << ui->editCookies->text().trimmed();
    argumentos << QStringLiteral("--") << url;

    proceso.start(herramientas->python(), argumentos);
}

void PaginaDescargas::cancelar()
{
    if (!ocupada())
        return;
    cancelado = true;
    cola.clear();
    ui->etiquetaEstado->setText(tr("Cancelando…"));
    if (proceso.state() != QProcess::NotRunning)
        ::kill(-static_cast<pid_t>(proceso.processId()), SIGTERM);
}

void PaginaDescargas::leerSalida()
{
    while (proceso.canReadLine())
        procesarLinea(QString::fromUtf8(proceso.readLine()).trimmed());
}

void PaginaDescargas::procesarLinea(const QString &linea)
{
    static const QRegularExpression porcentaje(QStringLiteral("(\\d+)(\\.\\d+)?%"));
    static const QString prefijoDestino = QStringLiteral("[download] Destination: ");

    if (actualizando) {
        if (!linea.isEmpty())
            anotar(linea);
    } else if (linea.startsWith(prefijoDestino)) {
        archivoActual = QFileInfo(linea.mid(prefijoDestino.size())).fileName();
        ultimoPorcentaje.clear();
        ui->barraArchivo->setValue(0);
        ui->etiquetaEstado->setText(tr("Descargando %1").arg(archivoActual));
    } else if (linea.startsWith(QStringLiteral("[download]"))) {
        const QRegularExpressionMatch coincidencia = porcentaje.match(linea);
        if (coincidencia.hasMatch() && coincidencia.captured(1) != ultimoPorcentaje) {
            ultimoPorcentaje = coincidencia.captured(1);
            ui->barraArchivo->setValue(ultimoPorcentaje.toInt());
            if (ultimoPorcentaje.toInt() >= 100)
                ui->etiquetaEstado->setText(tr("Procesando %1 (metadatos y carátula)…").arg(archivoActual));
        }
    } else if (linea.startsWith(QStringLiteral("ERROR: "))) {
        mensajeError = linea.mid(7);
    } else if (linea.startsWith(QStringLiteral("WARNING: "))) {
        anotar(linea);
    }
}

QString PaginaDescargas::ultimaMiniatura() const
{
    const QFileInfoList miniaturas = QDir(carpetaMiniaturas()).entryInfoList({QStringLiteral("*.jpg"), QStringLiteral("*.png")},
                                                                            QDir::Files, QDir::Time);
    return miniaturas.isEmpty() ? QString() : miniaturas.constFirst().absoluteFilePath();
}

// Lo que se llegó a bajar se muestra aunque después algo haya fallado (playlists)
void PaginaDescargas::registrarTerminados(const QString &miniatura)
{
    QFile archivo(archivoRutas());
    if (!archivo.open(QIODevice::ReadOnly))
        return;
    const QStringList rutas = QString::fromUtf8(archivo.readAll()).split(u'\n', Qt::SkipEmptyParts);
    for (const QString &ruta : rutas) {
        const QFileInfo info(ruta);
        const QString nombre = tipo == Tipo::Musica ? info.dir().dirName() + QStringLiteral(" — ") + info.completeBaseName()
                                                    : info.completeBaseName();
        auto *item = new QListWidgetItem(QIcon(miniatura), nombre, ui->listaVideos);
        item->setData(RolRuta, ruta);
        ui->listaVideos->clearSelection();
        ui->listaVideos->setCurrentItem(item);
        item->setSelected(true);
        Plataforma::avisarArchivoNuevo(ruta);
        ++listas;
    }
}

void PaginaDescargas::terminoProceso(int codigo, QProcess::ExitStatus estado)
{
    leerSalida();
    if (actualizando) {
        // Si no se pudo actualizar se sigue con la versión que hay
        if (codigo == 0 && estado == QProcess::NormalExit)
            herramientas->marcarYtDlpActualizado();
        else if (!cancelado)
            anotar(tr("No se pudo actualizar yt-dlp; se usa la versión instalada."));
        siguienteUrl();
        return;
    }

    const int antes = listas;
    registrarTerminados(ultimaMiniatura());
    if (!cancelado && (codigo != 0 || estado != QProcess::NormalExit || listas == antes)) {
        ++fallidas;
        anotar(tr("× Falló: %1").arg(mensajeError.isEmpty() ? tr("yt-dlp terminó con código %1").arg(codigo) : mensajeError));
        if (tipo == Tipo::Musica && !ui->checkCookies->isChecked())
            anotar(tr("   Puede deberse a la falta de cookies (probá con «Usar archivo de cookies»)."));
    }
    ui->barraTotal->setValue(indice);
    siguienteUrl();
}

void PaginaDescargas::finalizar()
{
    habilitarFormulario(true);
    ui->barraTotal->setValue(ui->barraTotal->maximum());

    const bool musica = tipo == Tipo::Musica;
    if (cancelado)
        ui->etiquetaEstado->setText(tr("Descarga cancelada."));
    else if (fallidas > 0)
        ui->etiquetaEstado->setText(tr("Terminado con errores: %1 descargado(s), %2 URL(s) fallaron.").arg(listas).arg(fallidas));
    else if (listas == 1)
        ui->etiquetaEstado->setText(musica ? tr("✓ Listo: 1 canción descargada.") : tr("✓ Listo: 1 video descargado."));
    else
        ui->etiquetaEstado->setText((musica ? tr("✓ Listo: %1 canciones descargadas.") : tr("✓ Listo: %1 videos descargados.")).arg(listas));
}

void PaginaDescargas::ver()
{
    if (const QListWidgetItem *item = ui->listaVideos->currentItem()) {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(item->data(RolRuta).toString())))
            ui->etiquetaEstado->setText(tr("× No hay ninguna app para abrir ese archivo."));
    }
}

// Android no tiene una papelera a la que mandar un archivo desde acá: el borrado es definitivo
void PaginaDescargas::borrar()
{
    QListWidgetItem *item = ui->listaVideos->currentItem();
    if (!item)
        return;
    QMessageBox caja(QMessageBox::Warning, windowTitle(), tr("¿Borrar «%1»? No se puede deshacer.").arg(item->text()),
                     QMessageBox::NoButton, this);
    const QPushButton *si = caja.addButton(tr("Borrar"), QMessageBox::DestructiveRole);
    caja.addButton(tr("No"), QMessageBox::RejectRole);
    caja.exec();
    if (caja.clickedButton() != si)
        return;

    const QString ruta = item->data(RolRuta).toString();
    if (QFile::remove(ruta) || !QFileInfo::exists(ruta)) {
        Plataforma::avisarArchivoNuevo(ruta);    // para que deje de aparecer en la galería
        QDir().rmdir(QFileInfo(ruta).absolutePath());    // la carpeta del artista, si quedó vacía
        delete item;
    } else {
        anotar(tr("No se pudo borrar: %1").arg(ruta));
    }
}
