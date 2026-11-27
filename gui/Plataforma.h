// Lo que hay que pedirle directamente a Android (por JNI).

#pragma once

#include <QString>

namespace Plataforma {

// Acceso a todos los archivos (hace falta para Movies, Music y para respaldar /sdcard)
bool tieneAccesoArchivos();
void pedirAccesoArchivos();    // abre la pantalla de Ajustes de esta app

// Permiso para mostrar la notificación de la tarea en curso (si se niega, igual funciona)
void pedirNotificaciones();

// El nombre del dispositivo en Ajustes (Galaxy S22). Si no hay, el modelo, que en algunas marcas
// es solo un código (SM-S901E).
QString nombreDispositivo();

// Avisa al sistema de un archivo nuevo, para que aparezca en la galería y los reproductores
void avisarArchivoNuevo(const QString &ruta);

// Servicio en primer plano: mantiene viva la app mientras hay una tarea. Devuelve "" si arrancó, o por qué
// no (Android no deja empezarlo con la app en segundo plano)
QString iniciarServicio(const QString &texto);
void detenerServicio();
void actualizarServicio(const QString &titulo, const QString &texto, int progreso);   // progreso -1: sin %
void avisarFinTarea(int id, const QString &titulo, const QString &texto);             // queda en las notificaciones
bool servicioVencido();     // Android cortó el servicio por tiempo (Android 15: 6 horas por día)

// Recordatorio: una notificación si pasan «dias» desde el último respaldo (0 = no avisar)
void programarRecordatorio(qint64 ultimoRespaldoMs, int dias);

// Qué página pidió abrir la notificación que abrió la app («respaldo»), o vacío
QString paginaPedida();

}
