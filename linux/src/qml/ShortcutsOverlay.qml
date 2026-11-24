import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every keyboard shortcut, in two columns. Any key or click closes it.
Popup {
    id: overlay

    // [keys, what they do] pairs.
    property var entries: []
    property string footer: ""

    modal: true
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }
    focus: true
    padding: 20
    width: Math.min(640, parent ? parent.width - 32 : 640)
    height: Math.min(implicitHeight, parent ? parent.height - 32 : implicitHeight)

    background: Rectangle {
        color: Material.dialogColor
        radius: Math.min(12, theme.radius)
    }

    contentItem: Flickable {
        focus: true
        Keys.onPressed: (event) => {
            overlay.close()
            event.accepted = true
        }
        implicitHeight: column.implicitHeight
        contentHeight: column.implicitHeight
        clip: true

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 12

            Label {
                text: qsTr("Keyboard Shortcuts")
                font.pixelSize: 18
                font.bold: true
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 20
                rowSpacing: 6

                Repeater {
                    // The pairs, flattened, so the grid fills row by row.
                    model: overlay.entries.reduce((all, pair) => all.concat(pair), [])

                    delegate: Label {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: index % 2 === 1
                        text: modelData
                        font.family: index % 2 === 0 ? "monospace" : Qt.application.font.family
                        color: index % 2 === 0 ? theme.accent : Material.foreground
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: overlay.footer !== ""
                text: overlay.footer
                opacity: 0.6
                wrapMode: Text.WordWrap
            }
        }
    }

}
