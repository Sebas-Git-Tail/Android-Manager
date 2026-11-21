// «Bajar con el servidor»: la notebook baja con yt-dlp y las cookies de su LibreWolf y el celular
// trae los archivos terminados. Es la API de descargas_web.py (~/Scripts/Claude/servidor-web en el
// servidor), la misma que usan las páginas https://bodrio.seb-dev.fyi/descargar-videos/ y
// /descargar-musica/:
//   POST api/consulta {url}            lo que ofrece el video (para elegir calidad, audio y subtítulos)
//   POST api/trabajos {urls, ...}      empieza la descarga → {id}
//   GET  api/trabajos/ID               cómo va y qué archivos ya terminaron
//   GET  api/trabajos/ID/archivos/CARPETA/NOMBRE   (y miniaturas/CARPETA/NOMBRE.jpg)
//   POST api/trabajos/ID/cancelar
// Los archivos quedan en las mismas carpetas que bajando en el celular (Music/Artista/…,
// Movies/Yt-dlp/Canal/…). Las cookies nunca llegan al celular.

#include "Descargas.h"

#include "ClienteHttp.h"
#include "ConfigServidor.h"
#include "Plataforma.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>

namespace {

// En /sdcard Android no deja usar algunos caracteres que en Linux sí valen: se cambian por «_»
QString nombreSeguro(QString nombre)
{
    static const QRegularExpression prohibidos(QStringLiteral("[\\x00-\\x1f\"*:<>?\\\\|/]"));
    nombre.replace(prohibidos, QStringLiteral("_"));
    nombre = nombre.trimmed();
    if (nombre.isEmpty() || nombre == u"." || nombre == u"..")
        nombre = QStringLiteral("_");
    return nombre;
}

QString megas(qint64 bytes)
{
    return QStringLiteral("%1 MB").arg(double(bytes) / (1024 * 1024), 0, 'f', 1);
}

bool terminado(const QString &estado)
{
    return estado == u"listo" || estado == u"error" || estado == u"cancelado" || estado == u"perdido";
}

QByteArray json(const QJsonObject &objeto)
{
    return QJsonDocument(objeto).toJson(QJsonDocument::Compact);
}

} // namespace

QUrl Descargas::urlServidor(const QString &ruta) const
{
    // Cada parte del camino va codificada (los nombres traen espacios, tildes, «#»…)
    QByteArray camino;
    const QStringList partes = ruta.split(u'/');
    for (const QString &parte : partes)
        camino += (camino.isEmpty() ? "" : "/") + QUrl::toPercentEncoding(parte);
    const QString base = ConfigServidor::instancia().webPublica()
                         + (musica() ? QStringLiteral("/descargar-musica/") : QStringLiteral("/descargar-videos/"));
    return QUrl::fromEncoded(base.toUtf8() + camino);
}

// El mensaje de error que manda el servidor ({"error": "..."}), o uno según el código
QString Descargas::mensajeDe(int codigo, const QByteArray &cuerpo)
{
    if (codigo == 0)
        return tr("No se pudo conectar con el servidor.");
    const QString mensaje = QJsonDocument::fromJson(cuerpo).object().value(QStringLiteral("error")).toString();
    return mensaje.isEmpty() ? tr("El servidor respondió con el código %1.").arg(codigo) : mensaje;
}

void Descargas::remotoEmpezar()
{
    ++vueltaRemota;
    trabajoRemoto.clear();
    estadoRemoto.clear();
    ultimoRemoto = {};
    porBajar.clear();
    vistos.clear();
    trayendo = false;
    fallosSeguidos = traidosFallidos = 0;
    if (!musica() && elegirPistas_) {
        consultando = true;
        indice = 0;
        remotoSiguienteConsulta();
        return;
    }
    remotoCrearTrabajo();
}

