// Pruebas de lo que la app lee y arma sin Android: salida de rsync (respaldo, versiones, restaurar),
// respuestas del servidor de descargas, Minecraft e historial de descargas.

#include "AnalisisMinecraft.h"
#include "AnalisisRsync.h"
#include "EstadoRemoto.h"
#include "HistorialDescargas.h"

#include <QJsonDocument>
#include <QTest>

class PruebaAnalisis : public QObject
{
    Q_OBJECT

private slots:
    // ── rsync ──
    void porcentaje()
    {
        QCOMPARE(analisisRsync::porcentaje(QStringLiteral("2.00MB  99%  2.00GB/s 0:00:01 (xfr#1, to-chk=0/3)")), 99);
        QCOMPARE(analisisRsync::porcentaje(QStringLiteral("13B 100%   8,00KB/s    0:00:00")), 100);   // ya sin los espacios de adelante (procesarLinea recorta)
        QCOMPARE(analisisRsync::porcentaje(QStringLiteral("99% seguro.txt")), -1);       // un archivo, no el avance
        QCOMPARE(analisisRsync::porcentaje(QStringLiteral("DCIM/foto 50% .jpg")), -1);
        QCOMPARE(analisisRsync::porcentaje(QString()), -1);
    }

    void lineas()
    {
        QVERIFY(analisisRsync::esPasoAAnteriores(
            QStringLiteral("backed up DCIM/a.jpg to ../Galaxy S22.anteriores/2026-11-26_11-00/DCIM/a.jpg")));
        QVERIFY(!analisisRsync::esPasoAAnteriores(QStringLiteral("DCIM/backed up.jpg")));
        QVERIFY(analisisRsync::esEncabezado(QStringLiteral("sending incremental file list")));
        QVERIFY(analisisRsync::esEncabezado(QStringLiteral("receiving incremental file list")));
        QVERIFY(analisisRsync::esEncabezado(QStringLiteral("sent 1,234 bytes  received 56 bytes")));
        QVERIFY(analisisRsync::esEncabezado(QStringLiteral("./")));
        QVERIFY(!analisisRsync::esEncabezado(QStringLiteral("DCIM/a.jpg")));
    }

    void versiones()
    {
        const QDateTime cuando(QDate(2026, 11, 26), QTime(9, 5, 33));
        QCOMPARE(analisisRsync::nombreVersion(cuando), QStringLiteral("2026-11-26_09-05"));
        QVERIFY(analisisRsync::esNombreVersion(QStringLiteral("2026-11-26_09-05")));
        QVERIFY(!analisisRsync::esNombreVersion(QStringLiteral("2026-11-26")));
        QVERIFY(!analisisRsync::esNombreVersion(QStringLiteral("../2026-11-26_09-05")));
        QCOMPARE(analisisRsync::textoVersion(QStringLiteral("2026-11-26_09-05")), QStringLiteral("26/11/2026 09:05"));
        QCOMPARE(analisisRsync::textoVersion(QStringLiteral("cualquiera")), QString());
        QCOMPARE(analisisRsync::carpetaAnteriores(QStringLiteral("Galaxy S22")), QStringLiteral("Galaxy S22.anteriores"));
        // Relativo a la carpeta del respaldo: queda al lado, fuera de lo que limpia --delete
        QCOMPARE(analisisRsync::argumentoBackupDir(QStringLiteral("Galaxy S22"), cuando),
                 QStringLiteral("--backup-dir=../Galaxy S22.anteriores/2026-11-26_09-05"));
    }

    void limpiezaDeVersiones()
    {
        const QString r = analisisRsync::rsyncPathConLimpieza(QStringLiteral("/home/sebastian/Respaldos/juan/"));
        QVERIFY(r.startsWith(QStringLiteral("find '/home/sebastian/Respaldos/juan' -mindepth 2 -maxdepth 2 -type d")));
        QVERIFY(r.contains(QStringLiteral("-path '*.anteriores/*'")));
        QVERIFY(r.contains(QStringLiteral("-mtime +30")));
        QVERIFY(r.endsWith(QStringLiteral("; rsync")));
        // Nada que el shell pueda interpretar: sin limpieza (el respaldo se hace igual)
        QCOMPARE(analisisRsync::rsyncPathConLimpieza(QStringLiteral("/home/x/Respaldos/ju'an/")), QString());
        QCOMPARE(analisisRsync::rsyncPathConLimpieza(QStringLiteral("/home/x/Respaldos/$(rm)/")), QString());
        QCOMPARE(analisisRsync::rsyncPathConLimpieza(QStringLiteral("/home/x/../")), QString());
        QCOMPARE(analisisRsync::rsyncPathConLimpieza(QStringLiteral("Respaldos/juan/")), QString());
        QCOMPARE(analisisRsync::rsyncPathConLimpieza(QStringLiteral("/home/x/Respaldos/juan/"), 0), QString());
    }

