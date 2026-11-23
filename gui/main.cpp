// Android-Manager: descargas de videos y música con yt-dlp, respaldo del celular con rsync y, solo para
// Sebastián, las acciones del servidor (Servidor) y los servidores de Minecraft de su PC (Minecraft).
// La interfaz es QML con el estilo Material (como Ingresos y Gastos y el Gestor de Stock); la
// lógica está en Descargas, Respaldo, Herramientas y Sistema, que QML ve como objetos globales.

#include "ConfigServidor.h"
#include "Descargas.h"
#include "Fallas.h"
#include "Herramientas.h"
#include "Minecraft.h"
#include "ModeloApps.h"
#include "Novedades.h"
#include "Respaldo.h"
#include "Servidor.h"
#include "Sistema.h"
#include "Traducciones.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(Comun_UiPlugin)   // comun/ui/qml: tema, tarjetas, avisos y la página «Apps»

int main(int argc, char *argv[])
{
    // Sin esto, Qt frena la app al pasarla a segundo plano y las descargas y los respaldos se
    // quedarían esperando (nadie leería la salida de yt-dlp y rsync)
    qputenv("QT_BLOCK_EVENT_LOOPS_WHEN_SUSPENDED", "0");

    QGuiApplication app(argc, argv);
    apps::cargarTraduccionesQt();   // botones estándar en español (comun/apps; Android y Windows no traen la traducción)
    QGuiApplication::setApplicationName(QStringLiteral("Android-Manager"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Android-Manager"));
    QGuiApplication::setApplicationVersion(QStringLiteral(VERSION_PROGRAMA));
    QQuickStyle::setStyle(QStringLiteral("Material"));
    soporte::iniciarFallas();       // si se cierra por una falla, manda un informe (comun/soporte)

    Herramientas herramientas;
    Descargas videos(Descargas::Tipo::Videos, &herramientas);
    Descargas musica(Descargas::Tipo::Musica, &herramientas);
    Respaldo respaldo(&herramientas);
    Servidor servidor(&herramientas);    // sección Servidor (solo Sebastián: se activa a mano)
    Minecraft minecraft(&herramientas, &servidor);    // sección Minecraft (usa la clave de Servidor)
    Sistema sistema(&videos, &musica, &respaldo);
    // «Apps y actualizaciones» (comun/apps): versión nueva de esta app y las otras para instalar
    auto *modeloApps = new apps::ModeloApps(QStringLiteral("Android-Manager"), &app);   // vive más que el motor de QML

    QQmlApplicationEngine motor;
    QQmlContext *contexto = motor.rootContext();
    contexto->setContextProperty(QStringLiteral("Herramientas"), &herramientas);
    contexto->setContextProperty(QStringLiteral("Videos"), &videos);
    contexto->setContextProperty(QStringLiteral("Musica"), &musica);
    contexto->setContextProperty(QStringLiteral("Respaldo"), &respaldo);
    contexto->setContextProperty(QStringLiteral("Servidor"), &servidor);
    contexto->setContextProperty(QStringLiteral("Minecraft"), &minecraft);
    contexto->setContextProperty(QStringLiteral("Sistema"), &sistema);
    contexto->setContextProperty(QStringLiteral("Apps"), modeloApps);
    // Qué cambió en cada versión (se muestra sola al abrir una versión nueva) e informe de fallas (comun/soporte)
    contexto->setContextProperty(QStringLiteral("Novedades"), new soporte::Novedades(&app));
    QObject::connect(&motor, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        // La interfaz no cargó (falla de QML): se informa y recién después se sale
        soporte::Fallas::instancia().informarYSalir(QStringLiteral("qml"), QStringLiteral("No se pudo cargar Main.qml"), 1);
    }, Qt::QueuedConnection);
    motor.loadFromModule("AndroidManager", "Main");

    herramientas.preparar();
    // Datos del servidor (IPs, carpeta de respaldos) desde servidor.json, una vez por día
    ConfigServidor::instancia().actualizar();

    const int codigo = app.exec();
    sistema.cancelarTodo();    // si se sale con algo en curso, que no queden procesos sueltos
    servidor.cancelarYEsperar();
    minecraft.cancelarTodo();
    return codigo;
}
