// Ventana principal: las tres herramientas en la portada; al elegir una se ve su página y
// el botón Volver (o el «atrás» del celular) regresa a la portada.

#pragma once

#include "Herramientas.h"

#include <QWidget>

namespace Ui { class VentanaPrincipal; }
class PaginaDescargas;
class PaginaRespaldo;

class VentanaPrincipal : public QWidget
{
    Q_OBJECT

public:
    explicit VentanaPrincipal(QWidget *parent = nullptr);
    ~VentanaPrincipal() override;

protected:
    void keyReleaseEvent(QKeyEvent *evento) override;
    void closeEvent(QCloseEvent *evento) override;
    void showEvent(QShowEvent *evento) override;

private:
    void mostrar(QWidget *pagina);
    void volver();
    void actualizarAcceso();
    void actualizarServicio();
    void respetarBarrasDelSistema();
    void aplicarMargenes(const QMargins &margenes);
    bool hayTarea() const;

    Ui::VentanaPrincipal *ui;
    Herramientas herramientas;
    PaginaDescargas *videos;
    PaginaDescargas *musica;
    PaginaRespaldo *respaldo;
    bool servicioActivo = false;
    bool notificacionesPedidas = false;
};