// La consulta de una URL la hace el servidor (con sus cookies): mismo paso que «yt-dlp -J» en el celular
void Descargas::remotoSiguienteConsulta()
{
    const QString url = urlsPedidas.value(indice);
    ++indice;
    ponerProgreso(-1, indice - 1);
    ponerEstado(tr("El servidor está buscando calidades, subtítulos y pistas de audio (URL %1 de %2)…").arg(indice).arg(total_));
    const int vuelta = vueltaRemota;
    http::enviar(this, urlServidor(QStringLiteral("api/consulta")), json({{QStringLiteral("url"), url}}),
                 [this, vuelta, url](int codigo, const QByteArray &cuerpo) {
        if (vuelta != vueltaRemota || !ocupada_)
            return;
        if (!cancelado && (codigo != 200 || !oferta.agregar(cuerpo))) {
            anotar(tr("× No se pudo consultar %1: %2").arg(url, mensajeDe(codigo, cuerpo)));
            ++fallidas;
        }
        if (indice < total_ && !cancelado) {
            remotoSiguienteConsulta();
            return;
        }
        consultando = false;
        indice = 0;
        if (cancelado || fallidas == total_) {
            finalizar();
            return;
        }
        fallidas = 0;
        if (oferta.vacia()) {
            anotar(tr("No hay calidades, subtítulos ni otras pistas de audio para elegir: se descarga como siempre."));
            remotoCrearTrabajo();
            return;
        }
        ponerEstado(tr("Elegí la calidad, los códecs y qué subtítulos y pistas de audio incluir."));
        modeloPistas.cargar(oferta);
        esperandoEleccion = true;
        emit pedirEleccion();
    }, 90);
}

void Descargas::remotoCrearTrabajo()
{
    QJsonObject datos{{QStringLiteral("urls"), QJsonArray::fromStringList(urlsPedidas)},
                      // En el celular los videos bajan la playlist entera (no llevan --no-playlist)
                      {QStringLiteral("playlist"), musica() ? playlist_ : true}};
    if (!musica()) {
        datos.insert(QStringLiteral("calidad"), eleccion.calidad);
        datos.insert(QStringLiteral("vcodec"), eleccion.vcodec);
        datos.insert(QStringLiteral("acodec"), eleccion.acodec);
        datos.insert(QStringLiteral("audios"), QJsonArray::fromStringList(eleccion.audios));
        datos.insert(QStringLiteral("subtitulos"), QJsonArray::fromStringList(eleccion.subtitulos));
    }
    ponerProgreso(-1, 0);
    ponerEstado(tr("Mandando los enlaces al servidor…"));
    const int vuelta = vueltaRemota;
    http::enviar(this, urlServidor(QStringLiteral("api/trabajos")), json(datos), [this, vuelta](int codigo, const QByteArray &cuerpo) {
        const QString id = QJsonDocument::fromJson(cuerpo).object().value(QStringLiteral("id")).toString();
        if (vuelta != vueltaRemota || !ocupada_) {
            // Se canceló mientras tanto: que el servidor tampoco siga
            if (!id.isEmpty())
                http::enviar(this, urlServidor(QStringLiteral("api/trabajos/%1/cancelar").arg(id)), {}, [](int, const QByteArray &) {});
            return;
        }
        if (codigo != 201 || id.isEmpty()) {
            remotoFallo(mensajeDe(codigo, cuerpo));
            return;
        }
        trabajoRemoto = id;
        ponerEstado(tr("En la cola del servidor…"));
        remotoPreguntar();
    });
}

void Descargas::remotoPreguntar()
{
    if (trabajoRemoto.isEmpty() || terminado(estadoRemoto))
        return;
    const int vuelta = vueltaRemota;
    http::pedir(this, urlServidor(QStringLiteral("api/trabajos/%1").arg(trabajoRemoto)), [this, vuelta](int codigo, const QByteArray &cuerpo) {
        if (vuelta != vueltaRemota || !ocupada_ || terminado(estadoRemoto))
            return;
        const QJsonDocument documento = QJsonDocument::fromJson(cuerpo);
        if (codigo == 200 && documento.isObject()) {
            fallosSeguidos = 0;
            remotoProcesar(documento.object());
            return;
        }
        if (codigo == 404) {
            remotoFallo(tr("El servidor ya no tiene esta descarga (venció o se reinició)."));
            return;
        }
        // Un corte de la conexión del celular: se sigue intentando un rato (el servidor sigue bajando)
        if (++fallosSeguidos >= 40) {
            remotoFallo(tr("Se perdió la conexión con el servidor."));
            return;
        }
        temporizadorRemoto.start();
    });
}

