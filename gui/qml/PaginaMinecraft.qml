// «Minecraft» (solo Sebastián): prender y apagar la PC, y sus servidores de Minecraft (lo mismo que el
// Panel de Minecraft del Manager de Linux). Tocar un servidor abre su página (consola, plugins, RAM…).
// Todo pasa por el servidor con la clave de la sección Servidor (ver Minecraft.h).
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import Comun.Ui

Hoja {
    id: pagina
    title: "Minecraft"

    // Minecraft.pc: 0 sin saber, 1 consultando, 2 apagada, 3 encendida, 4 prendiendo, 5 apagando
    readonly property int pc: Minecraft.pc
    property var marcados: ({})            // nombre → true (se conservan al actualizar la lista)
    readonly property var listaMarcados: Object.keys(marcados).filter(n => marcados[n])

    function marcar(nombre, si) {
        const copia = Object.assign({}, marcados)
        copia[nombre] = si
        marcados = copia
    }

    Component.onCompleted: if (Minecraft.activo) Minecraft.consultarPc()

    Connections {
        target: Minecraft
        function onAvisar(texto) { ventana.avisar(texto) }
    }

    // ── Sin la sección Servidor no se puede ──
    Pane {
        visible: !Minecraft.activo
        Layout.fillWidth: true
        padding: 16
        background: Rectangle { radius: 14; color: ventana.naranjaSuave }
        contentItem: Label {
            wrapMode: Text.WordWrap
            color: ventana.tinta
            text: "Esta sección usa la clave de la sección Servidor. Activala primero (dejá apretado el logo del menú) y autorizá la clave del celular en el servidor."
        }
    }

    // ── La PC ──
    Seccion {
        visible: Minecraft.activo
        titulo: "PC"

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Icono {
                nombre: "punto"
                color: pagina.pc === 3 ? ventana.verde : pagina.pc === 2 ? ventana.tintaApagada : ventana.naranja
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: ventana.tinta
                font.pixelSize: 16
                text: ["Sin saber", "Consultando…", "Apagada", "Encendida", "Prendiendo…", "Apagándose…"][pagina.pc]
            }
            Button {
                text: "Actualizar"
                flat: true
                enabled: !Minecraft.ocupado && !Minecraft.cargando
                onClicked: Minecraft.consultarPc()
            }
        }
        Label {
            visible: pagina.pc === 3 && !Minecraft.pcApagable
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            color: ventana.tintaSecundaria
            text: "Desde acá solo se apaga si se prendió desde la app (o Claude) en este arranque y nadie la está usando."
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                visible: pagina.pc === 0 || pagina.pc === 2 || pagina.pc === 4
                Layout.fillWidth: true
                text: "Prender"
                highlighted: true
                enabled: !Minecraft.ocupado
                onClicked: Minecraft.prenderPc()
            }
            Button {
                visible: pagina.pc === 3
                Layout.fillWidth: true
                text: "Apagar"
                enabled: !Minecraft.ocupado && Minecraft.pcApagable
                Material.foreground: ventana.rojo
                onClicked: dialogoApagar.open()
            }
        }
    }

    // ── Servidores ──
    Seccion {
        visible: Minecraft.activo && pagina.pc === 3
        titulo: Minecraft.cargando && Minecraft.servidores.length === 0 ? "Servidores (cargando…)" : "Servidores"

        Label {
            visible: Minecraft.servidores.length > 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            color: ventana.tintaSecundaria
            text: "Tocá uno para ver su consola, sus plugins y más. Marcá los que van a recibir Paper nuevo o un plugin."
        }

        Repeater {
            model: Minecraft.servidores
            delegate: ItemDelegate {
                id: fila
                required property var modelData
                Layout.fillWidth: true
                padding: 6
                onClicked: ventana.abrir(Qt.resolvedUrl("PaginaServidorMinecraft.qml"), { nombre: modelData.nombre })
                contentItem: RowLayout {
                    spacing: 8
                    CheckBox {
                        checked: pagina.marcados[fila.modelData.nombre] === true
                        onToggled: pagina.marcar(fila.modelData.nombre, checked)
                    }
                    Icono {
                        nombre: "punto"
                        color: fila.modelData.encendido ? ventana.verde : ventana.tintaApagada
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: fila.modelData.nombre + (fila.modelData.encendido ? "  (encendido)" : "")
                            font.weight: fila.modelData.encendido ? Font.DemiBold : Font.Normal
                            color: ventana.tinta
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: fila.modelData.tipo + " " + fila.modelData.version + " · mundo " + fila.modelData.mundo
                                  + " · " + fila.modelData.ram + " · " + fila.modelData.ultimo
                            font.pixelSize: 12
                            color: ventana.tintaSecundaria
                            elide: Text.ElideRight
                        }
                    }
                    Icono { nombre: "siguiente"; color: ventana.tintaApagada }
                }
            }
        }
    }

    // ── Varios a la vez (como «marcados» en Linux) ──
    Seccion {
        visible: Minecraft.activo && pagina.pc === 3 && Minecraft.servidores.length > 0
        titulo: pagina.listaMarcados.length > 0 ? "Marcados: " + pagina.listaMarcados.length : "Marcados: ninguno"

        Button {
            Layout.fillWidth: true
            text: "Actualizar Paper de los marcados"
            enabled: !Minecraft.ocupado && pagina.listaMarcados.length > 0
            onClicked: dialogoPaper.open()
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: campoPlugin
                Layout.fillWidth: true
                placeholderText: "Plugin de Modrinth (ej. Chunky) o URL"
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            }
            Button {
                text: "Instalar"
                enabled: !Minecraft.ocupado && pagina.listaMarcados.length > 0 && campoPlugin.text.trim() !== ""
                onClicked: { Minecraft.instalarPlugin(campoPlugin.text, pagina.listaMarcados); campoPlugin.text = "" }
            }
        }
    }

    // ── Cómo va ──
    Button {
        visible: Minecraft.ocupado && (Minecraft.cancelable || Minecraft.accionActual === "pc-prender")
        Layout.fillWidth: true
        text: Minecraft.cancelable ? "Cancelar (no apagar)" : "Dejar de esperar"
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
        contentItem: ColumnLayout {
            spacing: 4
            Label {
                Layout.fillWidth: true
                text: Minecraft.avisos
                wrapMode: Text.WrapAnywhere
                font.pixelSize: 12
                color: ventana.tintaSecundaria
            }
            Button {
                Layout.alignment: Qt.AlignRight
                text: "Limpiar"
                flat: true
                onClicked: Minecraft.limpiarAvisos()
            }
        }
    }

    Dialogo {
        id: dialogoApagar
        title: "¿Apagar la PC?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Solo se apaga si no hay ningún servidor de Minecraft prendido, nadie la está usando (ni Steam ni máquinas virtuales). Si no, te dice por qué."
        }
        footer: DialogButtonBox {
            Button { text: "No"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Apagar"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: Minecraft.apagarPc()
    }

    Dialogo {
        id: dialogoPaper
        title: "¿Actualizar Paper?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Se baja la última compilación estable de Paper (de la misma versión de Minecraft) para:\n\n"
                  + pagina.listaMarcados.join("\n") + "\n\nLos encendidos no se tocan y el jar anterior queda en Respaldos."
        }
        footer: DialogButtonBox {
            Button { text: "No"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Actualizar"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: Minecraft.actualizarPaper(pagina.listaMarcados)
    }
}
