// Carpeta de datos de la app: el entorno de Python y ffmpeg, los ajustes, la marca de yt-dlp.
// En Android es la carpeta «files» de la app. Antes se usaba QDir::homePath(), que en Android
// apuntaba a esa misma carpeta (Qt ponía HOME ahí), pero desde Android 17 la app ya no recibe
// HOME y daba «/»: la app no podía preparar las herramientas ni guardar los ajustes.

#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

inline QString carpetaDatos()
{
    static const QString carpeta = [] {
        QString ruta = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (ruta.isEmpty())
            ruta = QDir::homePath();
        QDir().mkpath(ruta);
        return ruta;
    }();
    return carpeta;
}

// Los ajustes de la app (nombres del respaldo, fecha del último, destino confirmado)
inline QString archivoAjustes()
{
    return QDir(carpetaDatos()).filePath(QStringLiteral("Android-Manager.ini"));
}
