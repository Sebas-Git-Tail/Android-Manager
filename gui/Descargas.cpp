#include "Descargas.h"

#include "Plataforma.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>

#include <csignal>
#include <unistd.h>

namespace {

const QString Almacenamiento = QStringLiteral("/sdcard");

} // namespace

// ── Lista de descargados ──

QVariant ModeloDescargados::data(const QModelIndex &indice, int rol) const
{
    if (!indice.isValid() || indice.row() >= filas.size())
        return {};
    const Fila &fila = filas.at(indice.row());
    switch (rol) {
    case RolNombre: return fila.nombre;
    case RolRuta: return fila.ruta;
    case RolMiniatura: return fila.miniatura.isEmpty() ? QString() : QUrl::fromLocalFile(fila.miniatura).toString();
    default: return {};
    }
}

QHash<int, QByteArray> ModeloDescargados::roleNames() const
{
    return {{RolNombre, "nombre"}, {RolRuta, "ruta"}, {RolMiniatura, "miniatura"}};
}

// Lo último bajado va arriba
void ModeloDescargados::agregar(const QString &nombre, const QString &ruta, const QString &miniatura)
{
    beginInsertRows({}, 0, 0);
    filas.prepend({nombre, ruta, miniatura});
    endInsertRows();
    emit cantidadCambio();
}

void ModeloDescargados::quitar(int fila)
{
    if (fila < 0 || fila >= filas.size())
        return;
    beginRemoveRows({}, fila, fila);
    filas.removeAt(fila);
    endRemoveRows();
    emit cantidadCambio();
}

// ── Descargas ──

Descargas::Descargas(Tipo tipo, Herramientas *herramientas, QObject *parent)
    : QObject(parent), tipo(tipo), herramientas(herramientas)
{
    carpeta_ = Almacenamiento + QStringLiteral("/Movies");
    usarServidor_ = QSettings().value(QStringLiteral("descargas/servidor"), true).toBool();
    // Las miniaturas de la lista son de esta sesión
    QDir(carpetaMiniaturasLista()).removeRecursively();

    proceso.setProcessChannelMode(QProcess::MergedChannels);
    // En su propio grupo de procesos: Cancelar corta yt-dlp y el ffmpeg que haya lanzado
    proceso.setChildProcessModifier([] { setpgid(0, 0); });
    connect(&proceso, &QProcess::readyReadStandardOutput, this, &Descargas::leerSalida);
    connect(&proceso, &QProcess::finished, this, &Descargas::terminoProceso);
    temporizadorRemoto.setSingleShot(true);
    temporizadorRemoto.setInterval(1500);
    connect(&temporizadorRemoto, &QTimer::timeout, this, &Descargas::remotoPreguntar);
    temporizadorTransferencia.setInterval(500);
    connect(&temporizadorTransferencia, &QTimer::timeout, this, &Descargas::remotoMostrarTransferencia);
    connect(&proceso, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        anotar(tr("× No se pudo ejecutar yt-dlp: %1").arg(proceso.errorString()));
        cola.clear();
        fallidas += qMax(1, total_ - indice);
        finalizar();
    });
}

void Descargas::ponerUsarServidor(bool usar)
{
    if (usar == usarServidor_ || ocupada_)
        return;
    usarServidor_ = usar;
    QSettings().setValue(QStringLiteral("descargas/servidor"), usar);
    emit formularioCambio();
}

QString Descargas::titulo() const
{
    return musica() ? tr("Descargar música") : tr("Descargar videos");
}

void Descargas::cancelarYEsperar()
{
    cancelar();
    proceso.waitForFinished(3000);
}

// Música va a Music; YouTube a Movies/Yt-dlp; redes sociales a la carpeta elegida
QString Descargas::destino() const
{
    if (musica())
        return Almacenamiento + QStringLiteral("/Music");
    if (plataforma_ == 0)
        return Almacenamiento + QStringLiteral("/Movies/Yt-dlp");
    return carpeta_.trimmed();
}

QString Descargas::carpetaSubtitulos() const
{
    return QDir(herramientas->temporal()).filePath(QStringLiteral("subtitulos"));
}

QString Descargas::carpetaMiniaturas() const
{
    return QDir(herramientas->temporal()).filePath(musica() ? QStringLiteral("miniaturas-musica") : QStringLiteral("miniaturas-videos"));
}

// Copias de las miniaturas para la lista: las de carpetaMiniaturas() se borran en cada descarga
QString Descargas::carpetaMiniaturasLista() const
{
    return QDir(herramientas->temporal()).filePath(musica() ? QStringLiteral("lista-musica") : QStringLiteral("lista-videos"));
}

