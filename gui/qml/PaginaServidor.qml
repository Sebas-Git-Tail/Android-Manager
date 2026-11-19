// «Servidor» (solo Sebastián): las acciones que corren en el servidor con la clave limitada del
// celular. Sin activar, explica qué es y ofrece activarla; activada, muestra la clave pública (para
// autorizarla en el servidor con autorizar-equipo.sh) y los botones de cada acción.
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import Comun.Ui

Hoja {
    id: pagina
    title: "Servidor"

    Connections {
        target: Servidor
        function onAvisar(texto) { ventana.avisar(texto) }
    }

    // ── Qué es ──
    Pane {
        Layout.fillWidth: true
        padding: 16
        background: Rectangle { radius: 14; color: ventana.azulSuave }
        contentItem: Label {
            wrapMode: Text.WordWrap
            color: ventana.tinta
            text: Servidor.activado
                  ? "Estas acciones corren en el servidor. Este celular entra con su propia clave, que allá solo puede pedir estas acciones: ni siquiera abre una terminal."
                  : "Corre en el servidor las miniaturas de Bodrio y las páginas del sitio. Al activarla, el celular crea su propia clave; hasta que la autorices en el servidor, no puede hacer nada."
        }
    }

    Pane {
        visible: !Servidor.disponible
        Layout.fillWidth: true
        padding: 14
        background: Rectangle { radius: 14; color: ventana.naranjaSuave }
        contentItem: Label {
            wrapMode: Text.WordWrap
            color: ventana.tinta
            text: "Esta versión de la app no incluye el cliente SSH."
        }
    }

    Button {
        visible: !Servidor.activado
        Layout.fillWidth: true
        Layout.preferredHeight: 56
        enabled: Servidor.disponible
        text: "Activar en este celular"
        icon.source: "qrc:/qt/qml/AndroidManager/iconos/escudo.svg"
        highlighted: true
        font.pixelSize: 16
        onClicked: Servidor.activar()
    }

    // ── La clave del celular ──
    Seccion {
        visible: Servidor.activado
        titulo: Servidor.autorizado ? "Clave de este celular (autorizada)" : "Clave de este celular"

        Label {
            visible: !Servidor.autorizado
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            color: ventana.tintaSecundaria
            text: "Para autorizarla, en el servidor:\nautorizar-equipo.sh celular '<la clave>'"
        }
        Label {
            Layout.fillWidth: true
            text: Servidor.clavePublica
            wrapMode: Text.WrapAnywhere
            font.pixelSize: 12
            color: ventana.azul
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "Copiar clave"
                flat: true
                icon.source: "qrc:/qt/qml/AndroidManager/iconos/pegar.svg"
                onClicked: Servidor.copiarClave()
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "Desactivar"
                flat: true
                enabled: !Servidor.ocupado
                Material.foreground: ventana.rojo
                onClicked: dialogoDesactivar.open()
            }
        }
    }

    // ── Acciones ──
    Repeater {
        model: Servidor.activado ? [
            { accion: "miniaturas-bodrio", icono: "video", titulo: "Miniaturas de Bodrio",
              detalle: "Crea las miniaturas que falten de los videos de Bodrio." },
            { accion: "descargas", icono: "descargar", titulo: "Páginas de descargas",
              detalle: "Rehace las páginas de Música, Android y Minecraft con lo que haya en esas carpetas." },
            { accion: "indices-instituto", icono: "libro", titulo: "Páginas del Instituto",
              detalle: "Rehace el índice de cada carpeta del Instituto." }
        ] : []
        delegate: TarjetaHerramienta {
            required property var modelData
            icono: modelData.icono
            titulo: modelData.titulo
            detalle: modelData.detalle
            ocupada: Servidor.accionActual === modelData.accion
            enabled: !Servidor.ocupado
            onClicked: Servidor.ejecutar(modelData.accion)
        }
    }

    Button {
        visible: Servidor.ocupado
        Layout.fillWidth: true
        text: "Cancelar"
        flat: true
        onClicked: Servidor.cancelar()
    }

    TarjetaEstado {
        texto: Servidor.estado
        tipo: Servidor.tipoEstado
        trabajando: Servidor.ocupado
    }

    // Lo que fue diciendo el servidor (p. ej. cada miniatura creada)
    Pane {
        visible: Servidor.salida !== ""
        Layout.fillWidth: true
        padding: 12
        background: Rectangle { radius: 12; color: ventana.superficie; border.color: ventana.borde }
        contentItem: Label {
            text: Servidor.salida
            wrapMode: Text.WrapAnywhere
            font.pixelSize: 12
            color: ventana.tintaSecundaria
        }
    }

    Dialogo {
        id: dialogoDesactivar
        title: "¿Desactivar la sección?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Se borra la clave de este celular. Para volver a usarla vas a tener que autorizar una nueva en el servidor (y conviene quitar la vieja con autorizar-equipo.sh --quitar)."
        }
        footer: DialogButtonBox {
            Button { text: "No"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Desactivar"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: Servidor.desactivar()
    }
}