    void filtros()
    {
        QCOMPARE(analisisRsync::escaparPatron(QStringLiteral("Cel [2]*?")), QStringLiteral("Cel \\[2]\\*\\?"));
        const QStringList f = analisisRsync::filtrosListado(QStringLiteral("Galaxy S22"));
        QCOMPARE(f.first(), QStringLiteral("--include=/Galaxy S22/"));
        QVERIFY(f.contains(QStringLiteral("--include=/Galaxy S22.anteriores/*/*/")));
        QCOMPARE(f.last(), QStringLiteral("--exclude=*"));
    }

    void listado()
    {
        // Lo que devolvió rsync --list-only en la prueba real (rsync 3.5.2)
        const QStringList lineas{
            QStringLiteral("drwxr-xr-x             62 2026/11/26 22:51:05 ."),
            QStringLiteral("drwxr-xr-x             32 2026/11/26 22:51:19 Galaxy S22.anteriores"),
            QStringLiteral("drwxr-xr-x             38 2026/11/26 22:51:05 Galaxy S22.anteriores/2026-11-26_11-00"),
            QStringLiteral("drwxr-xr-x             10 2026/11/26 22:51:05 Galaxy S22.anteriores/2026-11-26_11-00/DCIM"),
            QStringLiteral("drwxr-xr-x          4,096 2026/11/27 08:00:00 Galaxy S22.anteriores/2026-11-27_08-00"),
            QStringLiteral("drwxr-xr-x          4.096 2026/11/27 08:00:00 Galaxy S22.anteriores/no-es-version"),
            QStringLiteral("drwxr-xr-x             22 2026/11/26 22:51:05 Galaxy S22"),
            QStringLiteral("drwxr-xr-x             20 2026/11/26 22:51:19 Galaxy S22/DCIM"),
            QStringLiteral("drwxr-xr-x             12 2026/11/26 22:51:05 Galaxy S22/Música"),
            QStringLiteral("drwxr-xr-x             12 2026/11/26 22:51:05 Galaxy S22/Con espacios y ñ"),
            QStringLiteral("-rw-r--r--              5 2026/11/26 22:51:05 Galaxy S22/suelto.txt"),
            QStringLiteral("drwxr-xr-x             12 2026/11/26 22:51:05 Otro celular/DCIM"),
            QStringLiteral("rsync: algo raro"),
        };
        const auto e = analisisRsync::entradaListado(lineas.at(9));
        QVERIFY(e && e->carpeta);
        QCOMPARE(e->ruta, QStringLiteral("Galaxy S22/Con espacios y ñ"));
        QVERIFY(!analisisRsync::entradaListado(QStringLiteral("rsync: algo raro")));

        const auto o = analisisRsync::opcionesRestauracion(lineas, QStringLiteral("Galaxy S22"));
        QVERIFY(o.hayRespaldo);
        QCOMPARE(o.carpetasActual, (QStringList{QStringLiteral("Con espacios y ñ"), QStringLiteral("DCIM"), QStringLiteral("Música")}));
        QCOMPARE(o.versiones, (QStringList{QStringLiteral("2026-11-27_08-00"), QStringLiteral("2026-11-26_11-00")}));
        QCOMPARE(o.carpetasVersion.value(QStringLiteral("2026-11-26_11-00")), QStringList{QStringLiteral("DCIM")});
        QVERIFY(o.carpetasVersion.value(QStringLiteral("2026-11-27_08-00")).isEmpty());

        QVERIFY(!analisisRsync::opcionesRestauracion(lineas, QStringLiteral("Nada")).hayRespaldo);
    }

