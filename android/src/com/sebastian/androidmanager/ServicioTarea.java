// Servicio en primer plano mientras hay una descarga o un respaldo: sin esto, Android congela
// la app y sus procesos (yt-dlp, rsync) al poco tiempo de pasarla a segundo plano. Es lo único
// en Java porque Android exige que un servicio sea una clase Java; lo demás está en C++.
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
    private static final int ID_NOTIFICACION = 1;

    private PowerManager.WakeLock cpu;
    private WifiManager.WifiLock wifi;

    public static void iniciar(Context contexto, String texto) {
        contexto.startForegroundService(new Intent(contexto, ServicioTarea.class).putExtra("texto", texto));
    }

    public static void detener(Context contexto) {
        contexto.stopService(new Intent(contexto, ServicioTarea.class));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        getSystemService(NotificationManager.class).createNotificationChannel(
            new NotificationChannel(CANAL, "Descargas y respaldos", NotificationManager.IMPORTANCE_LOW));

        // Tocar la notificación vuelve a la app
        Intent abrir = getPackageManager().getLaunchIntentForPackage(getPackageName());
        PendingIntent alTocar = PendingIntent.getActivity(this, 0, abrir, PendingIntent.FLAG_IMMUTABLE);
        String texto = intent != null ? intent.getStringExtra("texto") : null;
        Notification notificacion = new Notification.Builder(this, CANAL)
            .setSmallIcon(android.R.drawable.stat_sys_download)
            .setContentTitle("Android-Manager")
            .setContentText(texto != null ? texto : "Tarea en curso")
            .setContentIntent(alTocar)
            .setOngoing(true)
            .build();
        startForeground(ID_NOTIFICACION, notificacion, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);

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

    @Override
    public void onDestroy() {
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
