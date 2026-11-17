// Opción para tocar (una de varias, como las calidades): borde fino y, si está elegida, fondo y
// texto del color de acento. Los botones «checked» de Material quedaban celestes y no se leían.
import QtQuick
import QtQuick.Controls.Material

AbstractButton {
    id: chip
    property bool elegido: false
    implicitWidth: etiqueta.implicitWidth + 28
    implicitHeight: 40
    background: Rectangle {
        radius: 10
        color: chip.elegido ? ventana.azulSuave : (chip.down ? (ventana.oscuro ? "#262625" : "#efefec") : "transparent")
        border.color: chip.elegido ? ventana.azul : ventana.borde
        border.width: chip.elegido ? 1.5 : 1
    }
    contentItem: Label {
        id: etiqueta
        text: chip.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.weight: chip.elegido ? Font.DemiBold : Font.Normal
        color: chip.elegido ? ventana.azul : ventana.tinta
    }
}
