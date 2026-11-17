// Lo que muestra la página «Calidad, audio y subtítulos» (PaginaPistas.qml): las opciones que
// ofrecen los videos consultados, en el orden y con los textos de OpcionesPistas (comun), los
// mismos del diálogo de las versiones de escritorio.

#pragma once

#include "ConsultaVideo.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class ModeloPistas : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList calidades READ calidades NOTIFY cambio)       // [{texto, valor}]; valor 0 = la mejor
    Q_PROPERTY(QStringList codecsVideo READ codecsVideo NOTIFY cambio)    // familias; "" = cualquiera
    Q_PROPERTY(QVariantList codecsAudio READ codecsAudio NOTIFY cambio)   // [{texto, valor}]; valor "" = cualquiera
    Q_PROPERTY(QVariantList audios READ audios NOTIFY cambio)             // [{texto, idioma}]
    Q_PROPERTY(QVariantList subtitulos READ subtitulos NOTIFY cambio)     // [{texto, idioma, secundario}]
    Q_PROPERTY(int secundarios READ secundarios NOTIFY cambio)            // traducciones automáticas ocultas

public:
    using QObject::QObject;

    void cargar(const OfertaVideo &oferta);

    QVariantList calidades() const { return calidades_; }
    QStringList codecsVideo() const { return codecsVideo_; }
    QVariantList codecsAudio() const { return codecsAudio_; }
    QVariantList audios() const { return audios_; }
    QVariantList subtitulos() const { return subtitulos_; }
    int secundarios() const { return secundarios_; }

    // Nombre de un códec de video, avisando si no está en la calidad elegida
    Q_INVOKABLE QString textoCodecVideo(const QString &familia, int alto) const;
    // Qué se va a bajar con esa calidad y ese códec
    Q_INVOKABLE QString explicacion(int alto, const QString &vcodec) const;

    // {audios: [...], subtitulos: [...], calidad, vcodec, acodec} → Eleccion
    static Eleccion eleccionDe(const QVariantMap &mapa);

signals:
    void cambio();

private:
    QList<FormatoVideo> formatos_;
    QVariantList calidades_, codecsAudio_, audios_, subtitulos_;
    QStringList codecsVideo_;
    int secundarios_ = 0;
};
