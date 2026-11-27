// «Respaldo»: a qué carpeta del servidor va, la contraseña SSH y el botón. Por --delete, lo que
// se borró del celular también se borra del respaldo (queda 30 días en «anteriores»): la primera vez (y
// cada vez que cambia el destino) se pregunta si la carpeta es la correcta. Abajo, «Restaurar»: traer al
// celular lo que hay en el servidor (todo, una carpeta o lo de «anteriores» de una fecha).
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts
import Comun.Ui

Hoja {
    id: pagina
    title: "Respaldo"

    Connections {
        target: Respaldo
        function onPedirConfirmacion(ruta) { dialogoDestino.ruta = ruta; dialogoDestino.open() }
        function onContrasenaUsada() { campoContrasena.text = "" }
        function onRestauracionCambio() { seccionRestaurar.version = ""; seccionRestaurar.carpeta = "" }
        function onEnfocar(campo) {
            const campos = { persona: campoPersona, celular: campoCelular, contrasena: campoContrasena }
            if (campo === "contrasena") campoContrasena.text = ""
            if (campos[campo]) campos[campo].forceActiveFocus()
        }
    }

    // ── Qué hace ──
    Pane {
        Layout.fillWidth: true
        padding: 16
        background: Rectangle { radius: 14; color: ventana.azulSuave }
        contentItem: ColumnLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: ventana.tinta
                text: "Copia el almacenamiento interno al servidor" + (Respaldo.destino !== "" ? ", a " : ", a la carpeta de la persona.")
            }
            Label {
                visible: Respaldo.destino !== ""
                Layout.fillWidth: true
                text: Respaldo.destino
                wrapMode: Text.WrapAnywhere
                font.pixelSize: 13
                color: ventana.azul
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 13
                color: ventana.tintaSecundaria
                text: "Sin Android/data ni Android/obb. Lo que borraste del celular también sale del respaldo, pero queda guardado 30 días aparte («anteriores»), por si hace falta."
            }
            RowLayout {
                spacing: 8
                IconImage {
                    source: "qrc:/qt/qml/AndroidManager/iconos/reloj.svg"
                    sourceSize: Qt.size(16, 16)
                    color: ventana.tintaSecundaria
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                    color: ventana.tintaSecundaria
                    text: Respaldo.ultimoRespaldo
                }
            }
        }
    }

    Pane {
        visible: !Respaldo.disponible
        Layout.fillWidth: true
        padding: 14
        background: Rectangle { radius: 14; color: ventana.naranjaSuave }
        contentItem: Label {
            wrapMode: Text.WordWrap
            color: ventana.tinta
            text: "Esta versión de la app no incluye rsync ni el cliente SSH: el respaldo no está disponible."
        }
    }

    // ── Destino ──
    Seccion {
        titulo: "Destino"
        enabled: !Respaldo.ocupada

        TextField {
            id: campoPersona
            Layout.fillWidth: true
            Material.containerStyle: Material.Outlined
            placeholderText: "¿De quién es este celular?"
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            text: Respaldo.persona
            onTextChanged: if (text !== Respaldo.persona) Respaldo.persona = text
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: -4
            Layout.bottomMargin: 8
            text: "Tu nombre, en una palabra."
            font.pixelSize: 12
            color: ventana.tintaApagada
        }
        TextField {
            id: campoCelular
            Layout.fillWidth: true
            Material.containerStyle: Material.Outlined
            placeholderText: "Nombre del celular"
            inputMethodHints: Qt.ImhNoPredictiveText
            text: Respaldo.celular
            onTextChanged: if (text !== Respaldo.celular) Respaldo.celular = text
        }
    }

    // ── Servidor ──
    Seccion {
        titulo: "Servidor"
        enabled: !Respaldo.ocupada

        TextField {
            id: campoContrasena
            Layout.fillWidth: true
            Material.containerStyle: Material.Outlined
            placeholderText: "Contraseña SSH del servidor"
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            onAccepted: Respaldo.empezar(text)
        }
        SwitchDelegate {
            Layout.fillWidth: true
            leftPadding: 0; rightPadding: 0
            text: "Solo simular (no copia ni borra nada)"
            checked: Respaldo.simular
            onToggled: Respaldo.simular = checked
        }
    }

    Button {
        Layout.fillWidth: true
        Layout.preferredHeight: 56
        enabled: Respaldo.disponible
        text: Respaldo.ocupada ? "Cancelar" : (Respaldo.simular ? "Simular respaldo" : "Empezar respaldo")
        icon.source: "qrc:/qt/qml/AndroidManager/iconos/" + (Respaldo.ocupada ? "cerrar" : "respaldo") + ".svg"
        highlighted: true
        Material.accent: Respaldo.ocupada ? ventana.rojoRelleno : ventana.verdeRelleno
        font.pixelSize: 16
        onClicked: Respaldo.ocupada ? Respaldo.cancelar() : Respaldo.empezar(campoContrasena.text)
    }

    // ── Restaurar: del servidor al celular ──
    Seccion {
        id: seccionRestaurar
        titulo: "Restaurar"
        enabled: !Respaldo.ocupada
        property string version: ""     // "" = el respaldo actual
        property string carpeta: ""     // "" = todo

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            color: ventana.tintaSecundaria
            text: "Trae al celular lo que está en el servidor. No borra nada del celular ni reemplaza lo que acá sea más nuevo. Usa la contraseña de arriba."
        }
        Button {
            text: Respaldo.listado ? "Volver a mirar el servidor" : "Ver qué hay en el servidor"
            flat: Respaldo.listado
            highlighted: !Respaldo.listado
            enabled: Respaldo.disponible
            onClicked: Respaldo.listar(campoContrasena.text)
        }
        Label { visible: Respaldo.listado; text: "Desde"; color: ventana.tintaSecundaria; font.pixelSize: 13 }
        ComboBox {
            visible: Respaldo.listado
            Layout.fillWidth: true
            model: ["El respaldo de ahora"].concat(Respaldo.versiones.map(v => "Lo que salió el " + v.texto))
            onActivated: (i) => { seccionRestaurar.version = i === 0 ? "" : Respaldo.versiones[i - 1].nombre; seccionRestaurar.carpeta = "" }
        }
        Label {
            visible: Respaldo.listado && seccionRestaurar.version !== ""
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: ventana.tintaApagada
            text: "Son los archivos que ese respaldo sacó (porque ya no estaban en el celular) o reemplazó por otra versión."
        }
        Label { visible: Respaldo.listado; text: "Qué"; color: ventana.tintaSecundaria; font.pixelSize: 13 }
        ComboBox {
            id: eleccionCarpeta
            visible: Respaldo.listado
            Layout.fillWidth: true
            readonly property var carpetas: Respaldo.listado ? Respaldo.carpetas(seccionRestaurar.version) : []
            model: ["Todo"].concat(carpetas)
            onActivated: (i) => seccionRestaurar.carpeta = i === 0 ? "" : carpetas[i - 1]
        }
        Button {
            visible: Respaldo.listado
            Layout.fillWidth: true
            text: "Restaurar…"
            icon.source: "qrc:/qt/qml/AndroidManager/iconos/descargar.svg"
            highlighted: true
            Material.accent: ventana.acento
            onClicked: {
                dialogoRestaurar.texto = Respaldo.descripcionRestauracion(seccionRestaurar.version, seccionRestaurar.carpeta)
                dialogoRestaurar.open()
            }
        }
    }

    TarjetaEstado {
        texto: Respaldo.estado
        tipo: Respaldo.tipoEstado
        trabajando: Respaldo.ocupada
        progreso: Respaldo.progreso
        detalle: Respaldo.archivo
    }

    Avisos {
        texto: Respaldo.avisos
        cantidad: Respaldo.cantidadAvisos
        onLimpiar: Respaldo.limpiarAvisos()
    }

    Dialogo {
        id: dialogoDestino
        property string ruta
        title: "¿Es la carpeta correcta?"
        ColumnLayout {
            width: parent.width
            spacing: 10
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "El respaldo va a ir a:" }
            Label {
                Layout.fillWidth: true
                text: dialogoDestino.ruta
                wrapMode: Text.WrapAnywhere
                color: ventana.azul
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Lo que haya en esa carpeta y no esté en este celular sale del respaldo (queda 30 días en «anteriores»)."
            }
        }
        footer: DialogButtonBox {
            Button { text: "No"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Sí, respaldar"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: Respaldo.confirmarDestino(true)
        onRejected: Respaldo.confirmarDestino(false)
    }

    Dialogo {
        id: dialogoRestaurar
        property string texto
        title: "¿Restaurar?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: dialogoRestaurar.texto + "\n\nNo se borra nada del celular, y lo que acá sea más nuevo no se reemplaza."
        }
        footer: DialogButtonBox {
            Button { text: "No"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Sí, restaurar"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: Respaldo.restaurar(campoContrasena.text, seccionRestaurar.version, seccionRestaurar.carpeta)
    }
}
