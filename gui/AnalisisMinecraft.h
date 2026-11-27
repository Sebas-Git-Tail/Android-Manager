// Lo que la sección Minecraft lee de las respuestas del servidor (los mismos eventos que la ventana de
// Linux: SERVIDOR, JUGADORES, PLUGIN…) y cómo arma los pedidos, sin nada de Android ni de procesos: así se
// prueba en Linux (tests/tst_analisis.cpp).

#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>
#include <utility>

namespace analisisMinecraft {

// «PALABRA resto» → {PALABRA, resto}
std::pair<QString, QString> partir(const QString &linea);

// Los argumentos viajan en base64, separados por saltos de línea: «minecraft <base64>»
QString pedido(const QStringList &argumentos);
// La primera palabra de un pedido («iniciar», «pc-prender»…)
QString accionDe(const QString &pedido);

// «Amigos/26.1.2/SMP-1» → «SMP-1 (Amigos)»
QString etiqueta(const QString &servidor);

// SERVIDOR estado<tab>nombre<tab>tipo<tab>versión<tab>tamaño<tab>último uso<tab>RAM<tab>jar
// (lo de después de «SERVIDOR ») → lo que muestra la lista; nada si la línea está incompleta
std::optional<QVariantMap> servidor(const QString &resto, const QDate &hoy = QDate::currentDate());

// JUGADORES conectados<tab>máximo<tab>nombre,nombre… → {conectados, texto}
std::optional<std::pair<int, QString>> jugadores(const QString &resto);

// PLUGIN archivo<tab>nombre<tab>versión<tab>nueva<tab>estado<tab>…
std::optional<QVariantMap> plugin(const QString &resto);

} // namespace analisisMinecraft
