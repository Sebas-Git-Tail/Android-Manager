// Android-Manager: descargas de videos y música con yt-dlp, y respaldo del celular con rsync.

#include "VentanaPrincipal.h"

#include <QApplication>
#include <QPalette>

int main(int argc, char *argv[])
{
    // Sin esto, Qt frena la app al pasarla a segundo plano y las descargas y los respaldos se
    // quedarían esperando (nadie leería la salida de yt-dlp y rsync)
    qputenv("QT_BLOCK_EVENT_LOOPS_WHEN_SUSPENDED", "0");

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Android-Manager"));
    QApplication::setApplicationVersion(QStringLiteral(VERSION_PROGRAMA));
    QApplication::setStyle(QStringLiteral("Fusion"));
    // El resaltado que da el sistema es muy claro para el texto blanco
    QPalette paleta = QApplication::palette();
    paleta.setColor(QPalette::Highlight, QColor(0x2a, 0x6f, 0xb5));
    paleta.setColor(QPalette::HighlightedText, Qt::white);
    // Al definir una paleta propia, lo deshabilitado deja de verse apagado si no se lo indica
    for (const QPalette::ColorRole rol : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        paleta.setColor(QPalette::Disabled, rol, QColor(0x80, 0x80, 0x80));
    QApplication::setPalette(paleta);

    // Botones y casillas de un tamaño cómodo para tocar con el dedo
    app.setStyleSheet(QStringLiteral(
        "QPushButton, QComboBox, QLineEdit { min-height: 40px; }"
        "QCheckBox { min-height: 36px; }"
        "QCheckBox::indicator { width: 24px; height: 24px; }"));

    VentanaPrincipal ventana;
    ventana.showMaximized();
    return app.exec();
}
