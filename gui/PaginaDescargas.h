// Páginas «Descargar Vídeos» y «Descargar Música»: la misma página con dos modos. Hace lo
// mismo que Logica/Descargar.sh en el Manager de Linux: las mismas opciones de yt-dlp y una
// URL por vez, leyendo el avance de su salida.

#pragma once

#include "Herramientas.h"

#include <QProcess>
#include <QStringList>
#include <QWidget>

namespace Ui { class PaginaDescargas; }

class PaginaDescargas : public QWidget
{
    Q_OBJECT

public:
    enum class Tipo { Videos, Musica };

    PaginaDescargas(Tipo tipo, Herramientas *herramientas, QWidget *parent = nullptr);
    ~PaginaDescargas() override;

    bool ocupada() const;
    void cancelarYEsperar();

signals:
    void ocupacionCambiada();

private slots:
    void descargar();
    void cancelar();
    void leerSalida();
    void terminoProceso(int codigo, QProcess::ExitStatus estado);
    void ver();
    void borrar();

private:
    void siguienteUrl();
    void procesarLinea(const QString &linea);
    void registrarTerminados(const QString &miniatura);
    void finalizar();
    void anotar(const QString &texto);
    void habilitarFormulario(bool habilitado);
    QString destino() const;
    QString carpetaMiniaturas() const;
    QString archivoRutas() const;
    QString ultimaMiniatura() const;

    Ui::PaginaDescargas *ui;
    Tipo tipo;
    Herramientas *herramientas;
    QProcess proceso;
    QStringList cola;
    int total = 0;
    int indice = 0;
    int listas = 0;
    int fallidas = 0;
    bool actualizando = false;
    bool cancelado = false;
    QString archivoActual;
    QString mensajeError;
    QString ultimoPorcentaje;
};