QString Descargas::archivoRutas() const
{
    return QDir(herramientas->temporal()).filePath(musica() ? QStringLiteral("rutas-musica.txt") : QStringLiteral("rutas-videos.txt"));
}

void Descargas::ponerOcupada(bool ocupada)
{
    if (ocupada_ == ocupada)
        return;
    ocupada_ = ocupada;
    emit ocupadaCambio();
}

void Descargas::ponerEstado(const QString &texto, TipoEstado tipo)
{
    estado_ = texto;
    tipoEstado_ = tipo;
    emit estadoCambio();
}

void Descargas::ponerProgreso(int archivo, int hechas)
{
    progresoArchivo_ = archivo;
    hechas_ = hechas;
    emit progresoCambio();
}

void Descargas::anotar(const QString &texto)
{
    avisos_ << texto;
    emit avisosCambio();
}

void Descargas::limpiarAvisos()
{
    avisos_.clear();
    emit avisosCambio();
}

void Descargas::pegar()
{
    const QString texto = QGuiApplication::clipboard()->text().trimmed();
    if (texto.isEmpty())
        return;
    if (!urls_.trimmed().isEmpty() && !urls_.endsWith(u'\n'))
        urls_ += u'\n';
    urls_ += texto;
    emit formularioCambio();
}

void Descargas::abrirYoutubeMusic()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://music.youtube.com")));
}

void Descargas::descargar()
{
    if (ocupada_)
        return;
    QStringList urls;
    const QStringList lineas = urls_.split(u'\n');
    for (const QString &linea : lineas) {
        if (!linea.trimmed().isEmpty())
            urls << linea.trimmed();
    }
    if (urls.isEmpty()) {
        ponerEstado(tr("No se ingresó ninguna URL."), Error);
        return;
    }
    if (!Plataforma::tieneAccesoArchivos()) {
        ponerEstado(tr("Falta dar acceso a los archivos (botón en la portada)."), Error);
        return;
    }
    if (destino().isEmpty()) {
        ponerEstado(tr("Elegí una carpeta de destino."), Error);
        return;
    }
    if (!QDir().mkpath(destino())) {
        ponerEstado(tr("No se pudo crear la carpeta %1.").arg(destino()), Error);
        return;
    }
    QDir(carpetaMiniaturas()).removeRecursively();
    QDir(carpetaSubtitulos()).removeRecursively();
    QDir().mkpath(carpetaMiniaturas());

    cola = urls;
    urlsPedidas = urls;
    eleccion = {};
    oferta.limpiar();
    total_ = int(urls.size());
    indice = listas = fallidas = 0;
    cancelado = false;
    avisos_.clear();
    emit avisosCambio();
    mostrarProgreso_ = true;
    ponerProgreso(0, 0);
    ponerOcupada(true);

    remoto = usarServidor_;
    if (remoto) {
        remotoEmpezar();
        return;
    }
    proceso.setProcessEnvironment(herramientas->ambiente());

    // YouTube cambia seguido y un yt-dlp viejo deja de funcionar
    if (herramientas->ytDlpDesactualizado()) {
        actualizando = true;
        ponerProgreso(-1, 0);
        ponerEstado(tr("Buscando actualizaciones de yt-dlp…"));
        proceso.start(herramientas->python(), {herramientas->argumentosYtDlp().constFirst(), QStringLiteral("--update")});
        return;
    }
    empezar();
}

// Después de actualizar yt-dlp: en videos, si se pidió, primero la consulta; si no, a descargar
void Descargas::empezar()
{
    actualizando = false;
    if (!musica() && elegirPistas_ && !cancelado) {
        consultando = true;
        indice = 0;
        // La respuesta es JSON por la salida estándar: los avisos van aparte
        proceso.setProcessChannelMode(QProcess::SeparateChannels);
        siguienteConsulta();
        return;
    }
    siguienteUrl();
}

// «yt-dlp -J» de la URL siguiente (lo que hace Descargar.sh --pistas en Linux)
void Descargas::siguienteConsulta()
{
    if (cancelado) {
        consultando = false;
        proceso.setProcessChannelMode(QProcess::MergedChannels);
        finalizar();
        return;
    }
    const QString url = urlsPedidas.value(indice);
    ++indice;
    ponerProgreso(-1, indice - 1);
    ponerEstado(tr("Buscando calidades, subtítulos y pistas de audio (URL %1 de %2)…").arg(indice).arg(total_));
    QStringList argumentos = herramientas->argumentosYtDlp();
    argumentos << argumentosConsulta();
    argumentos << QStringLiteral("--") << url;
    proceso.start(herramientas->python(), argumentos);
}

