// Un servidor de Minecraft de la PC: iniciar, detener y reiniciar (avisando en el chat si hay gente),
// su consola en vivo con comandos, sus plugins y «Más» (RAM, Paper, reiniciar mundo). Lo mismo que el
// Panel de Minecraft de Linux, salvo «Abrir carpeta» y «Jugar con Prism», que son de la PC.
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import Comun.Ui

Hoja {
    id: pagina
    property string nombre
    title: nombre

    readonly property var datos: {
        const lista = Minecraft.servidores
        for (let i = 0; i < lista.length; ++i)
            if (lista[i].nombre === nombre)
                return lista[i]
        return null
    }
    readonly property bool encendido: datos !== null && datos.encendido
    // Minecraft.consola: 0 nada, 1 apagado, 2 solo lectura, 3 acepta comandos
    readonly property bool comandos: Minecraft.consolaDe === nombre && Minecraft.consola === 3
    property int pestana: 0
    property bool reinicio: false          // el diálogo de aviso es para reiniciar (si no, detener)

    Component.onCompleted: Minecraft.verConsola(nombre)
    Component.onDestruction: Minecraft.dejarConsola()

    Connections {
        target: Minecraft
        function onLineaConsola(texto) {
            const alFinal = lista.atYEnd || lista.count === 0
            lineas.append({ texto: texto })
            if (lineas.count > 500)
                lineas.remove(0, lineas.count - 500)
            if (alFinal)
                Qt.callLater(lista.positionViewAtEnd)
        }
        function onConsolaVaciada() { lineas.clear() }
        function onAvisar(texto) { ventana.avisar(texto) }
    }
    ListModel { id: lineas }

    function pedirDetener(esReinicio) {
        reinicio = esReinicio
        if (Minecraft.conectados > 0 && comandos)
            dialogoAviso.open()
        else
            dialogoConfirmar.open()
    }

    // ── Estado y botones ──
    Seccion {
        titulo: pagina.encendido ? "Encendido" : "Apagado"
        Label {
            visible: pagina.datos !== null
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: ventana.tintaSecundaria
            text: pagina.datos ? pagina.datos.tipo + " " + pagina.datos.version + " · mundo " + pagina.datos.mundo
                                 + " · hasta " + pagina.datos.ram + " de RAM · último uso " + pagina.datos.ultimo : ""
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                Layout.fillWidth: true
                text: "Iniciar"
                highlighted: true
                enabled: !Minecraft.ocupado && !pagina.encendido && pagina.datos !== null
                onClicked: Minecraft.iniciar(pagina.nombre)
            }
            Button {
                Layout.fillWidth: true
                text: "Detener"
                enabled: !Minecraft.ocupado && pagina.encendido
                Material.foreground: ventana.rojo
                onClicked: pagina.pedirDetener(false)
            }
            Button {
                Layout.fillWidth: true
                text: "Reiniciar"
                enabled: !Minecraft.ocupado && pagina.encendido
                onClicked: pagina.pedirDetener(true)
            }
        }
    }

    Segmentos {
        Layout.fillWidth: true
        opciones: ["Consola", "Plugins", "Más"]
        elegida: pagina.pestana
        onElegir: indice => {
            pagina.pestana = indice
            if (indice === 1 && Minecraft.pluginsDe !== pagina.nombre)
                Minecraft.consultarPlugins(pagina.nombre)
        }
    }

    // ── Consola ──
    ColumnLayout {
        visible: pagina.pestana === 0
        Layout.fillWidth: true
        spacing: 8

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: ventana.tinta
            text: Minecraft.consolaDe === pagina.nombre ? Minecraft.jugadores : ""
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(260, ventana.height * 0.45)
            radius: 12
            color: ventana.superficie
            border.color: ventana.borde
            clip: true
            ListView {
                id: lista
                anchors.fill: parent
                anchors.margins: 8
                model: lineas
                boundsBehavior: Flickable.StopAtBounds
                delegate: Label {
                    required property string texto
                    width: ListView.view.width
                    text: texto
                    wrapMode: Text.WrapAnywhere
                    font.family: "monospace"
                    font.pixelSize: 11
                    color: ventana.tinta
                }
                ScrollBar.vertical: ScrollBar {}
            }
            Label {
                anchors.centerIn: parent
                visible: lineas.count === 0
                text: "Sin registro todavía"
                color: ventana.tintaApagada
            }
        }
        // Comandos frecuentes: completan el campo (no se mandan solos)
        Flow {
            Layout.fillWidth: true
            spacing: 6
            visible: pagina.comandos
            Repeater {
                model: ["list", "save-all", "time set day", "weather clear", "say "]
                delegate: Chip {
                    required property string modelData
                    text: modelData.trim()
                    onClicked: { campoComando.text = modelData; campoComando.forceActiveFocus() }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: campoComando
                Layout.fillWidth: true
                enabled: pagina.comandos
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                placeholderText: pagina.comandos ? "Comando (ej. say hola)"
                               : Minecraft.consolaDe === pagina.nombre && Minecraft.consola === 2
                                 ? "Se prendió sin consola compartida: reinicialo para mandarle comandos"
                                 : "Apagado: no recibe comandos"
                onAccepted: enviar.clicked()
            }
            Button {
                id: enviar
                text: "Enviar"
                enabled: pagina.comandos && !Minecraft.enviando && campoComando.text.trim() !== ""
                onClicked: { Minecraft.enviarComando(campoComando.text); campoComando.text = "" }
            }
        }
    }

    // ── Plugins ──
    ColumnLayout {
        visible: pagina.pestana === 1
        Layout.fillWidth: true
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: ventana.tintaSecundaria
                text: Minecraft.buscandoPlugins ? "Consultando Modrinth…"
                      : Minecraft.pluginsNuevos > 0 ? Minecraft.pluginsNuevos + " con versión nueva" : "Se reconocen en Modrinth"
            }
            Button {
                text: "Buscar"
                flat: true
                enabled: !Minecraft.buscandoPlugins
                onClicked: Minecraft.consultarPlugins(pagina.nombre)
            }
        }
        Repeater {
            model: Minecraft.pluginsDe === pagina.nombre ? Minecraft.plugins : []
            delegate: Pane {
                required property var modelData
                Layout.fillWidth: true
                padding: 10
                background: Rectangle { radius: 10; color: ventana.superficie; border.color: ventana.borde }
                contentItem: ColumnLayout {
                    spacing: 2
                    Label { Layout.fillWidth: true; text: modelData.nombre; color: ventana.tinta; elide: Text.ElideRight }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        text: modelData.estado === "NUEVA" ? modelData.version + " → " + modelData.nueva
                              : modelData.estado === "AL_DIA" ? modelData.version + " · al día"
                              : modelData.estado === "SIN_VERSION" ? modelData.version + " · sin versión para esta versión de Minecraft"
                              : modelData.version + " · no está en Modrinth"
                        color: modelData.estado === "NUEVA" ? ventana.naranja : modelData.estado === "AL_DIA" ? ventana.verde : ventana.tintaSecundaria
                    }
                }
            }
        }
        Label {
            visible: !Minecraft.buscandoPlugins && Minecraft.pluginsDe === pagina.nombre && Minecraft.plugins.length === 0
            text: "No tiene plugins."
            color: ventana.tintaApagada
        }
        Button {
            Layout.fillWidth: true
            visible: Minecraft.pluginsNuevos > 0 && Minecraft.pluginsDe === pagina.nombre
            text: pagina.encendido ? "Detenelo para actualizar los plugins" : "Actualizar " + Minecraft.pluginsNuevos + " plugin(s)"
            enabled: !pagina.encendido && !Minecraft.ocupado && !Minecraft.buscandoPlugins
            onClicked: Minecraft.actualizarPlugins(pagina.nombre)
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: campoPlugin
                Layout.fillWidth: true
                placeholderText: "Plugin nuevo (nombre en Modrinth o URL)"
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            }
            Button {
                text: "Instalar"
                enabled: !Minecraft.ocupado && campoPlugin.text.trim() !== ""
                onClicked: { Minecraft.instalarPlugin(campoPlugin.text, [pagina.nombre]); campoPlugin.text = "" }
            }
        }
    }

    // ── Más ──
    ColumnLayout {
        visible: pagina.pestana === 2
        Layout.fillWidth: true
        spacing: 10

        Seccion {
            titulo: "Memoria máxima"
            RowLayout {
                Layout.fillWidth: true
                SpinBox {
                    id: ram
                    from: 1
                    to: 24
                    value: pagina.datos ? parseInt(pagina.datos.ram) || 8 : 8
                    textFromValue: (v) => v + " GB"
                    valueFromText: (t) => parseInt(t)
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Guardar"
                    enabled: !Minecraft.ocupado && pagina.datos !== null && ram.value + "G" !== pagina.datos.ram
                    onClicked: Minecraft.cambiarRam(pagina.nombre, ram.value)
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: ventana.tintaSecundaria
                text: "Se usa la próxima vez que se inicie."
            }
        }
        Button {
            Layout.fillWidth: true
            text: "Actualizar Paper"
            enabled: !Minecraft.ocupado && !pagina.encendido
            onClicked: dialogoPaper.open()
        }
        Button {
            Layout.fillWidth: true
            text: "Reiniciar mundo…"
            enabled: !Minecraft.ocupado && !pagina.encendido
            Material.foreground: ventana.rojo
            onClicked: dialogoMundo.open()
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: ventana.tintaApagada
            text: "«Abrir carpeta» y «Jugar con Prism» están en el Manager de la PC."
        }
    }

    // ── Cómo va ──
    Button {
        visible: Minecraft.ocupado && Minecraft.cancelable
        Layout.fillWidth: true
        text: "Cancelar (no apagar)"
        flat: true
        onClicked: Minecraft.cancelar()
    }
    TarjetaEstado {
        texto: Minecraft.estado
        tipo: Minecraft.tipoEstado
        trabajando: Minecraft.ocupado
        progreso: Minecraft.progreso
    }
    Pane {
        visible: Minecraft.avisos !== ""
        Layout.fillWidth: true
        padding: 12
        background: Rectangle { radius: 12; color: ventana.superficie; border.color: ventana.borde }
        contentItem: Label {
            text: Minecraft.avisos
            wrapMode: Text.WrapAnywhere
            font.pixelSize: 12
            color: ventana.tintaSecundaria
        }
    }

    // ── Diálogos ──
    Dialogo {
        id: dialogoAviso
        title: pagina.reinicio ? "¿Reiniciar " + pagina.nombre + "?" : "¿Detener " + pagina.nombre + "?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: (Minecraft.conectados === 1 ? "Hay 1 jugador conectado." : "Hay " + Minecraft.conectados + " jugadores conectados.")
                  + " Con el aviso, el chat cuenta un minuto y se puede cancelar mientras tanto."
        }
        footer: DialogButtonBox {
            Button { text: "Cancelar"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button {
                text: "Ya"
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
            }
            Button { text: "Avisar 1 min"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onApplied: {
            close()
            if (pagina.reinicio) Minecraft.reiniciar(pagina.nombre, 0)
            else Minecraft.detener(pagina.nombre, 0)
        }
        onAccepted: {
            if (pagina.reinicio) Minecraft.reiniciar(pagina.nombre, 60)
            else Minecraft.detener(pagina.nombre, 60)
        }
    }
    Dialogo {
        id: dialogoConfirmar
        title: pagina.reinicio ? "¿Reiniciar " + pagina.nombre + "?" : "¿Detener " + pagina.nombre + "?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: pagina.reinicio ? "Se detiene guardando los mundos y se vuelve a prender."
                                  : "Se apaga guardando los mundos (los jugadores se desconectan)."
        }
        footer: DialogButtonBox {
            Button { text: "No"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: pagina.reinicio ? "Reiniciar" : "Detener"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: {
            if (pagina.reinicio) Minecraft.reiniciar(pagina.nombre, 0)
            else Minecraft.detener(pagina.nombre, 0)
        }
    }
    Dialogo {
        id: dialogoPaper
        title: "¿Actualizar Paper?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Se baja la última compilación estable de Paper " + (pagina.datos ? pagina.datos.version : "") + ". El jar anterior queda en Respaldos."
        }
        footer: DialogButtonBox {
            Button { text: "No"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Actualizar"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: Minecraft.actualizarPaper([pagina.nombre])
    }
    Dialogo {
        id: dialogoMundo
        title: "¿Reiniciar el mundo?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "El mundo de " + pagina.nombre + " (con el Nether y el End) se guarda comprimido en Respaldos y se borra: al iniciarlo se genera uno nuevo."
        }
        footer: DialogButtonBox {
            Button { text: "No"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Reiniciar mundo"; flat: true; Material.foreground: ventana.rojo; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: Minecraft.reiniciarMundo(pagina.nombre)
    }
}
