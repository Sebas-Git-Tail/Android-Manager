// Recordatorio de respaldo: si pasan varios días sin respaldar el celular, aparece una notificación
// que al tocarla abre la app en la página Respaldo. El respaldo en sí no puede ser automático: la
// contraseña del servidor se escribe cada vez y no se guarda en ningún lado.
// La alarma la programa la app después de cada respaldo (Plataforma::programarRecordatorio) y se
// vuelve a programar sola si el celular se reinicia.
package com.sebastian.androidmanager;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

public class Recordatorio extends BroadcastReceiver {
    private static final String CANAL = "recordatorios";
    private static final int ID_NOTIFICACION = 2;
    private static final String AJUSTES = "recordatorio";
    private static final long DIA = 24L * 60 * 60 * 1000;

    // ultimoRespaldo: en milisegundos desde 1970. dias: 0 desactiva el recordatorio.
    public static void programar(Context contexto, long ultimoRespaldo, int dias) {
        contexto.getSharedPreferences(AJUSTES, Context.MODE_PRIVATE).edit()
            .putLong("ultimo", ultimoRespaldo).putLong("aviso", ultimoRespaldo + dias * DIA)
            .putInt("dias", dias).apply();
        reprogramar(contexto);
    }

    private static PendingIntent alarma(Context contexto) {
        return PendingIntent.getBroadcast(contexto, 0, new Intent(contexto, Recordatorio.class),
                                          PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private static void reprogramar(Context contexto) {
        SharedPreferences ajustes = contexto.getSharedPreferences(AJUSTES, Context.MODE_PRIVATE);
        long aviso = ajustes.getLong("aviso", 0);
        int dias = ajustes.getInt("dias", 0);
        AlarmManager alarmas = contexto.getSystemService(AlarmManager.class);
        alarmas.cancel(alarma(contexto));
        if (aviso <= 0 || dias <= 0)
            return;
        // Si ya pasó, en un minuto. Inexacta: no hace falta el permiso de alarmas exactas
        long cuando = Math.max(aviso, System.currentTimeMillis() + 60 * 1000);
        alarmas.set(AlarmManager.RTC, cuando, alarma(contexto));
    }

    @Override
    public void onReceive(Context contexto, Intent intent) {
        if (Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
            reprogramar(contexto);
            return;
        }
        SharedPreferences ajustes = contexto.getSharedPreferences(AJUSTES, Context.MODE_PRIVATE);
        long ultimo = ajustes.getLong("ultimo", 0);
        if (ultimo <= 0)
            return;
        long dias = (System.currentTimeMillis() - ultimo) / DIA;

        NotificationManager notificaciones = contexto.getSystemService(NotificationManager.class);
        notificaciones.createNotificationChannel(
            new NotificationChannel(CANAL, "Recordatorio de respaldo", NotificationManager.IMPORTANCE_DEFAULT));
        Intent abrir = contexto.getPackageManager().getLaunchIntentForPackage(contexto.getPackageName());
        abrir.putExtra("pagina", "respaldo");
        PendingIntent alTocar = PendingIntent.getActivity(contexto, 1, abrir,
                                                          PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Notification notificacion = new Notification.Builder(contexto, CANAL)
            .setSmallIcon(android.R.drawable.stat_notify_sync)
            .setContentTitle("Respaldo del celular")
            .setContentText("Hace " + dias + " días que no respaldás el celular. Tocá para respaldar.")
            .setContentIntent(alTocar)
            .setAutoCancel(true)
            .build();
        notificaciones.notify(ID_NOTIFICACION, notificacion);
        // Si sigue sin respaldar, vuelve a avisar en 3 días
        ajustes.edit().putLong("aviso", System.currentTimeMillis() + 3 * DIA).apply();
        reprogramar(contexto);
    }
}
