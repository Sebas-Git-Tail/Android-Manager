// Servicio en primer plano mientras hay una descarga o un respaldo: sin esto, Android congela
// la app y sus procesos (yt-dlp, rsync) al poco tiempo de pasarla a segundo plano. Es lo único
// en Java porque Android exige que un servicio sea una clase Java; lo demás está en C++.
//
// La notificación muestra la tarea y cómo va (porcentaje), y al terminar queda un aviso «terminó» o
// «falló» (si la app no está a la vista). Desde Android 15 un servicio «dataSync» puede durar como mucho
// 6 horas por día: al cumplirse, el sistema llama a onTimeout() y el servicio se tiene que ir enseguida
// (si no, cierra la app); se avisa con una notificación y la app lo cuenta al volver (tomarVencido()).
package com.sebastian.androidmanager;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.net.wifi.WifiManager;
import android.os.IBinder;
import android.os.PowerManager;

public class ServicioTarea extends Service {
    private static final String CANAL = "tareas";
    private static final String CANAL_AVISOS = "avisos";
    private static final int ID_NOTIFICACION = 1;

    // Lo que muestra la notificación (lo cambia actualizar() desde C++)
    private static volatile String titulo = "Android-Manager";
    private static volatile String texto = "Tarea en curso";
    private static volatile int progreso = -1;
    private static volatile boolean activo = false;
    private static volatile boolean vencido = false;

    private PowerManager.WakeLock cpu;
    private WifiManager.WifiLock wifi;

    // "" si arrancó; si no, por qué (ForegroundServiceStartNotAllowedException: Android no deja empezar
    // un servicio en primer plano con la app en segundo plano)
    public static String iniciar(Context contexto, String texto) {
        ServicioTarea.texto = texto;
        progreso = -1;
        try {
            contexto.startForegroundService(new Intent(contexto, ServicioTarea.class));
            return "";
        } catch (Exception e) {
            return e.getClass().getSimpleName();
        }
    }

    public static void detener(Context contexto) {
        contexto.stopService(new Intent(contexto, ServicioTarea.class));
    }

    // Cambia lo que dice la notificación (progreso -1: sin porcentaje)
    public static void actualizar(Context contexto, String titulo, String texto, int progreso) {
        ServicioTarea.titulo = titulo;
        ServicioTarea.texto = texto;
        ServicioTarea.progreso = progreso;
        if (!activo)
            return;
        try {
            contexto.getSystemService(NotificationManager.class).notify(ID_NOTIFICACION, construir(contexto));
        } catch (Exception e) {
            // sin permiso de notificaciones, o el sistema no deja: la tarea sigue igual
        }
    }

    // Aviso que queda al terminar una tarea («terminó», «falló»); id distinto por tarea
    public static void avisarFin(Context contexto, int id, String titulo, String texto) {
        try {
            crearCanales(contexto);
            Notification aviso = new Notification.Builder(contexto, CANAL_AVISOS)
                .setSmallIcon(android.R.drawable.stat_sys_download_done)
                .setContentTitle(titulo)
                .setContentText(texto)
                .setStyle(new Notification.BigTextStyle().bigText(texto))
                .setContentIntent(abrirApp(contexto))
                .setAutoCancel(true)
                .build();
            contexto.getSystemService(NotificationManager.class).notify(id, aviso);
        } catch (Exception e) {
            // sin permiso de notificaciones: no se avisa
        }
    }

    // Si el sistema cortó el servicio por tiempo desde la última vez que se preguntó
    public static boolean tomarVencido() {
        boolean v = vencido;
        vencido = false;
        return v;
    }

    private static void crearCanales(Context contexto) {
        NotificationManager gestor = contexto.getSystemService(NotificationManager.class);
        gestor.createNotificationChannel(new NotificationChannel(CANAL, "Descargas y respaldos", NotificationManager.IMPORTANCE_LOW));
        gestor.createNotificationChannel(new NotificationChannel(CANAL_AVISOS, "Tareas terminadas", NotificationManager.IMPORTANCE_DEFAULT));
    }

    // Tocar la notificación vuelve a la app
    private static PendingIntent abrirApp(Context contexto) {
        Intent abrir = contexto.getPackageManager().getLaunchIntentForPackage(contexto.getPackageName());
        return PendingIntent.getActivity(contexto, 0, abrir, PendingIntent.FLAG_IMMUTABLE);
    }

    private static Notification construir(Context contexto) {
        Notification.Builder b = new Notification.Builder(contexto, CANAL)
            .setSmallIcon(android.R.drawable.stat_sys_download)
            .setContentTitle(titulo)
            .setContentText(texto)
            .setContentIntent(abrirApp(contexto))
            .setOnlyAlertOnce(true)
            .setOngoing(true);
        int p = progreso;
        b.setProgress(100, Math.max(0, Math.min(100, p)), p < 0);
        return b.build();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        crearCanales(this);
        try {
            startForeground(ID_NOTIFICACION, construir(this), ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        } catch (Exception e) {
            // Android 15: ya se usaron las 6 horas del día (o no deja por otro motivo). Sin servicio, la
            // tarea sigue solo mientras la app esté a la vista; la app lo avisa
            vencido = true;
            stopSelf();
            return START_NOT_STICKY;
        }
        activo = true;

        // Con la pantalla apagada, que no se duerman ni el procesador ni el Wi-Fi
        if (cpu == null) {
            cpu = getSystemService(PowerManager.class).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "AndroidManager:tarea");
            cpu.acquire();
            wifi = getSystemService(WifiManager.class).createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "AndroidManager:tarea");
            wifi.acquire();
        }
        // Si el sistema mata la app, no tiene sentido revivir el servicio solo
        return START_NOT_STICKY;
    }

    // Android 15+: se terminó el tiempo de los servicios «dataSync» (6 horas por día). Hay que irse ya.
    // Sin @Override: el SDK con el que compila Qt puede ser anterior a Android 15
    public void onTimeout(int startId, int tipo) {
        vencido = true;
        avisarFin(this, 9, "Android-Manager",
                  "Android frenó la tarea en segundo plano (llegó al límite de horas por día). Abrí la app para que siga.");
        stopForeground(STOP_FOREGROUND_REMOVE);
        stopSelf();
    }

    @Override
    public void onDestroy() {
        activo = false;
        if (cpu != null)
            cpu.release();
        if (wifi != null)
            wifi.release();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