void Descargas::terminoConsulta(int codigo, QProcess::ExitStatus estado)
{
    const QByteArray respuesta = proceso.readAllStandardOutput();
    const QString errores = QString::fromUtf8(proceso.readAllStandardError());
    if (!cancelado && (codigo != 0 || estado != QProcess::NormalExit || !oferta.agregar(respuesta))) {
        static const QRegularExpression lineaError(QStringLiteral("^ERROR: (.*)$"), QRegularExpression::MultilineOption);
        QString mensaje;
        for (auto it = lineaError.globalMatch(errores); it.hasNext();)
            mensaje = it.next().captured(1).trimmed();
        anotar(tr("× No se pudo consultar %1: %2").arg(urlsPedidas.value(indice - 1),
                                                       mensaje.isEmpty() ? tr("yt-dlp terminó con código %1").arg(codigo) : mensaje));
        ++fallidas;
    }
    if (indice < total_ && !cancelado) {
        siguienteConsulta();
        return;
    }

    consultando = false;
    proceso.setProcessChannelMode(QProcess::MergedChannels);
    indice = 0;
    if (cancelado || fallidas == total_) {
        // Si no se pudo consultar ninguna URL, tampoco se van a poder descargar
        cola.clear();
        finalizar();
        return;
    }
    fallidas = 0;
    if (oferta.vacia()) {
        anotar(tr("No hay calidades, subtítulos ni otras pistas de audio para elegir: se descarga como siempre."));
        siguienteUrl();
        return;
    }
    ponerEstado(tr("Elegí la calidad, los códecs y qué subtítulos y pistas de audio incluir."));
    modeloPistas.cargar(oferta);
    esperandoEleccion = true;
    emit pedirEleccion();
}

void Descargas::continuarConEleccion(const QVariantMap &elegido)
{
    if (!esperandoEleccion)
        return;
    esperandoEleccion = false;
    eleccion = ModeloPistas::eleccionDe(elegido);
    if (remoto)
        remotoCrearTrabajo();
    else
        siguienteUrl();
}

void Descargas::cancelarEleccion()
{
    if (!esperandoEleccion)
        return;
    esperandoEleccion = false;
    cancelado = true;
    cola.clear();
    finalizar();
}

void Descargas::siguienteUrl()
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
    ponerProgreso(0, indice - 1);
    ponerEstado(tr("URL %1 de %2: %3").arg(indice).arg(total_).arg(url));

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
    if (musica()) {
        argumentos << QStringLiteral("-x");
        if (!playlist_)
            argumentos << QStringLiteral("--no-playlist");
    } else {
        // MKV siempre; calidad, códecs, pistas y subtítulos si se eligieron
        argumentos << argumentosEleccion(eleccion, carpetaSubtitulos());
    }
    argumentos << QStringLiteral("--") << url;

    proceso.start(herramientas->python(), argumentos);
}

void Descargas::cancelar()
{
    if (!ocupada_)
        return;
    if (esperandoEleccion) {
        cancelarEleccion();
        return;
    }
    cancelado = true;
    cola.clear();
    ponerEstado(tr("Cancelando…"));
    if (remoto) {
        remotoCancelar();
        return;
    }
    if (proceso.state() != QProcess::NotRunning)
        ::kill(-static_cast<pid_t>(proceso.processId()), SIGTERM);
}

void Descargas::leerSalida()
{
    if (consultando)
        return;    // la respuesta de la consulta se lee entera al terminar
    while (proceso.canReadLine())
        procesarLinea(QString::fromUtf8(proceso.readLine()).trimmed());
}

void Descargas::procesarLinea(const QString &linea)
{
    static const QRegularExpression porcentaje(QStringLiteral("(\\d+)(\\.\\d+)?%"));
    static const QString prefijoDestino = QStringLiteral("[download] Destination: ");

    if (actualizando) {
        if (!linea.isEmpty())
            anotar(linea);
    } else if (linea.startsWith(prefijoDestino)) {
        archivoActual = QFileInfo(linea.mid(prefijoDestino.size())).fileName();
        ultimoPorcentaje.clear();
        ponerProgreso(0, hechas_);
        ponerEstado(tr("Descargando %1").arg(archivoActual));
    } else if (linea.startsWith(QStringLiteral("[download]"))) {
        const QRegularExpressionMatch coincidencia = porcentaje.match(linea);
        if (coincidencia.hasMatch() && coincidencia.captured(1) != ultimoPorcentaje) {
            ultimoPorcentaje = coincidencia.captured(1);
            ponerProgreso(ultimoPorcentaje.toInt(), hechas_);
            if (ultimoPorcentaje.toInt() >= 100)
                ponerEstado(tr("Procesando %1 (metadatos y carátula)…").arg(archivoActual));
        }
    } else if (linea.startsWith(QStringLiteral("ERROR: "))) {
        mensajeError = linea.mid(7);
    } else if (linea.startsWith(QStringLiteral("WARNING: "))) {
        anotar(linea);
    }
}

