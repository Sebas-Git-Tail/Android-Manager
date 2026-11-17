// «Descargar Vídeos» y «Descargar Música»: la misma lógica con dos modos, sin interfaz (la
// página es PaginaDescargas.qml). Hace lo mismo que Logica/Descargar.sh en el Manager de Linux:
// las mismas opciones de yt-dlp y una URL por vez, leyendo el avance de su salida.

#pragma once

#include "ConsultaVideo.h"
#include "Herramientas.h"
#include "ModeloPistas.h"

#include <QAbstractListModel>
#include <QProcess>
#include <QStringList>

// Lo que se bajó en esta sesión: nombre, ruta del archivo y su miniatura
class ModeloDescargados : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int cantidad READ rowCount NOTIFY cantidadCambio)

public:
    enum Rol { RolNombre = Qt::UserRole + 1, RolRuta, RolMiniatura };
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &padre = {}) const override { return padre.isValid() ? 0 : int(filas.size()); }
    QVariant data(const QModelIndex &indice, int rol) const override;
    QHash<int, QByteArray> roleNames() const override;

    void agregar(const QString &nombre, const QString &ruta, const QString &miniatura);
    void quitar(int fila);
    QString ruta(int fila) const { return fila >= 0 && fila < filas.size() ? filas.at(fila).ruta : QString(); }

signals:
    void cantidadCambio();

private:
    struct Fila { QString nombre, ruta, miniatura; };
    QList<Fila> filas;
};

class Descargas : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool musica READ musica CONSTANT)
    Q_PROPERTY(QString titulo READ titulo CONSTANT)

    // El formulario (QML lo lee y lo escribe; queda acá para no perderse al cambiar de página)
    Q_PROPERTY(QString urls MEMBER urls_ NOTIFY formularioCambio)
    Q_PROPERTY(int plataforma MEMBER plataforma_ NOTIFY formularioCambio)       // 0 YouTube, 1 redes sociales
    Q_PROPERTY(QString carpeta MEMBER carpeta_ NOTIFY formularioCambio)         // solo redes sociales
    Q_PROPERTY(bool playlist MEMBER playlist_ NOTIFY formularioCambio)          // solo música
    Q_PROPERTY(bool usarCookies MEMBER usarCookies_ NOTIFY formularioCambio)
    Q_PROPERTY(QString archivoCookies MEMBER archivoCookies_ NOTIFY formularioCambio)
    Q_PROPERTY(bool elegirPistas MEMBER elegirPistas_ NOTIFY formularioCambio)  // solo videos

    // Lo que está pasando
    Q_PROPERTY(bool ocupada READ ocupada NOTIFY ocupadaCambio)
    Q_PROPERTY(QString estado READ estado NOTIFY estadoCambio)
    Q_PROPERTY(int tipoEstado READ tipoEstado NOTIFY estadoCambio)              // TipoEstado
    Q_PROPERTY(bool mostrarProgreso READ mostrarProgreso NOTIFY progresoCambio)
    Q_PROPERTY(int progresoArchivo READ progresoArchivo NOTIFY progresoCambio)  // 0..100; -1 sin barra del archivo
    Q_PROPERTY(int hechas READ hechas NOTIFY progresoCambio)
    Q_PROPERTY(int total READ total NOTIFY progresoCambio)
    Q_PROPERTY(QString avisos READ avisos NOTIFY avisosCambio)
    Q_PROPERTY(int cantidadAvisos READ cantidadAvisos NOTIFY avisosCambio)
    Q_PROPERTY(QObject *descargados READ descargados CONSTANT)
    Q_PROPERTY(QObject *pistas READ pistas CONSTANT)

public:
    enum class Tipo { Videos, Musica };
    enum TipoEstado { Info, Exito, Error };
    Q_ENUM(TipoEstado)

    Descargas(Tipo tipo, Herramientas *herramientas, QObject *parent = nullptr);

    bool musica() const { return tipo == Tipo::Musica; }
    QString titulo() const;
    bool ocupada() const { return ocupada_; }
    QString estado() const { return estado_; }
    int tipoEstado() const { return tipoEstado_; }
    bool mostrarProgreso() const { return mostrarProgreso_; }
    int progresoArchivo() const { return progresoArchivo_; }
    int hechas() const { return hechas_; }
    int total() const { return total_; }
    QString avisos() const { return avisos_.join(u'\n'); }
    int cantidadAvisos() const { return int(avisos_.size()); }
    QObject *descargados() { return &modeloDescargados; }
    QObject *pistas() { return &modeloPistas; }

    Q_INVOKABLE void descargar();
    Q_INVOKABLE void cancelar();
    Q_INVOKABLE void pegar();                    // agrega lo del portapapeles a las URLs
    Q_INVOKABLE void abrirYoutubeMusic();
    Q_INVOKABLE bool ver(int fila);              // falso si no hay ninguna app para abrirlo
    Q_INVOKABLE void borrar(int fila);           // definitivo: Android no tiene papelera para esto
    Q_INVOKABLE void continuarConEleccion(const QVariantMap &eleccion);
    Q_INVOKABLE void cancelarEleccion();
    Q_INVOKABLE void limpiarAvisos();

    void cancelarYEsperar();

signals:
    void formularioCambio();
    void ocupadaCambio();
    void estadoCambio();
    void progresoCambio();
    void avisosCambio();
    // La consulta terminó y hay calidades, pistas o subtítulos para elegir (en «pistas»):
    // la página de elección responde con continuarConEleccion() o cancelarEleccion()
    void pedirEleccion();

private:
    void empezar();
    void siguienteConsulta();
    void terminoConsulta(int codigo, QProcess::ExitStatus estado);
    void siguienteUrl();
    void leerSalida();
    void terminoProceso(int codigo, QProcess::ExitStatus estado);
    void procesarLinea(const QString &linea);
    void registrarTerminados(const QString &miniatura);
    void finalizar();
    void anotar(const QString &texto);
    void ponerEstado(const QString &texto, TipoEstado tipo = Info);
    void ponerOcupada(bool ocupada);
    void ponerProgreso(int archivo, int hechas);
    QString destino() const;
    QString carpetaMiniaturas() const;
    QString carpetaMiniaturasLista() const;
    QString carpetaSubtitulos() const;
    QString archivoRutas() const;
    QString ultimaMiniatura() const;

    Tipo tipo;
    Herramientas *herramientas;
    QProcess proceso;
    ModeloDescargados modeloDescargados;
    ModeloPistas modeloPistas;

    QString urls_, carpeta_, archivoCookies_;
    int plataforma_ = 0;
    bool playlist_ = false, usarCookies_ = false, elegirPistas_ = false;

    bool ocupada_ = false;
    QString estado_;
    TipoEstado tipoEstado_ = Info;
    bool mostrarProgreso_ = false;
    int progresoArchivo_ = -1;
    int hechas_ = 0;
    QStringList avisos_;

    QStringList cola;
    int total_ = 0;
    int indice = 0;
    int listas = 0;
    int fallidas = 0;
    int miniaturasGuardadas = 0;
    bool actualizando = false;
    bool consultando = false;       // paso previo: «yt-dlp -J» de cada URL
    bool esperandoEleccion = false; // la página de elección está abierta
    QStringList urlsPedidas;        // todas las URL de esta descarga (la consulta no las consume)
    OfertaVideo oferta;             // lo que ofrecen (suma de las consultas)
    Eleccion eleccion;              // lo elegido (vacío: lo de siempre, en MKV)
    bool cancelado = false;
    QString archivoActual;
    QString mensajeError;
    QString ultimoPorcentaje;
};
