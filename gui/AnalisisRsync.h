// Lo que el respaldo y la restauración leen de la salida de rsync y los argumentos que arman, sin nada de
// Android: así se prueba en Linux (tests/tst_analisis.cpp).
//
// Respaldo con versiones (el mismo criterio en los tres Manager): la copia espejo sigue igual
// (Respaldos/<persona>/<celular>/, con --delete), pero lo que rsync borraría o pisaría en el servidor no se
// pierde: --backup lo mueve a la carpeta hermana <celular>.anteriores/<AAAA-MM-DD_HH-MM>/, que queda fuera de
// lo que limpia --delete. Las versiones de más de 30 días se borran en la misma conexión (--rsync-path).

#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>

namespace analisisRsync {

constexpr int DiasAnteriores = 30;

// «2.00MB  99%  2.00GB/s 0:00:01 (xfr#1, to-chk=0/3)» → 99; -1 si la línea no es de avance
int porcentaje(const QString &linea);

// «backed up DCIM/foto.jpg to ../x.anteriores/…» (--info=backup1): un archivo que pasó a «anteriores»
bool esPasoAAnteriores(const QString &linea);

// Encabezados y estadísticas de rsync, que no son nombres de archivo
bool esEncabezado(const QString &linea);

// «<celular>.anteriores» y la carpeta de esta vez («2026-11-26_10-05»)
QString carpetaAnteriores(const QString &celular);
QString nombreVersion(const QDateTime &cuando);
bool esNombreVersion(const QString &nombre);
// «26/11/2026 10:05» para mostrar una versión («» si el nombre no es de una)
QString textoVersion(const QString &nombre);

// --backup-dir, relativo a la carpeta de destino: «--backup-dir=../<celular>.anteriores/<fecha>»
QString argumentoBackupDir(const QString &celular, const QDateTime &cuando);

// --rsync-path que, antes del rsync del servidor, borra las versiones de más de «dias» días de todas las
// carpetas *.anteriores de la persona. Va entre comillas simples y anda igual en bash y en fish (el shell
// del servidor). Vacío si la ruta tiene caracteres que no conviene meter en un comando: entonces el
// respaldo se hace igual, sin limpiar.
QString rsyncPathConLimpieza(const QString &carpetaPersona, int dias = DiasAnteriores);

// Un nombre literal dentro de un patrón de --include/--exclude (escapa * ? [ y \)
QString escaparPatron(const QString &nombre);

// Los --include/--exclude para listar (rsync --list-only -r) desde la carpeta de la persona solo lo que hace
// falta para restaurar: las carpetas de primer nivel del respaldo, las versiones y sus carpetas
QStringList filtrosListado(const QString &celular);

// Una línea de --list-only: «drwxr-xr-x          4,096 2026/11/26 10:00:00 celular/DCIM»
struct EntradaListado
{
    bool carpeta = false;
    QString ruta;
};
std::optional<EntradaListado> entradaListado(const QString &linea);

// Qué se puede restaurar: las carpetas del respaldo actual, las versiones (la más nueva primero) y las
// carpetas de cada versión
struct OpcionesRestauracion
{
    bool hayRespaldo = false;
    QStringList carpetasActual;
    QStringList versiones;
    QMap<QString, QStringList> carpetasVersion;
};
OpcionesRestauracion opcionesRestauracion(const QStringList &lineas, const QString &celular);

} // namespace analisisRsync
