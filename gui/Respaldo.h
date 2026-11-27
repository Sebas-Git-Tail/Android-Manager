// «Respaldo», sin interfaz (la página es PaginaRespaldo.qml): copia el almacenamiento interno al
// servidor con rsync, lo mismo que General/Android/Respaldo.sh en el Manager de Linux. Como allá,
// lo que se borró del celular también se borra del respaldo (--delete). Cada celular respalda en
// la carpeta de su dueño: Respaldos/<persona>/<celular>/. Los dos nombres quedan guardados.
//
// Desde la 1.13.0, lo que el respaldo borra o pisa en el servidor no se pierde: va a la carpeta hermana
// <celular>.anteriores/<AAAA-MM-DD_HH-MM>/ y se borra a los 30 días (ver AnalisisRsync.h). Y se puede
// traer de vuelta al celular (restaurar): todo, una carpeta, o lo que quedó en «anteriores» de una fecha.
// La restauración no borra nada del celular ni pisa lo que allá sea más nuevo (--update).

#pragma once

#include "AnalisisRsync.h"
#include "Herramientas.h"
#include "ResumenSimulacion.h"

#include <QProcess>
#include <QStringList>
#include <QVariantList>

class QTcpSocket;

class Respaldo : public QObject
{
    Q_OBJECT
    // El formulario: los nombres se guardan apenas se escriben
    Q_PROPERTY(QString persona READ textoPersona WRITE ponerPersona NOTIFY nombresCambio)
    Q_PROPERTY(QString celular READ textoCelular WRITE ponerCelular NOTIFY nombresCambio)
    Q_PROPERTY(bool simular MEMBER simular_ NOTIFY simularCambio)

    Q_PROPERTY(QString destino READ destino NOTIFY nombresCambio)                 // ruta en el servidor ("" si faltan nombres)
    Q_PROPERTY(QString ultimoRespaldo READ textoUltimoRespaldo NOTIFY ultimoCambio)
    Q_PROPERTY(bool disponible READ disponible CONSTANT)                          // la app trae rsync y dbclient

    Q_PROPERTY(bool ocupada READ ocupada NOTIFY ocupadaCambio)
    Q_PROPERTY(QString estado READ estado NOTIFY estadoCambio)
    Q_PROPERTY(int tipoEstado READ tipoEstado NOTIFY estadoCambio)                // 0 info, 1 éxito, 2 error
    Q_PROPERTY(int progreso READ progreso NOTIFY progresoCambio)                  // 0..100; -1 sin barra
    Q_PROPERTY(QString archivo READ archivo NOTIFY progresoCambio)                // el que se está copiando
    Q_PROPERTY(QString avisos READ avisos NOTIFY avisosCambio)
    Q_PROPERTY(int cantidadAvisos READ cantidadAvisos NOTIFY avisosCambio)
    Q_PROPERTY(bool restaurando READ restaurando NOTIFY ocupadaCambio)            // la tarea en curso trae archivos al celular

    // Restaurar: lo que se encontró en el servidor (listar())
    Q_PROPERTY(bool listado READ listado NOTIFY restauracionCambio)               // ya se consultó, y hay respaldo
    Q_PROPERTY(QVariantList versiones READ versiones NOTIFY restauracionCambio)   // [{nombre, texto}], la más nueva primero

public:
    enum TipoEstado { Info, Exito, Error };
    Q_ENUM(TipoEstado)

    explicit Respaldo(Herramientas *herramientas, QObject *parent = nullptr);

    QString textoPersona() const { return persona_; }
    QString textoCelular() const { return celular_; }
    void ponerPersona(const QString &texto);
    void ponerCelular(const QString &texto);
    QString destino() const;
    QString textoUltimoRespaldo() const;
    bool disponible() const { return herramientas->respaldoDisponible(); }
    bool ocupada() const { return trabajando; }
    QString estado() const { return estado_; }
    int tipoEstado() const { return tipoEstado_; }
    int progreso() const { return progreso_; }
    QString archivo() const { return archivo_; }
    QString avisos() const { return avisos_.join(u'\n'); }
    int cantidadAvisos() const { return int(avisos_.size()); }
    bool restaurando() const { return trabajando && modo == Modo::Restaurar; }
    bool listado() const { return opciones.hayRespaldo; }
    QVariantList versiones() const;

