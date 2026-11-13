#include "Plataforma.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>

namespace {

QJniObject contexto()
{
    return QJniObject(QNativeInterface::QAndroidApplication::context());
}

QJniObject cadena(const QString &texto)
{
    return QJniObject::fromString(texto);
}

// new String[]{texto}
jobjectArray arregloDeUna(const QString &texto)
{
    QJniEnvironment entorno;
    jclass claseString = entorno.findClass("java/lang/String");
    return entorno->NewObjectArray(1, claseString, cadena(texto).object<jstring>());
}

} // namespace

namespace Plataforma {

bool tieneAccesoArchivos()
{
    return QJniObject::callStaticMethod<jboolean>("android/os/Environment", "isExternalStorageManager", "()Z");
}

void pedirAccesoArchivos()
{
    const QJniObject paquete = contexto().callObjectMethod("getPackageName", "()Ljava/lang/String;");
    const QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        cadena(QStringLiteral("package:") + paquete.toString()).object<jstring>());
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;Landroid/net/Uri;)V",
                      cadena(QStringLiteral("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION")).object<jstring>(),
                      uri.object());
    contexto().callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
}

void pedirNotificaciones()
{
    contexto().callMethod<void>("requestPermissions", "([Ljava/lang/String;I)V",
                                arregloDeUna(QStringLiteral("android.permission.POST_NOTIFICATIONS")), jint(1));
    QJniEnvironment().checkAndClearExceptions();
}

QString nombreDispositivo()
{
    const QJniObject resolver = contexto().callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    const QString nombre = QJniObject::callStaticObjectMethod(
        "android/provider/Settings$Global", "getString",
        "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
        resolver.object(), cadena(QStringLiteral("device_name")).object<jstring>()).toString().trimmed();
    QJniEnvironment().checkAndClearExceptions();
    if (!nombre.isEmpty())
        return nombre;
    return QJniObject::getStaticObjectField("android/os/Build", "MODEL", "Ljava/lang/String;").toString();
}

void avisarArchivoNuevo(const QString &ruta)
{
    QJniObject::callStaticMethod<void>(
        "android/media/MediaScannerConnection", "scanFile",
        "(Landroid/content/Context;[Ljava/lang/String;[Ljava/lang/String;Landroid/media/MediaScannerConnection$OnScanCompletedListener;)V",
        contexto().object(), arregloDeUna(ruta), nullptr, nullptr);
}

void iniciarServicio(const QString &texto)
{
    QJniObject::callStaticMethod<void>("com/sebastian/androidmanager/ServicioTarea", "iniciar",
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       contexto().object(), cadena(texto).object<jstring>());
    QJniEnvironment().checkAndClearExceptions();
}

void programarRecordatorio(qint64 ultimoRespaldoMs, int dias)
{
    QJniObject::callStaticMethod<void>("com/sebastian/androidmanager/Recordatorio", "programar",
                                       "(Landroid/content/Context;JI)V", contexto().object(),
                                       jlong(ultimoRespaldoMs), jint(dias));
    QJniEnvironment().checkAndClearExceptions();
}

QString paginaPedida()
{
    const QJniObject actividad = QJniObject(QNativeInterface::QAndroidApplication::context());
    const QJniObject intent = actividad.callObjectMethod("getIntent", "()Landroid/content/Intent;");
    QString pagina;
    if (intent.isValid())
        pagina = intent.callObjectMethod("getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;",
                                         cadena(QStringLiteral("pagina")).object<jstring>()).toString();
    QJniEnvironment().checkAndClearExceptions();
    return pagina;
}

void detenerServicio()
{
    QJniObject::callStaticMethod<void>("com/sebastian/androidmanager/ServicioTarea", "detener",
                                       "(Landroid/content/Context;)V", contexto().object());
    QJniEnvironment().checkAndClearExceptions();
}

}
