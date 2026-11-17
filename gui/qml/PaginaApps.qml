// «Apps y actualizaciones»: la versión nueva de esta app y las otras apps de Sebastián para
// instalar. Usa «Apps» (apps::ModeloApps de comun, como Ingresos y Gastos). Carina y Bodrio nunca
// aparecen en la lista.
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts

Page {
    id: pagina
    title: "Apps y actualizaciones"
    background: null

    Component.onCompleted: { Apps.refrescar(); Apps.buscarActualizacion(true) }
    // Al volver del instalador de Android, la lista se pone al día
    Connections {
        target: Qt.application
        function onStateChanged() { if (Qt.application.state === Qt.ApplicationActive) Apps.refrescar() }
    }

    ListView {
        id: lista
        anchors.fill: parent
        clip: true
        model: Apps
        spacing: 10
        topMargin: ventana.margen
        bottomMargin: ventana.margen + pagina.SafeArea.margins.bottom
        ScrollIndicator.vertical: ScrollIndicator {}

        header: ColumnLayout {
            width: lista.width
            spacing: 10
            Seccion {
                Layout.leftMargin: ventana.margen
                Layout.rightMargin: ventana.margen
                Layout.bottomMargin: 6
                titulo: "Esta app"
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    color: ventana.tinta
                    text: "Android-Manager " + Apps.versionPropia
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Apps.versionNueva !== "" ? ventana.naranja : ventana.tintaSecundaria
                    text: Apps.actualizando ? (Apps.progresoPropio >= 0 ? "Descargando… " + Math.round(Apps.progresoPropio * 100) + " %" : "Descargando…")
                        : Apps.versionNueva !== "" ? "Hay una versión nueva: la " + Apps.versionNueva
                        : "Estás en la última versión."
                }
                ProgressBar {
                    visible: Apps.actualizando
                    Layout.fillWidth: true
                    from: 0; to: 1
                    value: Math.max(0, Apps.progresoPropio)
                    indeterminate: Apps.progresoPropio < 0
                }
                Button {
                    visible: Apps.versionNueva !== "" && !Apps.actualizando
                    text: "Actualizar a la " + Apps.versionNueva
                    highlighted: true
                    onClicked: Apps.actualizarEstaApp()
                }
            }
            Label {
                visible: lista.count > 0
                Layout.leftMargin: ventana.margen + 4
                text: "Otras apps"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.8
                color: ventana.tintaApagada
            }
        }

        delegate: Item {        // ListView ignora la x del delegado: la tarjeta va adentro, con márgenes
            id: fila
            required property int index
            required property string nombre
            required property string descripcion
            required property string estado
            required property string accion
            required property bool ocupado
            required property real progreso
            width: lista.width
            implicitHeight: tarjetaApp.implicitHeight

            Pane {
                id: tarjetaApp
                x: ventana.margen
                width: fila.width - 2 * ventana.margen
                padding: 16
                background: Rectangle { radius: 14; color: ventana.superficie; border.color: ventana.borde }
                contentItem: RowLayout {
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label { text: fila.nombre; font.weight: Font.DemiBold; font.pixelSize: 16; color: ventana.tinta; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Label { text: fila.descripcion; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: ventana.tintaSecundaria; font.pixelSize: 13 }
                        Label {
                            text: fila.estado + (fila.ocupado && fila.progreso >= 0 ? " " + Math.round(fila.progreso * 100) + " %" : "")
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 12
                            color: ventana.tintaApagada
                        }
                    }
                    Button {
                        visible: fila.accion !== ""
                        text: fila.accion
                        highlighted: true
                        onClicked: Apps.instalar(fila.index)
                    }
                    BusyIndicator { visible: fila.ocupado; running: visible; implicitWidth: 32; implicitHeight: 32 }
                }
            }
        }

        footer: Label {
            width: lista.width
            leftPadding: ventana.margen + 4
            rightPadding: ventana.margen
            topPadding: 12
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: ventana.tintaApagada
            text: Apps.mensaje
        }
    }
}
