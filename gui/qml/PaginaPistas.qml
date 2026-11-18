// «Calidad, audio y subtítulos»: lo que ofrecen los videos consultados (no hay listas fijas) y qué
// va dentro del archivo, siempre MKV. Las opciones vienen ordenadas de C++ (OpcionesPistas, lo
// mismo que el diálogo de las versiones de escritorio). Volver atrás cancela la descarga.
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import Comun.Ui

Hoja {
    id: pagina
    required property var modelo            // Videos (Descargas)
    readonly property var pistas: modelo.pistas
    property bool respondida: false
    title: "Calidad, audio y subtítulos"

    property int calidad: 0
    property string vcodec: ""
    property string acodec: ""

    function aceptar() {
        const marcados = (repetidor) => {
            const idiomas = []
            for (let i = 0; i < repetidor.count; ++i) {
                const fila = repetidor.itemAt(i)
                if (fila && fila.checked) idiomas.push(fila.idioma)
            }
            return idiomas
        }
        respondida = true
        modelo.continuarConEleccion({ audios: marcados(repetidorAudios), subtitulos: marcados(repetidorSubtitulos),
                                      calidad: calidad, vcodec: vcodec, acodec: acodec })
        ventana.close()      // vuelve a la página de descargas (como el «atrás»)
    }
    // Si se va de la página sin elegir (atrás o Cancelar), la descarga se cancela
    StackView.onRemoved: if (!respondida) modelo.cancelarEleccion()

    component Titulo: Label {
        Layout.topMargin: 4
        font.pixelSize: 13
        color: ventana.tintaSecundaria
    }

    // ── Calidad y códecs ──
    Seccion {
        titulo: "Calidad"
        visible: pistas.calidades.length > 0

        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: pistas.calidades
                Chip {
                    required property var modelData
                    text: modelData.texto
                    elegido: pagina.calidad === modelData.valor
                    onClicked: pagina.calidad = modelData.valor
                }
            }
        }
        Titulo { text: "Códec de video" }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: pistas.codecsVideo
                Chip {
                    required property string modelData
                    text: pistas.textoCodecVideo(modelData, pagina.calidad)
                    elegido: pagina.vcodec === modelData
                    onClicked: pagina.vcodec = modelData
                }
            }
        }
        Titulo { text: "Códec de audio"; visible: pistas.codecsAudio.length > 1 }
        Flow {
            visible: pistas.codecsAudio.length > 1
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: pistas.codecsAudio
                Chip {
                    required property var modelData
                    text: modelData.texto
                    elegido: pagina.acodec === modelData.valor
                    onClicked: pagina.acodec = modelData.valor
                }
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 4
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: ventana.tintaApagada
            text: pistas.explicacion(pagina.calidad, pagina.vcodec)
        }
    }

    // ── Pistas de audio (la primera marcada es la que suena por defecto) ──
    Seccion {
        titulo: "Pistas de audio"
        padding: 8
        topPadding: 14

        Label {
            visible: pistas.audios.length < 2
            Layout.fillWidth: true
            Layout.leftMargin: 8
            wrapMode: Text.WordWrap
            color: ventana.tintaSecundaria
            text: "El video tiene una sola pista de audio: no hay otras para elegir."
        }
        Repeater {
            id: repetidorAudios
            model: pistas.audios.length > 1 ? pistas.audios : []
            CheckDelegate {
                required property var modelData
                readonly property string idioma: modelData.idioma
                Layout.fillWidth: true
                text: modelData.texto
            }
        }
        Button {
            visible: pistas.audios.length > 1
            Layout.alignment: Qt.AlignRight
            flat: true
            text: "Marcar todas"
            onClicked: {
                let todas = true
                for (let i = 0; i < repetidorAudios.count; ++i) todas = todas && repetidorAudios.itemAt(i).checked
                for (let j = 0; j < repetidorAudios.count; ++j) repetidorAudios.itemAt(j).checked = !todas
            }
        }
    }

    // ── Subtítulos: las traducciones automáticas a otros idiomas (YouTube ofrece más de cien)
    // se ven si se piden, si se las busca o si ya están marcadas ──
    Seccion {
        titulo: "Subtítulos"
        padding: 8
        topPadding: 14

        Label {
            visible: pistas.subtitulos.length === 0
            Layout.fillWidth: true
            Layout.leftMargin: 8
            color: ventana.tintaSecundaria
            text: "El video no ofrece subtítulos."
        }
        TextField {
            id: filtro
            visible: pistas.subtitulos.length > 8
            Layout.fillWidth: true
            Layout.margins: 8
            Material.containerStyle: Material.Outlined
            placeholderText: "Buscar idioma"
            inputMethodHints: Qt.ImhNoPredictiveText
        }
        SwitchDelegate {
            id: todasAutomaticas
            visible: pistas.secundarios > 0
            Layout.fillWidth: true
            text: "Mostrar todas las traducciones automáticas (" + pistas.secundarios + " más)"
        }
        Repeater {
            id: repetidorSubtitulos
            model: pistas.subtitulos
            CheckDelegate {
                required property var modelData
                readonly property string idioma: modelData.idioma
                Layout.fillWidth: true
                text: modelData.texto
                visible: text.toLowerCase().indexOf(filtro.text.trim().toLowerCase()) >= 0
                         && (!modelData.secundario || filtro.text.trim() !== "" || todasAutomaticas.checked || checked)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            text: "Cancelar"
            flat: true
            onClicked: ventana.close()
        }
        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            text: "Descargar"
            icon.source: "qrc:/qt/qml/AndroidManager/iconos/descargar.svg"
            highlighted: true
            onClicked: pagina.aceptar()
        }
    }
}