QString Descargas::ultimaMiniatura() const
{
    const QFileInfoList miniaturas = QDir(carpetaMiniaturas()).entryInfoList({QStringLiteral("*.jpg"), QStringLiteral("*.png")},
                                                                            QDir::Files, QDir::Time);
    return miniaturas.isEmpty() ? QString() : miniaturas.constFirst().absoluteFilePath();
}

// Lo que se llegó a bajar se muestra aunque después algo haya fallado (playlists)
void Descargas::registrarTerminados(const QString &miniatura)
{
    QFile archivo(archivoRutas());
    if (!archivo.open(QIODevice::ReadOnly))
        return;
    // Copia de la miniatura para la lista (la original se borra en la descarga siguiente)
    QString copia;
    if (!miniatura.isEmpty()) {
        QDir().mkpath(carpetaMiniaturasLista());
        copia = QDir(carpetaMiniaturasLista()).filePath(QStringLiteral("%1.%2").arg(++miniaturasGuardadas).arg(QFileInfo(miniatura).suffix()));
        if (!QFile::copy(miniatura, copia))
            copia.clear();
    }
    const QStringList rutas = QString::fromUtf8(archivo.readAll()).split(u'\n', Qt::SkipEmptyParts);
    for (const QString &ruta : rutas) {
        const QFileInfo info(ruta);
        const QString nombre = musica() ? info.dir().dirName() + QStringLiteral(" — ") + info.completeBaseName()
                                        : info.completeBaseName();
        modeloDescargados.agregar(nombre, ruta, copia);
        Plataforma::avisarArchivoNuevo(ruta);
        ++listas;
    }
}

void Descargas::terminoProceso(int codigo, QProcess::ExitStatus estado)
{
    if (consultando) {
        terminoConsulta(codigo, estado);
        return;
    }
    leerSalida();
    if (actualizando) {
        // Si no se pudo actualizar se sigue con la versión que hay
        if (codigo == 0 && estado == QProcess::NormalExit)
            herramientas->marcarYtDlpActualizado();
        else if (!cancelado)
            anotar(tr("No se pudo actualizar yt-dlp; se usa la versión instalada."));
        empezar();
        return;
    }

    const int antes = listas;
    registrarTerminados(ultimaMiniatura());
    if (!cancelado && (codigo != 0 || estado != QProcess::NormalExit || listas == antes)) {
        ++fallidas;
        anotar(tr("× Falló: %1").arg(mensajeError.isEmpty() ? tr("yt-dlp terminó con código %1").arg(codigo) : mensajeError));
        if (musica())
            anotar(tr("   Puede deberse a la falta de cookies (probá con «Bajar con el servidor», que usa las del servidor)."));
    }
    ponerProgreso(progresoArchivo_, indice);
    siguienteUrl();
}

void Descargas::finalizar()
{
    ponerProgreso(-1, total_);
    mostrarProgreso_ = false;
    emit progresoCambio();
    ponerOcupada(false);

    if (cancelado)
        ponerEstado(tr("Descarga cancelada."));
    else if (fallidas > 0)
        ponerEstado(tr("Terminó con errores: %1 descargado(s), %2 URL(s) fallaron.").arg(listas).arg(fallidas), Error);
    else if (listas == 1)
        ponerEstado(musica() ? tr("Listo: 1 canción descargada.") : tr("Listo: 1 video descargado."), Exito);
    else
        ponerEstado((musica() ? tr("Listo: %1 canciones descargadas.") : tr("Listo: %1 videos descargados.")).arg(listas), Exito);
}

bool Descargas::ver(int fila)
{
    const QString ruta = modeloDescargados.ruta(fila);
    if (ruta.isEmpty())
        return false;
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(ruta)))
        return true;
    ponerEstado(tr("No hay ninguna app para abrir ese archivo."), Error);
    return false;
}

void Descargas::borrar(int fila)
{
    const QString ruta = modeloDescargados.ruta(fila);
    if (ruta.isEmpty())
        return;
    if (QFile::remove(ruta) || !QFileInfo::exists(ruta)) {
        Plataforma::avisarArchivoNuevo(ruta);    // para que deje de aparecer en la galería
        QDir().rmdir(QFileInfo(ruta).absolutePath());    // la carpeta del artista, si quedó vacía
        modeloDescargados.quitar(fila);
    } else {
        anotar(tr("No se pudo borrar: %1").arg(ruta));
    }
}
