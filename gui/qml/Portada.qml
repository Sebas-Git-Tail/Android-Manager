// Portada: el logo, los avisos (falta el permiso, se están preparando las herramientas) y una
// tarjeta por herramienta. Si una está trabajando, su tarjeta muestra cómo va. «Apps y actualizaciones»
// y la versión están en el menú de las 3 rayitas (Main.qml).
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts
import Comun.Ui

Hoja {
    title: "Android-Manager"

    // ── Encabezado ──
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        Layout.bottomMargin: 4
        spacing: 14
        Image {
            source: "qrc:/qt/qml/AndroidManager/Logo.png"
            sourceSize.height: 176
            Layout.preferredHeight: 88
            Layout.preferredWidth: 88 * implicitWidth / Math.max(1, implicitHeight)
            fillMode: Image.PreserveAspectFit
            smooth: true
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                text: "Descargas y respaldo"
                font.pixelSize: 22
                font.weight: Font.DemiBold
                color: ventana.tinta
            }
            Label {
                Layout.fillWidth: true
                text: "Bajá videos y música, y guardá una copia del celular en el servidor."
                wrapMode: Text.WordWrap
                color: ventana.tintaSecundaria
            }
        }
    }

    // ── Falta el permiso de los archivos ──
    Pane {
        visible: !Sistema.accesoArchivos
        Layout.fillWidth: true
        padding: 16
        background: Rectangle { radius: 14; color: ventana.naranjaSuave; border.color: Qt.rgba(ventana.naranja.r, ventana.naranja.g, ventana.naranja.b, 0.35) }
        contentItem: ColumnLayout {
            spacing: 10
            RowLayout {
                spacing: 10
                IconImage {
                    Layout.alignment: Qt.AlignTop
                    source: "qrc:/qt/qml/AndroidManager/iconos/aviso.svg"
                    sourceSize: Qt.size(22, 22)
                    color: ventana.naranja
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: "Falta un permiso"; font.weight: Font.DemiBold; color: ventana.tinta }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: ventana.tintaSecundaria
                        text: "Para guardar en Movies y Music y para respaldar el celular, la app necesita acceso a todos los archivos."
                    }
                }
            }
            Button {
                Layout.alignment: Qt.AlignRight
                text: "Dar acceso"
                highlighted: true
                Material.accent: ventana.naranja
                onClicked: Sistema.pedirAcceso()
            }
        }
    }

    // ── Preparando las herramientas (solo la primera vez) o no se pudo ──
    Pane {
        visible: !Herramientas.listas
        Layout.fillWidth: true
        padding: 14
        background: Rectangle { radius: 14; color: ventana.superficie; border.color: ventana.borde }
        contentItem: RowLayout {
            spacing: 12
            BusyIndicator {
                visible: Herramientas.preparando
                running: visible
                implicitWidth: 32
                implicitHeight: 32
            }
            IconImage {
                visible: !Herramientas.preparando
                source: "qrc:/qt/qml/AndroidManager/iconos/error.svg"
                sourceSize: Qt.size(22, 22)
                color: ventana.rojo
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Herramientas.preparando ? ventana.tintaSecundaria : ventana.rojo
                text: Herramientas.preparando ? "Preparando las herramientas (solo tarda la primera vez)…" : Herramientas.error
            }
        }
    }

    // ── Herramientas ──
    Label {
        Layout.topMargin: 8
        Layout.leftMargin: 4
        text: "Herramientas"
        font.pixelSize: 12
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.8
        color: ventana.tintaApagada
    }

    TarjetaHerramienta {
        enabled: Herramientas.listas
        icono: "qrc:/qt/qml/AndroidManager/iconos/video.svg"
        titulo: "Videos"
        tono: ventana.azul
        ocupada: Videos.ocupada
        progreso: Videos.progresoArchivo
        detalle: Videos.ocupada ? Videos.estado : "YouTube y redes sociales, en MKV"
        onClicked: ventana.abrir(Qt.resolvedUrl("PaginaDescargas.qml"), { modelo: Videos })
    }
    TarjetaHerramienta {
        enabled: Herramientas.listas
        icono: "qrc:/qt/qml/AndroidManager/iconos/musica.svg"
        titulo: "Música"
        tono: ventana.oscuro ? "#c48df0" : "#8a4fc4"
        ocupada: Musica.ocupada
        progreso: Musica.progresoArchivo
        detalle: Musica.ocupada ? Musica.estado : "YouTube Music, con carátula y datos de la canción"
        onClicked: ventana.abrir(Qt.resolvedUrl("PaginaDescargas.qml"), { modelo: Musica })
    }
    TarjetaHerramienta {
        enabled: Herramientas.listas
        icono: "qrc:/qt/qml/AndroidManager/iconos/respaldo.svg"
        titulo: "Respaldo"
        tono: ventana.verde
        ocupada: Respaldo.ocupada
        progreso: Respaldo.progreso
        detalle: Respaldo.ocupada ? Respaldo.estado : "Copia el celular al servidor. " + Respaldo.ultimoRespaldo
        onClicked: ventana.abrir(Qt.resolvedUrl("PaginaRespaldo.qml"))
    }
}