void Descargas::remotoProcesar(const QJsonObject &trabajo)
{
    ultimoRemoto = trabajo;
    estadoRemoto = trabajo.value(QStringLiteral("estado")).toString();

    // Archivos que terminaron en el servidor y todavía no se pidieron
    const QJsonArray carpetas = trabajo.value(QStringLiteral("carpetas")).toArray();
    for (const QJsonValue &valorCarpeta : carpetas) {
        const QJsonObject carpeta = valorCarpeta.toObject();
        const QString nombreCarpeta = carpeta.value(QStringLiteral("nombre")).toString();
        const QJsonArray archivos = carpeta.value(QStringLiteral("archivos")).toArray();
        for (const QJsonValue &valorArchivo : archivos) {
            const QJsonObject archivo = valorArchivo.toObject();
            PorBajar nuevo{nombreCarpeta, archivo.value(QStringLiteral("nombre")).toString(),
                           qint64(archivo.value(QStringLiteral("tamano")).toDouble()),
                           archivo.value(QStringLiteral("miniatura")).toBool()};
            const QString clave = nuevo.carpeta + u'/' + nuevo.nombre;
            if (!nuevo.nombre.isEmpty() && !vistos.contains(clave)) {
                vistos.insert(clave);
                porBajar << nuevo;
            }
        }
    }

    // Cuántas URL ya terminó el servidor
    int hechasUrls = 0;
    const QJsonArray urls = trabajo.value(QStringLiteral("urls")).toArray();
    for (const QJsonValue &valor : urls) {
        const QString estado = valor.toObject().value(QStringLiteral("estado")).toString();
        if (estado == u"listo" || estado == u"parcial" || estado == u"error")
            ++hechasUrls;
    }

    // Mientras se trae un archivo, la tarjeta muestra eso; si no, lo que hace el servidor
    if (trayendo) {
        ponerProgreso(progresoArchivo_, hechasUrls);
    } else if (estadoRemoto == u"en cola") {
        ponerProgreso(-1, hechasUrls);
        ponerEstado(tr("En la cola del servidor (esperando turno)…"));
    } else if (estadoRemoto == u"descargando") {
        const QString actual = trabajo.value(QStringLiteral("actual")).toString();
        const QString item = trabajo.value(QStringLiteral("item")).toString();
        const int porcentaje = trabajo.value(QStringLiteral("porcentaje")).toInt();
        ponerProgreso(actual.isEmpty() ? -1 : porcentaje, hechasUrls);
        QString texto = actual.isEmpty() ? tr("El servidor está preparando la descarga…")
                        : porcentaje >= 100 ? tr("El servidor está procesando %1…").arg(actual)
                                            : tr("El servidor está bajando %1").arg(actual);
        if (!item.isEmpty())
            texto += tr(" (%1)").arg(item);
        ponerEstado(texto);
    }

    remotoSiguienteArchivo();
    if (terminado(estadoRemoto))
        remotoQuizasTerminar();
    else
        temporizadorRemoto.start();
}

// Trae al celular el siguiente archivo terminado (primero su miniatura, que es chica, para la lista)
void Descargas::remotoSiguienteArchivo()
{
    if (trayendo || porBajar.isEmpty() || cancelado)
        return;
    enCamino = porBajar.takeFirst();
    trayendo = true;
    const QString carpeta = QDir(destino()).filePath(nombreSeguro(enCamino.carpeta));
    if (!QDir().mkpath(carpeta)) {
        anotar(tr("× No se pudo crear la carpeta %1.").arg(carpeta));
        ++traidosFallidos;
        trayendo = false;
        remotoSiguienteArchivo();
        remotoQuizasTerminar();
        return;
    }
    const QString final = QDir(carpeta).filePath(nombreSeguro(enCamino.nombre));
    rutaEnCamino = final + QStringLiteral(".parcial");
    QFile::remove(rutaEnCamino);
    const QString base = QStringLiteral("api/trabajos/%1/").arg(trabajoRemoto);
    const int vuelta = vueltaRemota;

    const auto traerArchivo = [this, vuelta, base, final](const QString &miniatura) {
        archivoActual = QFileInfo(enCamino.nombre).completeBaseName();
        ponerProgreso(0, hechas_);
        remotoMostrarTransferencia();
        temporizadorTransferencia.start();
        const PorBajar archivo = enCamino;
        http::descargar(this, urlServidor(base + QStringLiteral("archivos/%1/%2").arg(archivo.carpeta, archivo.nombre)), rutaEnCamino,
                        [this, vuelta, archivo, final, miniatura](int codigo) {
            if (vuelta == vueltaRemota)
                remotoTerminoArchivo(codigo, archivo, final, miniatura);
        });
    };
    if (!enCamino.miniatura) {
        traerArchivo({});
        return;
    }
    QDir().mkpath(carpetaMiniaturasLista());
    const QString miniatura = QDir(carpetaMiniaturasLista()).filePath(QStringLiteral("%1.jpg").arg(++miniaturasGuardadas));
    const QString nombreMiniatura = QFileInfo(enCamino.nombre).completeBaseName() + QStringLiteral(".jpg");
    http::descargar(this, urlServidor(base + QStringLiteral("miniaturas/%1/%2").arg(enCamino.carpeta, nombreMiniatura)), miniatura,
                    [this, vuelta, miniatura, traerArchivo](int codigo) {
        if (vuelta != vueltaRemota)
            return;
        if (cancelado) {
            trayendo = false;
            remotoQuizasTerminar();
            return;
        }
        traerArchivo(codigo == 200 ? miniatura : QString());
    });
}

