// «Videos» y «Música»: la misma página con dos modos (modelo = Videos o Musica, objetos Descargas
// de C++). Enlaces, opciones, el botón grande, cómo va y lo que ya se bajó.
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts
import Comun.Ui

Hoja {
    id: pagina
    required property var modelo
    readonly property bool musica: modelo.musica
    title: musica ? "Música" : "Videos"

    // ── Enlaces ──
    Seccion {
        titulo: musica ? "Enlaces de YouTube Music" : "Enlaces"
        enabled: !modelo.ocupada

        TextArea {
            id: campoUrls
            Layout.fillWidth: true
            Layout.minimumHeight: 120
            Material.containerStyle: Material.Outlined
            placeholderText: "Pegá o escribí los enlaces"      // con texto, queda arriba como etiqueta
            wrapMode: TextArea.WrapAnywhere
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhUrlCharactersOnly
            text: modelo.urls
            onTextChanged: if (text !== modelo.urls) modelo.urls = text
        }
        Label {
            Layout.fillWidth: true
            text: musica ? "Uno por línea (music.youtube.com/…): se bajan de a uno, en orden." : "Uno por línea (youtube.com/…, youtu.be/… o de redes sociales): se bajan de a uno, en orden."
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            color: ventana.tintaApagada
        }
        Flow {
            Layout.fillWidth: true
            spacing: 4
            Material.foreground: ventana.azul
            Button {
                text: "Pegar"
                icon.source: "qrc:/qt/qml/AndroidManager/iconos/pegar.svg"
                flat: true
                onClicked: modelo.pegar()
            }
            Button {
                visible: musica
                text: "Abrir YouTube Music"
                icon.source: "qrc:/qt/qml/AndroidManager/iconos/abrir.svg"
                flat: true
                onClicked: modelo.abrirYoutubeMusic()
            }
            Button {
                visible: campoUrls.text !== ""
                text: "Borrar todo"
                icon.source: "qrc:/qt/qml/AndroidManager/iconos/cerrar.svg"
                flat: true
                Material.foreground: ventana.tintaSecundaria
                onClicked: modelo.urls = ""
            }
        }
    }

    // ── Opciones ──
    Seccion {
        titulo: "Opciones"
        enabled: !modelo.ocupada

        // Videos: dónde se guarda
        Label { visible: !musica; text: "Guardar"; color: ventana.tintaSecundaria; font.pixelSize: 13 }
        Segmentos {
            visible: !musica
            opciones: ["YouTube", "Redes sociales"]
            elegida: modelo.plataforma
            onElegir: (indice) => modelo.plataforma = indice
        }
        Label {
            visible: !musica && modelo.plataforma === 0
            Layout.fillWidth: true
            text: "Van a Movies/Yt-dlp."
            font.pixelSize: 12
            color: ventana.tintaApagada
        }
        TextField {
            visible: !musica && modelo.plataforma === 1
            Layout.fillWidth: true
            Material.containerStyle: Material.Outlined
            placeholderText: "Carpeta"
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            text: modelo.carpeta
            onTextChanged: if (text !== modelo.carpeta) modelo.carpeta = text
        }
        Label {
            visible: musica
            Layout.fillWidth: true
            text: "Van a Music, en una carpeta por artista."
            font.pixelSize: 12
            color: ventana.tintaApagada
        }

        SwitchDelegate {
            visible: musica
            Layout.fillWidth: true
            leftPadding: 0; rightPadding: 0
            text: "Bajar la playlist completa"
            checked: modelo.playlist
            onToggled: modelo.playlist = checked
        }
        SwitchDelegate {
            visible: !musica
            Layout.fillWidth: true
            leftPadding: 0; rightPadding: 0
            text: "Elegir calidad, audio y subtítulos"
            checked: modelo.elegirPistas
            onToggled: modelo.elegirPistas = checked
        }
        SwitchDelegate {
            Layout.fillWidth: true
            leftPadding: 0; rightPadding: 0
            text: "Bajar con el servidor"
            checked: modelo.usarServidor
            onToggled: modelo.usarServidor = checked
        }
        Label {
            Layout.fillWidth: true
            text: modelo.usarServidor
                  ? "Lo baja el servidor con las cookies de su navegador (siempre activadas) y después llega al celular."
                  : "Se baja en el celular, sin cookies: algunos videos o canciones pueden fallar."
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            color: ventana.tintaApagada
        }
    }

    // ── Descargar / Cancelar ──
    Button {
        Layout.fillWidth: true
        Layout.preferredHeight: 56
        text: modelo.ocupada ? "Cancelar" : "Descargar"
        icon.source: "qrc:/qt/qml/AndroidManager/iconos/" + (modelo.ocupada ? "cerrar" : "descargar") + ".svg"
        highlighted: true
        Material.accent: modelo.ocupada ? ventana.rojoRelleno : ventana.acento
        font.pixelSize: 16
        onClicked: modelo.ocupada ? modelo.cancelar() : modelo.descargar()
    }

    TarjetaEstado {
        texto: modelo.estado
        tipo: modelo.tipoEstado
        trabajando: modelo.ocupada
        progreso: modelo.progresoArchivo
        hechas: modelo.hechas
        total: modelo.total
    }

    // ── Lo que se bajó (en esta sesión) ──
    Seccion {
        visible: modelo.descargados.cantidad > 0
        titulo: musica ? "Canciones descargadas" : "Videos descargados"
        padding: 8
        topPadding: 14

        Repeater {
            model: modelo.descargados
            ItemDelegate {
                required property int index
                required property string nombre
                required property string miniatura
                Layout.fillWidth: true
                leftPadding: 8
                rightPadding: 0
                onClicked: modelo.ver(index)
                contentItem: RowLayout {
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: musica ? 52 : 92
                        Layout.preferredHeight: 52
                        radius: 8
                        color: ventana.oscuro ? "#2a2a29" : "#ecebe7"
                        clip: true
                        Image {
                            anchors.fill: parent
                            source: miniatura
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            visible: status === Image.Ready
                        }
                        IconImage {
                            anchors.centerIn: parent
                            visible: miniatura === ""
                            source: "qrc:/qt/qml/AndroidManager/iconos/" + (musica ? "musica" : "video") + ".svg"
                            sourceSize: Qt.size(22, 22)
                            color: ventana.tintaApagada
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: nombre
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: ventana.tinta
                    }
                    ToolButton {
                        icon.source: "qrc:/qt/qml/AndroidManager/iconos/borrar.svg"
                        icon.color: ventana.tintaApagada
                        onClicked: { dialogoBorrar.fila = index; dialogoBorrar.nombre = nombre; dialogoBorrar.open() }
                        Accessible.name: "Borrar"
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            text: musica ? "Tocá una para escucharla." : "Tocá uno para verlo."
            font.pixelSize: 12
            color: ventana.tintaApagada
        }
    }

    Avisos {
        texto: modelo.avisos
        cantidad: modelo.cantidadAvisos
        onLimpiar: modelo.limpiarAvisos()
    }

    // Android no tiene una papelera a la que mandar un archivo desde acá: el borrado es definitivo
    Dialogo {
        id: dialogoBorrar
        property int fila: -1
        property string nombre
        title: "¿Borrar?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "«" + dialogoBorrar.nombre + "» se borra del celular. No se puede deshacer."
        }
        footer: DialogButtonBox {
            Button { text: "No"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Borrar"; flat: true; Material.foreground: ventana.rojo; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: { modelo.borrar(fila); ventana.avisar("Borrado") }
    }
}