    // ── Servidor de descargas ──
    void estadoRemoto()
    {
        const QByteArray json = R"({"estado": "descargando", "actual": "Video.mkv", "item": "2 de 3", "porcentaje": 40,
            "urls": [{"url": "u1", "estado": "listo"}, {"url": "u2", "estado": "error", "error": "Privado"},
                     {"url": "u3", "estado": "parcial", "error": "Uno falló"}, {"url": "u4", "estado": "descargando"}],
            "carpetas": [{"nombre": "Canal", "archivos": [{"nombre": "Video.mkv", "tamano": 1234, "miniatura": true},
                                                          {"nombre": "", "tamano": 1}]},
                         {"nombre": "Otro", "archivos": [{"nombre": "b.opus", "tamano": 5.0}]}]})";
        const auto e = trabajoServidor::leer(QJsonDocument::fromJson(json).object());
        QCOMPARE(e.estado, QStringLiteral("descargando"));
        QCOMPARE(e.actual, QStringLiteral("Video.mkv"));
        QCOMPARE(e.item, QStringLiteral("2 de 3"));
        QCOMPARE(e.porcentaje, 40);
        QCOMPARE(e.hechasUrls, 3);
        QCOMPARE(e.sinTerminar, 1);
        QCOMPARE(e.fallidas.size(), 2);
        QCOMPARE(e.fallidas.at(0).url, QStringLiteral("u2"));
        QCOMPARE(e.fallidas.at(0).error, QStringLiteral("Privado"));
        QCOMPARE(e.archivos.size(), 2);     // el de nombre vacío no cuenta
        QCOMPARE(e.archivos.at(0).carpeta, QStringLiteral("Canal"));
        QCOMPARE(e.archivos.at(0).tamano, qint64(1234));
        QVERIFY(e.archivos.at(0).miniatura);
        QVERIFY(!e.archivos.at(1).miniatura);

        const auto vacio = trabajoServidor::leer({});
        QVERIFY(vacio.estado.isEmpty());
        QCOMPARE(vacio.hechasUrls, 0);
        QVERIFY(vacio.archivos.isEmpty());
    }

    void terminadoYNombres()
    {
        for (const char *e : {"listo", "error", "cancelado", "perdido"})
            QVERIFY(trabajoServidor::terminado(QString::fromLatin1(e)));
        QVERIFY(!trabajoServidor::terminado(QStringLiteral("en cola")));
        QVERIFY(!trabajoServidor::terminado(QStringLiteral("descargando")));
        QCOMPARE(trabajoServidor::nombreSeguro(QStringLiteral("¿Qué? «Hola»: a/b|c*.mkv")), QStringLiteral("¿Qué_ «Hola»_ a_b_c_.mkv"));
        QCOMPARE(trabajoServidor::nombreSeguro(QStringLiteral("..")), QStringLiteral("_"));
        QCOMPARE(trabajoServidor::nombreSeguro(QStringLiteral("  ")), QStringLiteral("_"));
    }

    // ── Minecraft ──
    void minecraftServidor()
    {
        const QDate hoy(2026, 11, 26);
        const auto s = analisisMinecraft::servidor(
            QStringLiteral("ENCENDIDO\tAmigos/26.1.2/SMP-1\tfabric\t26.1.2\t1.2 GB\t2026-11-26\t4\tfabric.jar"), hoy);
        QVERIFY(s);
        QCOMPARE(s->value(QStringLiteral("titulo")).toString(), QStringLiteral("SMP-1"));
        QCOMPARE(s->value(QStringLiteral("grupo")).toString(), QStringLiteral("Amigos"));
        QCOMPARE(s->value(QStringLiteral("tipo")).toString(), QStringLiteral("Fabric"));
        QCOMPARE(s->value(QStringLiteral("ultimo")).toString(), QStringLiteral("hoy"));
        QVERIFY(s->value(QStringLiteral("encendido")).toBool());
        const auto viejo = analisisMinecraft::servidor(QStringLiteral("APAGADO\tSuelto\t\t1.21\t3 GB\t2026-01-02\t2\tpaper.jar"), hoy);
        QVERIFY(viejo);
        QCOMPARE(viejo->value(QStringLiteral("tipo")).toString(), QStringLiteral("Paper"));     // sin tipo: los viejos
        QCOMPARE(viejo->value(QStringLiteral("grupo")).toString(), QString());
        QCOMPARE(viejo->value(QStringLiteral("ultimo")).toString(), QStringLiteral("02/01/2026"));
        QVERIFY(!viejo->value(QStringLiteral("encendido")).toBool());
        QVERIFY(!analisisMinecraft::servidor(QStringLiteral("ENCENDIDO\tincompleto")));
    }

    void minecraftJugadoresYPlugins()
    {
        auto j = analisisMinecraft::jugadores(QStringLiteral("0\t20\t"));
        QVERIFY(j);
        QCOMPARE(j->first, 0);
        QCOMPARE(j->second, QStringLiteral("Nadie conectado (entran hasta 20)."));
        j = analisisMinecraft::jugadores(QStringLiteral("3\t20\tAna,Beto"));
        QCOMPARE(j->first, 3);
        QCOMPARE(j->second, QStringLiteral("Conectados (3 de 20): Ana, Beto y 1 más"));
        QVERIFY(!analisisMinecraft::jugadores(QStringLiteral("3\t20")));

        const auto p = analisisMinecraft::plugin(QStringLiteral("EssentialsX.jar\tEssentialsX\t2.20\t2.21\tNUEVA\textra"));
        QVERIFY(p);
        QCOMPARE(p->value(QStringLiteral("estado")).toString(), QStringLiteral("NUEVA"));
        QCOMPARE(p->value(QStringLiteral("nueva")).toString(), QStringLiteral("2.21"));
        QVERIFY(!analisisMinecraft::plugin(QStringLiteral("a\tb")));
    }

    void minecraftPedidos()
    {
        const QString pedido = analisisMinecraft::pedido({QStringLiteral("comando"), QStringLiteral("26.1.2 Claudia"),
                                                          QStringLiteral("say «hola» 'che'")});
        QVERIFY(pedido.startsWith(QStringLiteral("minecraft ")));
        QCOMPARE(QString::fromUtf8(QByteArray::fromBase64(pedido.mid(10).toLatin1())),
                 QStringLiteral("comando\n26.1.2 Claudia\nsay «hola» 'che'"));
        QCOMPARE(analisisMinecraft::accionDe(pedido), QStringLiteral("comando"));
        QCOMPARE(analisisMinecraft::accionDe(QStringLiteral("pc-prender")), QStringLiteral("pc-prender"));
        QCOMPARE(analisisMinecraft::partir(QStringLiteral("LINEA hola mundo")),
                 (std::pair{QStringLiteral("LINEA"), QStringLiteral("hola mundo")}));
        QCOMPARE(analisisMinecraft::partir(QStringLiteral("LATIDO")), (std::pair{QStringLiteral("LATIDO"), QString()}));
        QCOMPARE(analisisMinecraft::etiqueta(QStringLiteral("Amigos/26.1.2/SMP-1")), QStringLiteral("SMP-1 (Amigos)"));
        QCOMPARE(analisisMinecraft::etiqueta(QStringLiteral("Suelto")), QStringLiteral("Suelto"));
    }

    // ── Historial de descargas ──
    void historial()
    {
        historialDescargas::Historial h;
        h.entradas << historialDescargas::Entrada{QStringLiteral("Video"), QStringLiteral("/sdcard/Movies/Yt-dlp/Canal/Video.mkv"),
                                                  QStringLiteral("/datos/miniaturas-videos/1-1.jpg")};
        h.entradas << historialDescargas::Entrada{QStringLiteral("Artista — Tema"), QStringLiteral("/sdcard/Music/Artista/Tema.opus"), {}};
        h.fallidas << QStringLiteral("https://youtu.be/x");
        h.papelera << QStringLiteral("/sdcard/Music/Artista/.Otro.opus.borrando");
        const auto vuelta = historialDescargas::cargar(historialDescargas::guardar(h));
        QCOMPARE(vuelta.entradas, h.entradas);
        QCOMPARE(vuelta.fallidas, h.fallidas);
        QCOMPARE(vuelta.papelera, h.papelera);

        // Lo roto o lo que no escondió la app se ignora (nunca borrar un archivo cualquiera)
        const auto raro = historialDescargas::cargar(
            R"({"descargados": [{"nombre": "sin ruta"}, 3], "fallidas": ["  ", "u"], "papelera": ["/sdcard/DCIM/foto.jpg"]})");
        QVERIFY(raro.entradas.isEmpty());
        QCOMPARE(raro.fallidas, QStringList{QStringLiteral("u")});
        QVERIFY(raro.papelera.isEmpty());
        QVERIFY(historialDescargas::cargar("no es json").entradas.isEmpty());

        // Como mucho MaxEntradas
        historialDescargas::Historial muchos;
        for (int i = 0; i < historialDescargas::MaxEntradas + 10; ++i)
            muchos.entradas << historialDescargas::Entrada{QString::number(i), QStringLiteral("/a/%1").arg(i), {}};
        QCOMPARE(historialDescargas::cargar(historialDescargas::guardar(muchos)).entradas.size(), historialDescargas::MaxEntradas);

        QCOMPARE(historialDescargas::rutaEscondida(QStringLiteral("/sdcard/Movies/Canal/Video 1.mkv")),
                 QStringLiteral("/sdcard/Movies/Canal/.Video 1.mkv.borrando"));
    }
};

QTEST_GUILESS_MAIN(PruebaAnalisis)
#include "tst_analisis.moc"