void Descargas::remotoMostrarTransferencia()
{
    if (!trayendo || rutaEnCamino.isEmpty())
        return;
    const qint64 bajado = QFileInfo(rutaEnCamino).size();
    const int porcentaje = enCamino.tamano > 0 ? int(qMin<qint64>(100, bajado * 100 / enCamino.tamano)) : -1;
    ponerProgreso(porcentaje, hechas_);
    ponerEstado(tr("Trayendo al celular: %1 (%2 de %3)").arg(archivoActual, megas(bajado), megas(enCamino.tamano)));
}

void Descargas::remotoTerminoArchivo(int codigo, const PorBajar &archivo, const QString &final, const QString &miniatura)
{
    temporizadorTransferencia.stop();
    trayendo = false;
    if (codigo == 200) {
        QFile::remove(final);    // si ya estaba (otra descarga del mismo video), se reemplaza
        if (QFile::rename(rutaEnCamino, final)) {
            const QFileInfo info(final);
            const QString nombre = musica() ? info.dir().dirName() + QStringLiteral(" — ") + info.completeBaseName()
                                            : info.completeBaseName();
            modeloDescargados.agregar(nombre, final, miniatura);
            Plataforma::avisarArchivoNuevo(final);
            ++listas;
        } else {
            anotar(tr("× No se pudo guardar %1.").arg(final));
            QFile::remove(rutaEnCamino);
            ++traidosFallidos;
        }
    } else {
        QFile::remove(rutaEnCamino);
        if (!cancelado) {
            anotar(tr("× No se pudo traer «%1» del servidor (%2).")
                       .arg(archivo.nombre, codigo ? tr("código %1").arg(codigo) : tr("se cortó la conexión")));
            ++traidosFallidos;
        }
    }
    rutaEnCamino.clear();
    remotoSiguienteArchivo();
    remotoQuizasTerminar();
}

// Termina cuando el servidor terminó y ya se trajo todo (o se canceló)
void Descargas::remotoQuizasTerminar()
{
    if (!ocupada_ || trayendo || !terminado(estadoRemoto) || (!porBajar.isEmpty() && !cancelado))
        return;
    temporizadorRemoto.stop();
    if (!cancelado) {
        const QJsonArray urls = ultimoRemoto.value(QStringLiteral("urls")).toArray();
        int sinTerminar = 0;
        for (const QJsonValue &valor : urls) {
            const QJsonObject url = valor.toObject();
            const QString estado = url.value(QStringLiteral("estado")).toString();
            if (estado == u"error" || estado == u"parcial") {
                ++fallidas;
                anotar(tr("× Falló %1: %2").arg(url.value(QStringLiteral("url")).toString(),
                                               url.value(QStringLiteral("error")).toString()));
            } else if (estado != u"listo") {
                ++sinTerminar;
            }
        }
        if (estadoRemoto == u"perdido")
            fallidas += qMax(1, sinTerminar);
        fallidas += traidosFallidos;
    }
    ++vueltaRemota;    // lo que llegue tarde de esta descarga ya no cuenta
    finalizar();
}

void Descargas::remotoFallo(const QString &mensaje)
{
    if (!ocupada_)
        return;
    anotar(QStringLiteral("× ") + mensaje);
    estadoRemoto = QStringLiteral("perdido");
    temporizadorRemoto.stop();
    porBajar.clear();
    if (trayendo)
        http::cancelarDescarga(rutaEnCamino);    // su respuesta termina la descarga
    else
        remotoQuizasTerminar();
}

void Descargas::remotoCancelar()
{
    porBajar.clear();
    temporizadorRemoto.stop();
    if (!trabajoRemoto.isEmpty() && !terminado(estadoRemoto))
        http::enviar(this, urlServidor(QStringLiteral("api/trabajos/%1/cancelar").arg(trabajoRemoto)), {}, [](int, const QByteArray &) {});
    estadoRemoto = QStringLiteral("cancelado");
    if (trayendo) {
        http::cancelarDescarga(rutaEnCamino);    // su respuesta termina la descarga
        return;
    }
    // Si quedaba una consulta o el pedido de crear el trabajo, sus respuestas se ignoran (vueltaRemota)
    consultando = false;
    remotoQuizasTerminar();
}