    Q_INVOKABLE void empezar(const QString &contrasena);
    Q_INVOKABLE void confirmarDestino(bool aceptado);   // respuesta a pedirConfirmacion()
    Q_INVOKABLE void cancelar();
    Q_INVOKABLE void limpiarAvisos();

    // Restaurar (cada vez con la contraseña, como el respaldo)
    Q_INVOKABLE void listar(const QString &contrasena);
    Q_INVOKABLE QStringList carpetas(const QString &version) const;   // version "" = el respaldo actual
    Q_INVOKABLE QString descripcionRestauracion(const QString &version, const QString &carpeta) const;
    Q_INVOKABLE void restaurar(const QString &contrasena, const QString &version, const QString &carpeta);

    void cortarAlSalir();    // al cerrar la app: corta lo que haya sin esperar

signals:
    void nombresCambio();
    void simularCambio();
    void ultimoCambio();
    void ocupadaCambio();
    void estadoCambio();
    void progresoCambio();
    void avisosCambio();
    void restauracionCambio();
    // Por --delete, respaldar en la carpeta equivocada borraría lo que haya allí: se pregunta
    // cada vez que el destino cambia (la primera vez incluida). Respuesta: confirmarDestino()
    void pedirConfirmacion(const QString &ruta);
    void enfocar(const QString &campo);   // «persona», «celular» o «contrasena»
    void contrasenaUsada();               // ya se usó (o se descartó): la página borra el campo

private:
    enum class Modo { Respaldar, Listar, Restaurar };

    bool revisarNombres(const QString &contrasena);
    void continuar();
    QStringList argumentosRespaldo(const QString &servidor);
    QStringList argumentosListado(const QString &servidor) const;
    QStringList argumentosRestauracion(const QString &servidor) const;
    void terminoListado();
    QString textoCancelado() const;
    void probarServidor(int indice);
    void copiar(const QString &servidor);
    void leerSalida();
    void procesarLinea(const QString &linea);
    void terminoProceso(int codigo, QProcess::ExitStatus estado);
    void anotar(const QString &texto);
    void ponerEstado(const QString &texto, TipoEstado tipo = Info);
    void ponerTrabajando(bool trabajando);
    void ponerProgreso(int progreso, const QString &archivo);
    void mostrarResumen();
    void anotarAnteriores();
    void anotarRespaldoHecho();
    QString persona() const;       // normalizada; vacía si lo escrito no sirve como carpeta
    QString celular() const;       // vacío si lo escrito no sirve como carpeta
    QString rutaEnServidor() const;
    QString carpetaPersona() const;    // Respaldos/<persona>/ en el servidor
    void guardarNombres();

    Herramientas *herramientas;
    QProcess proceso;
    QTcpSocket *prueba = nullptr;
    QString persona_, celular_;
    bool simular_ = false;
    bool simulando = false;        // lo que se eligió al empezar (no cambia a mitad de camino)
    QString contrasena;            // se guarda solo mientras se confirma y se busca el servidor
    QByteArray pendiente;
    ResumenSimulacion resumen;
    QString errorConexion;
    QString estado_, archivo_;
    TipoEstado tipoEstado_ = Info;
    int progreso_ = -1;
    QStringList avisos_;
    bool sinDatos = false;         // rsync no llegó a recibir nada del servidor
    bool cancelado = false;
    bool trabajando = false;

    Modo modo = Modo::Respaldar;
    int anterioresMovidos = 0;     // archivos que en este respaldo pasaron a «anteriores»
    QString versionNueva;          // la carpeta de «anteriores» de este respaldo
    QStringList lineasListado;
    analisisRsync::OpcionesRestauracion opciones;
    QString restaurarVersion, restaurarCarpeta;
    int restaurados = 0;
};
