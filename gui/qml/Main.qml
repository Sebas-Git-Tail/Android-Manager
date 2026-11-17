// Ventana principal: barra arriba y una pila de páginas (portada → herramienta). El «atrás» del
// celular vuelve a la página anterior; en la portada cierra la app (preguntando si hay una tarea).
// Colores como Ingresos y Gastos: Material del sistema (claro u oscuro) con acento #2a78d6.
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: ventana
    width: 412
    height: 880
    visible: true
    title: "Android-Manager"

    Material.theme: Material.System
    Material.accent: acento
    Material.primary: "#2a78d6"

    readonly property bool oscuro: Material.theme === Material.Dark
    readonly property color fondo: oscuro ? "#111110" : "#f3f3f1"
    readonly property color superficie: oscuro ? "#1c1c1b" : "#ffffff"
    readonly property color borde: oscuro ? "#2c2c2a" : "#e1e0d9"
    readonly property color tinta: oscuro ? "#f0efec" : "#1b1b1a"
    readonly property color tintaSecundaria: oscuro ? "#c3c2b7" : "#52514e"
    readonly property color tintaApagada: "#898781"
    readonly property color azul: oscuro ? "#5b9cec" : "#2a78d6"
    readonly property color acento: oscuro ? "#3580dc" : "#2a78d6"       // relleno de botones (texto blanco encima)
    readonly property color azulSuave: oscuro ? "#1a2a3d" : "#e3eefb"
    readonly property color verde: oscuro ? "#4cc38a" : "#1f8a54"
    readonly property color rojo: oscuro ? "#f07070" : "#d23c3b"
    readonly property color rojoRelleno: oscuro ? "#c43c3c" : "#d23c3b"   // botones rellenos (texto blanco encima)
    readonly property color verdeRelleno: oscuro ? "#24935c" : "#1f8a54"
    readonly property color naranja: oscuro ? "#f0954f" : "#c4621c"
    readonly property color naranjaSuave: oscuro ? "#3a2616" : "#fdeedd"
    readonly property int margen: 16

    color: fondo

    // Color del texto de estado según su tipo (0 info, 1 éxito, 2 error)
    function colorEstado(tipo) { return tipo === 1 ? verde : tipo === 2 ? rojo : tintaSecundaria }

    function abrir(pagina, propiedades) { pila.push(pagina, propiedades || {}) }

    // «Atrás»: vuelve a la página anterior; en la portada, si hay una tarea, pregunta antes de salir
    onClosing: (cierre) => {
        if (pila.depth > 1) {
            pila.pop()
            cierre.accepted = false
        } else if (Sistema.hayTarea) {
            dialogoSalir.open()
            cierre.accepted = false
        }
    }

    // Mensajito abajo («Copiado», «Borrado»…)
    function avisar(texto) { aviso.texto = texto; aviso.open() }
    Popup {
        id: aviso
        property string texto
        x: (ventana.width - width) / 2
        y: ventana.height - height - 48
        width: Math.min(ventana.width - 32, etiquetaAviso.implicitWidth + 32)
        padding: 14
        modal: false
        closePolicy: Popup.CloseOnPressOutside
        background: Rectangle { radius: 10; color: ventana.oscuro ? "#f0efec" : "#2c2c2a" }
        contentItem: Label {
            id: etiquetaAviso
            text: aviso.texto
            wrapMode: Text.WordWrap
            color: ventana.oscuro ? "#0b0b0b" : "#f0efec"
        }
        onOpened: cerrarAviso.restart()
        Timer { id: cerrarAviso; interval: 3000; onTriggered: aviso.close() }
    }

    header: ToolBar {
        Material.foreground: "white"
        topPadding: SafeArea.margins.top      // la barra de estado del celular queda sobre el azul
        contentHeight: 56                     // igual con o sin el botón de volver
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: pila.depth > 1 ? 0 : 12
            spacing: 0
            ToolButton {
                visible: pila.depth > 1
                icon.source: "qrc:/qt/qml/AndroidManager/iconos/volver.svg"
                onClicked: ventana.close()     // lo mismo que el «atrás» del celular
                Accessible.name: "Volver"
            }
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 4
                text: pila.currentItem && pila.currentItem.title ? pila.currentItem.title : ventana.title
                font.pixelSize: 20
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            BusyIndicator {   // hay algo en curso (se ve desde cualquier página)
                visible: Sistema.hayTarea
                running: visible
                implicitWidth: 36
                implicitHeight: 36
                Material.accent: "white"
                Layout.rightMargin: 8
            }
        }
    }

    StackView {
        id: pila
        anchors.fill: parent
        initialItem: Portada {}
    }

    // La consulta de Videos terminó y hay calidades, pistas o subtítulos para elegir
    Connections {
        target: Videos
        function onPedirEleccion() { ventana.abrir(Qt.resolvedUrl("PaginaPistas.qml"), { modelo: Videos }) }
    }

    Component.onCompleted: {
        if (Sistema.paginaPedida === "respaldo")     // abierta desde la notificación del recordatorio
            abrir(Qt.resolvedUrl("PaginaRespaldo.qml"))
    }

    // ¿Hay una versión nueva? (una vez por día): pregunta y abre «Apps y actualizaciones»
    Timer { interval: 4000; running: true; onTriggered: Apps.buscarActualizacion(false) }
    Connections {
        target: Apps
        function onVersionNuevaCambio() { if (Apps.versionNueva !== "" && pila.depth === 1) dialogoVersion.open() }
    }

    Dialogo {
        id: dialogoVersion
        title: "Versión nueva"
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Hay una versión nueva de Android-Manager: la " + Apps.versionNueva + " (tenés la " + Apps.versionPropia + ").\n\n¿La instalo ahora?"
        }
        onAccepted: { Apps.actualizarEstaApp(); ventana.abrir(Qt.resolvedUrl("PaginaApps.qml")) }
    }

    Dialogo {
        id: dialogoSalir
        title: "¿Salir?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Hay una descarga o un respaldo en curso. ¿Cancelarlo y salir?"
        }
        footer: DialogButtonBox {
            Button { text: "Seguir"; flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: "Cancelar y salir"; flat: true; Material.foreground: ventana.rojo; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: { Sistema.cancelarTodo(); Qt.quit() }
    }
}
