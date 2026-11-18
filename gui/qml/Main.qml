// Ventana principal: barra arriba y una pila de páginas (portada → herramienta). Las 3 rayitas abren
// el menú lateral, como en las otras apps (con un puntito si hay una versión nueva). El «atrás» del
// celular cierra el menú o vuelve a la página anterior; en la portada cierra la app (preguntando si hay una tarea).
// Colores como Ingresos y Gastos: Material del sistema (claro u oscuro) con acento #2a78d6.
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import Comun.Ui

ApplicationWindow {
    id: ventana
    width: 412
    height: 880
    visible: true
    title: "Android-Manager"

    Material.theme: Tema.oscuro ? Material.Dark : Material.Light   // claro u oscuro: lo elige el menú (Tema.alternar)
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
        if (menu.opened) {
            menu.close()
            cierre.accepted = false
        } else if (pila.depth > 1) {
            pila.pop()
            cierre.accepted = false
        } else if (Sistema.hayTarea) {
            dialogoSalir.open()
            cierre.accepted = false
        }
    }

    // Mensajito abajo («Copiado», «Borrado»…): AvisoFlotante de Comun.Ui
    function avisar(texto) { aviso.mostrar(texto) }
    AvisoFlotante { id: aviso }
    // «Apps y actualizaciones» es la página común de Comun.Ui (no un archivo de esta app)
    Component { id: paginaApps; PaginaApps {} }

    header: ToolBar {
        id: barra
        Material.foreground: "white"
        topPadding: SafeArea.margins.top      // la barra de estado del celular queda sobre el azul
        contentHeight: 56                     // igual con o sin el botón de volver
        RowLayout {
            anchors.fill: parent
            spacing: 0
            ToolButton {
                visible: pila.depth > 1
                icon.source: "qrc:/qt/qml/AndroidManager/iconos/volver.svg"
                onClicked: ventana.close()     // lo mismo que el «atrás» del celular
                Accessible.name: "Volver"
            }
            BotonMenu {
                visible: pila.depth === 1
                punto: Apps.versionNueva !== ""
                onClicked: menu.open()
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

    // ── Menú lateral (como en las otras apps) ──
    Drawer {
        id: menu
        width: Math.min(ventana.width * 0.82, 340)
        height: ventana.height
        interactive: pila.depth === 1          // en una herramienta, deslizar desde el borde no lo abre
        background: Rectangle { color: ventana.superficie }

        // Abre una herramienta desde el menú (siempre se está en la portada)
        function ir(pagina, propiedades) { close(); ventana.abrir(pagina, propiedades) }

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 8                       // el Drawer ya se corre debajo de la barra de estado
            anchors.bottomMargin: menu.SafeArea.margins.bottom + 12
            spacing: 2
            RowLayout {
                Layout.leftMargin: 20
                Layout.rightMargin: 16
                Layout.bottomMargin: 12
                spacing: 12
                Image {
                    source: "qrc:/qt/qml/AndroidManager/Logo.png"
                    sourceSize.height: 96
                    Layout.preferredHeight: 48
                    Layout.preferredWidth: 48 * implicitWidth / Math.max(1, implicitHeight)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
                Label {
                    Layout.fillWidth: true
                    text: "Android-Manager"
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    color: ventana.tinta
                    elide: Text.ElideRight
                }
            }
            Label {
                Layout.leftMargin: 20
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                text: "HERRAMIENTAS"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.letterSpacing: 0.8
                color: ventana.tintaApagada
            }
            ItemMenu {
                text: "Videos"
                icono: "video"
                enabled: Herramientas.listas
                onClicked: menu.ir(Qt.resolvedUrl("PaginaDescargas.qml"), { modelo: Videos })
            }
            ItemMenu {
                text: "Música"
                icono: "musica"
                enabled: Herramientas.listas
                onClicked: menu.ir(Qt.resolvedUrl("PaginaDescargas.qml"), { modelo: Musica })
            }
            ItemMenu {
                text: "Respaldo"
                icono: "respaldo"
                enabled: Herramientas.listas
                onClicked: menu.ir(Qt.resolvedUrl("PaginaRespaldo.qml"))
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                Layout.preferredHeight: 1
                color: ventana.borde
            }
            ItemMenu {
                text: "Apps y actualizaciones"
                icono: "paquete"
                destacado: Apps.versionNueva !== ""
                onClicked: menu.ir(paginaApps)
            }
            ItemMenu {   // el menú queda abierto: se ve el cambio enseguida
                text: Tema.textoAlternar
                icono: Tema.iconoAlternar
                onClicked: Tema.alternar()
            }
            Item { Layout.fillHeight: true }
            Label {
                Layout.leftMargin: 20
                text: "Versión " + Sistema.version
                font.pixelSize: 12
                color: ventana.tintaApagada
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

    // ¿Hay una versión nueva? Se busca cada vez que se abre (marca el menú); pregunta como mucho una vez por día
    Timer { interval: 4000; running: true; onTriggered: Apps.buscarActualizacion(false) }
    Connections {
        target: Apps
        function onAvisoVersionNueva() { if (pila.depth === 1) dialogoVersion.open() }
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
        onAccepted: { Apps.actualizarEstaApp(); ventana.abrir(paginaApps) }
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
