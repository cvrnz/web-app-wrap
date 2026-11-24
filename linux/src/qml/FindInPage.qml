import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtWebEngine

// Find on the page. Enter goes to the next match, Shift+Enter to the previous one, and Esc
// closes it.
Pane {
    id: findBar

    property var view
    property int matches: 0
    property int active: 0

    function open() {
        visible = true
        field.selectAll()
        field.forceActiveFocus()
        search(false)
    }

    function close() {
        visible = false
        if (view) {
            view.findText("")
            view.forceActiveFocus()
        }
    }

    function search(backward) {
        if (view)
            view.findText(field.text, backward ? WebEngineView.FindBackward : 0)
    }

    function showResult(result) {
        matches = result.numberOfMatches
        active = result.activeMatch
    }

    visible: false
    padding: 6
    Material.elevation: 6
    background: Rectangle {
        color: Material.dialogColor
        radius: Math.min(8, theme.radius)
        border.color: Qt.rgba(0.5, 0.5, 0.5, 0.3)
    }

    onViewChanged: if (visible) search(false)

    RowLayout {
        spacing: 4

        TextField {
            id: field
            Layout.preferredWidth: 220
            placeholderText: qsTr("Find on page")
            selectByMouse: true
            onTextEdited: findBar.search(false)
            Keys.onReturnPressed: (event) => findBar.search(event.modifiers & Qt.ShiftModifier)
            Keys.onEnterPressed: (event) => findBar.search(event.modifiers & Qt.ShiftModifier)
            Keys.onEscapePressed: findBar.close()
        }

        Label {
            Layout.minimumWidth: 60
            horizontalAlignment: Text.AlignHCenter
            text: field.text === "" ? "" : findBar.matches === 0 ? qsTr("No matches")
                                                                : qsTr("%1 of %2").arg(findBar.active).arg(findBar.matches)
            opacity: 0.7
        }

        ToolButton { text: "↑"; onClicked: findBar.search(true); Accessible.name: qsTr("Previous Match") }
        ToolButton { text: "↓"; onClicked: findBar.search(false); Accessible.name: qsTr("Next Match") }
        ToolButton { text: "×"; onClicked: findBar.close(); Accessible.name: qsTr("Close") }
    }
}
