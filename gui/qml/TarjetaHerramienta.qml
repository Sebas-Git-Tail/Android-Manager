// Botón grande de la portada: ícono de color, título, una línea que explica qué hace y, si la
// herramienta está trabajando, su estado y una barrita de progreso
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts

ItemDelegate {
    id: tarjeta
    property string icono
    property string titulo
    property string detalle
    property color tono: ventana.azul
    property bool ocupada: false
    property int progreso: -1          // 0..100; -1 = sin saber cuánto falta
    property string insignia: ""       // p. ej. «Nueva versión»

    Layout.fillWidth: true
    padding: 14
    leftPadding: 14
    rightPadding: 10

    background: Rectangle {
        radius: 16
        color: tarjeta.down ? (ventana.oscuro ? "#262625" : "#efefec") : ventana.superficie
        border.color: tarjeta.visualFocus ? ventana.azul : ventana.borde
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: RowLayout {
        spacing: 14
        Rectangle {
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignTop
            radius: 14
            color: Qt.rgba(tarjeta.tono.r, tarjeta.tono.g, tarjeta.tono.b, ventana.oscuro ? 0.18 : 0.12)
            IconImage {
                anchors.centerIn: parent
                source: tarjeta.icono
                sourceSize: Qt.size(24, 24)
                color: tarjeta.tono
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            RowLayout {
                spacing: 8
                Label {
                    text: tarjeta.titulo
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    color: ventana.tinta
                }
                Label {
                    visible: tarjeta.insignia !== ""
                    text: tarjeta.insignia
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: "white"
                    leftPadding: 7; rightPadding: 7; topPadding: 2; bottomPadding: 2
                    background: Rectangle { radius: height / 2; color: ventana.naranja }
                }
            }
            Label {
                Layout.fillWidth: true
                text: tarjeta.detalle
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                font.pixelSize: 13
                color: tarjeta.ocupada ? tarjeta.tono : ventana.tintaSecundaria
            }
            ProgressBar {
                visible: tarjeta.ocupada
                Layout.fillWidth: true
                Layout.topMargin: 4
                from: 0; to: 100
                value: Math.max(0, tarjeta.progreso)
                indeterminate: tarjeta.progreso < 0
                Material.accent: tarjeta.tono
            }
        }
        IconImage {
            Layout.alignment: Qt.AlignVCenter
            source: "qrc:/qt/qml/AndroidManager/iconos/siguiente.svg"
            sourceSize: Qt.size(20, 20)
            color: ventana.tintaApagada
        }
    }
}
