// Cómo va la tarea: el estado (con ícono si terminó bien o mal), la barra del archivo actual y,
// si son varias URLs, cuántas van
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts

Seccion {
    id: tarjeta
    property string texto
    property int tipo: 0               // 0 info, 1 éxito, 2 error
    property bool trabajando: false
    property int progreso: -1          // 0..100; -1 sin barra (o indeterminada si está trabajando)
    property string detalle: ""        // debajo de la barra (p. ej. el archivo que se copia)
    property int hechas: 0
    property int total: 0              // más de 1: se muestra «2 de 5»

    visible: texto !== "" || trabajando

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        IconImage {
            visible: !tarjeta.trabajando && tarjeta.tipo !== 0
            Layout.alignment: Qt.AlignTop
            source: "qrc:/qt/qml/AndroidManager/iconos/" + (tarjeta.tipo === 1 ? "listo" : "error") + ".svg"
            sourceSize: Qt.size(22, 22)
            color: ventana.colorEstado(tarjeta.tipo)
        }
        Label {
            Layout.fillWidth: true
            text: tarjeta.texto
            wrapMode: Text.Wrap
            color: tarjeta.tipo === 0 ? ventana.tinta : ventana.colorEstado(tarjeta.tipo)
            font.pixelSize: 15
        }
    }
    ProgressBar {
        visible: tarjeta.trabajando || tarjeta.progreso >= 0
        Layout.fillWidth: true
        from: 0; to: 100
        value: Math.max(0, tarjeta.progreso)
        indeterminate: tarjeta.trabajando && tarjeta.progreso < 0
    }
    Label {
        visible: tarjeta.detalle !== ""
        Layout.fillWidth: true
        text: tarjeta.detalle
        elide: Text.ElideMiddle
        font.pixelSize: 12
        color: ventana.tintaSecundaria
    }
    RowLayout {
        visible: tarjeta.trabajando && tarjeta.total > 1
        Layout.fillWidth: true
        spacing: 10
        ProgressBar {
            Layout.fillWidth: true
            from: 0; to: Math.max(1, tarjeta.total)
            value: tarjeta.hechas
            Material.accent: ventana.verde
        }
        Label {
            text: tarjeta.hechas + " de " + tarjeta.total
            font.pixelSize: 12
            color: ventana.tintaSecundaria
        }
    }
}
