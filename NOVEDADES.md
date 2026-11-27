# Novedades de Android-Manager

Lo que cambió en cada versión. La app lo muestra la primera vez que se abre una versión nueva y siempre desde
«Novedades» en el menú. Los cambios marcados [Sebastián] solo aparecen en sus equipos.

## 1.13.0 (26/11/2026)
- La app se cierra sola mucho menos: se arreglaron las causas de los cierres que llegaron en los informes
  (al pasar a segundo plano y al salir con algo en curso).
- Respaldo: lo que borraste o cambiaste en el celular ya no se pierde del servidor enseguida. Queda guardado
  30 días aparte («anteriores»), y al terminar te dice cuántos archivos fueron ahí.
- Respaldo: nuevo «Restaurar», para traer de vuelta al celular lo que está en el servidor: todo, una carpeta,
  o lo que un respaldo sacó en una fecha. No borra nada del celular y pregunta antes de empezar.
- Videos y Música: la lista de lo que bajaste se recuerda aunque cierres la app. Si algún enlace falló, aparece
  «Reintentar». Borrar ya no pregunta: tenés unos segundos para tocar «Deshacer».
- La notificación muestra qué se está bajando o respaldando y cuánto falta, y al terminar avisa si salió bien.
  Si Android no deja seguir en segundo plano, la app te lo avisa en vez de quedarse esperando.
- En el menú se explica por qué Videos, Música y Respaldo están apagados la primera vez (se están preparando).
- [Sebastián] Cierres: sin esperas en el hilo de la interfaz (clave de la sección Servidor con dropbearkey
  aparte; al salir se cortan los procesos con SIGKILL sin esperar 3 s cada uno) y QSG_RENDER_LOOP=basic contra
  el «Failed to acquire deadlock protector for QAndroidPlatformOpenGLWindow::eglSurface()» de Qt 6.10.0
  (QTBUG-140490/140674; lo arregla del todo Qt 6.10.2 o más nuevo). Sin el «Binding loop» de los diálogos.
- [Sebastián] Informes de fallas: en Android se le pregunta al sistema por qué terminó la app
  (ApplicationExitInfo): cerrarla desde Recientes, falta de memoria o una actualización ya no cuentan; un ANR
  o una excepción de Java sí. Servicio: onTimeout de Android 15 (dataSync, 6 h) y el error de empezarlo desde
  segundo plano. La versión de yt-dlp está abajo en el menú. Pruebas propias en tests/ (Compilar.sh y CI).

## 1.12.0 (24/11/2026)
- La app ocupa bastante menos en el celular (en el de prueba, de 305 MB a 224 MB): Python, ffmpeg y yt-dlp
  ya no se guardan dos veces al instalar, y quedaron afuera las partes que nunca se usaban. La primera vez
  que se abre después de actualizar tarda unos segundos más en prepararse.

## 1.11.2 (23/11/2026)
- [Sebastián] Minecraft: los servidores Fabric y Vanilla (los que se crean desde el Manager de la PC 4.14.0)
  se muestran como tales, no como Paper. En los Fabric y Forge, instalar un «plugin» pone el mod que le sirve.

## 1.11.1 (23/11/2026)
- [Sebastián] Minecraft: los grupos de servidores se despliegan al tocarlos (arrancan cerrados, salvo el que
  tenga uno encendido) y muestran cuántos servidores tienen y cuántos están encendidos.

## 1.11.0 (23/11/2026)
- [Sebastián] Minecraft: los servidores aparecen agrupados como en la carpeta nueva de la PC, y marcar un
  grupo marca todos sus servidores. Necesita el Manager de la PC 4.13.0 o más nuevo.

## 1.10.0 (23/11/2026)
- [Sebastián] Sección Minecraft (en el menú, con la sección Servidor activada): prender y apagar la PC desde
  cualquier lado, y sus servidores como en el Manager de la PC: iniciar, detener y reiniciar (avisando en el
  chat si hay gente), la consola en vivo con quién está conectado y para mandar comandos, Paper, plugins,
  RAM y reiniciar mundo.

## 1.9.0 (21/11/2026)
- Novedades: la primera vez que abrís una versión nueva te cuenta qué cambió. Siempre están en el menú.
- Si la app se cierra por una falla, manda un informe al servidor (sin tus datos) para que se pueda arreglar.
  También lo podés mandar a mano desde Novedades, con un comentario.
- Videos y Música: «Bajar con el servidor» (viene prendido y se recuerda). El servidor baja con yt-dlp y el
  celular trae los archivos terminados, a las mismas carpetas de siempre. Ya no hace falta un archivo de
  cookies. Sin servidor, se baja en el celular.
- [Sebastián] Al bajar con el servidor se usan las cookies del LibreWolf de la notebook: nunca llegan al celular.
- [Sebastián] Dejar apretado «Novedades» en el menú prende o apaga el modo Sebastián (ver también las novedades
  «solo vos»). Los informes de fallas se ven en http://100.101.213.44:8080/fallas/ (por Tailscale).

## 1.8.0 (19/11/2026)
- [Sebastián] Sección Servidor (dejar apretado el logo del menú): miniaturas de Bodrio y páginas del sitio,
  con una clave limitada.

## 1.7.0 (18/11/2026)
- Modo claro y oscuro con un botón (se recuerda).

## 1.6.0 (18/11/2026)
- Menú de 3 rayitas, con un puntito cuando hay una versión nueva.

## 1.5.0 (17/11/2026)
- Interfaz nueva, igual en todas las apps.
