// Lo que fueron diciendo yt-dlp o rsync (advertencias, errores, el resumen de una simulación),
// plegado: se abre tocando el título
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts

Seccion {
    id: avisos
    property string texto
    property int cantidad: 0
    property bool abierto: false
    signal limpiar()

    visible: cantidad > 0
    padding: 6
    topPadding: 4
    bottomPadding: abierto ? 12 : 4

    ItemDelegate {
        Layout.fillWidth: true
        onClicked: avisos.abierto = !avisos.abierto
        background: null
        contentItem: RowLayout {
            spacing: 10
            IconImage {
                source: "qrc:/qt/qml/AndroidManager/iconos/aviso.svg"
                sourceSize: Qt.size(20, 20)
                color: ventana.naranja
            }
            Label {
                Layout.fillWidth: true
                text: "Avisos (" + avisos.cantidad + ")"
                font.weight: Font.DemiBold
                color: ventana.tinta
            }
            IconImage {
                source: "qrc:/qt/qml/AndroidManager/iconos/desplegar.svg"
                sourceSize: Qt.size(20, 20)
                color: ventana.tintaApagada
                rotation: avisos.abierto ? 180 : 0
                Behavior on rotation { NumberAnimation { duration: 150 } }
            }
        }
    }
    Label {
        visible: avisos.abierto
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        text: avisos.texto
        wrapMode: Text.WrapAnywhere
        font.pixelSize: 12
        color: ventana.tintaSecundaria
        textFormat: Text.PlainText
    }
    Button {
        visible: avisos.abierto
        Layout.alignment: Qt.AlignRight
        Layout.rightMargin: 4
        text: "Limpiar"
        flat: true
        onClicked: { avisos.abierto = false; avisos.limpiar() }
    }
}
