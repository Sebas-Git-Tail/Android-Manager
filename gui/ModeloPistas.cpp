#include "ModeloPistas.h"

#include "OpcionesPistas.h"

void ModeloPistas::cargar(const OfertaVideo &oferta)
{
    formatos_ = oferta.formatos;

    calidades_.clear();
    if (!formatos_.isEmpty()) {     // el sitio no informó calidades: no hay nada que elegir
        calidades_ << QVariantMap{{QStringLiteral("texto"), OpcionesPistas::textoCalidad(0)}, {QStringLiteral("valor"), 0}};
        for (int alto : OpcionesPistas::calidades(formatos_))
            calidades_ << QVariantMap{{QStringLiteral("texto"), OpcionesPistas::textoCalidad(alto)}, {QStringLiteral("valor"), alto}};
    }
    codecsVideo_ = QStringList{QString()} + OpcionesPistas::familiasVideo(formatos_);

    codecsAudio_ = {QVariantMap{{QStringLiteral("texto"), tr("Cualquiera")}, {QStringLiteral("valor"), QString()}}};
    for (const QString &familia : OpcionesPistas::familiasAudio(oferta.codecsAudio))
        codecsAudio_ << QVariantMap{{QStringLiteral("texto"), OpcionesPistas::nombreCodec(familia)}, {QStringLiteral("valor"), familia}};

    audios_.clear();
    for (const OpcionesPistas::FilaIdioma &fila : OpcionesPistas::filasAudio(oferta.audios.values()))
        audios_ << QVariantMap{{QStringLiteral("texto"), fila.texto}, {QStringLiteral("idioma"), fila.idioma}};

    subtitulos_.clear();
    secundarios_ = 0;
    for (const OpcionesPistas::FilaIdioma &fila : OpcionesPistas::filasSubtitulos(oferta.subtitulos.values())) {
        subtitulos_ << QVariantMap{{QStringLiteral("texto"), fila.texto}, {QStringLiteral("idioma"), fila.idioma},
                                   {QStringLiteral("secundario"), fila.secundario}};
        secundarios_ += fila.secundario;
    }
    emit cambio();
}

QString ModeloPistas::textoCodecVideo(const QString &familia, int alto) const
{
    return familia.isEmpty() ? tr("Cualquiera") : OpcionesPistas::textoCodecVideo(familia, alto, formatos_);
}

QString ModeloPistas::explicacion(int alto, const QString &vcodec) const
{
    return OpcionesPistas::explicacion(alto, vcodec, formatos_);
}

Eleccion ModeloPistas::eleccionDe(const QVariantMap &mapa)
{
    Eleccion eleccion;
    eleccion.audios = mapa.value(QStringLiteral("audios")).toStringList();
    eleccion.subtitulos = mapa.value(QStringLiteral("subtitulos")).toStringList();
    eleccion.calidad = mapa.value(QStringLiteral("calidad")).toInt();
    eleccion.vcodec = mapa.value(QStringLiteral("vcodec")).toString();
    eleccion.acodec = mapa.value(QStringLiteral("acodec")).toString();
    return eleccion;
}
