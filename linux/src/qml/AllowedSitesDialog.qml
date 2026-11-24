import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Other sites the app may open, each with its subdomains. Clicked links to them stay in the
// app instead of going to the browser. Changes apply to open tabs straight away.
Dialog {
    id: dialog

    property string error: ""

    function add() {
        error = app.addAllowedDomain(field.text)
        if (error === "")
            field.clear()
    }

    title: qsTr("Allowed Sites")
    modal: true
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }
    background: Rectangle {
        color: Material.dialogColor
        radius: Math.min(12, theme.radius)
    }
    width: 440
    standardButtons: Dialog.Close
    onOpened: {
        error = ""
        field.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: qsTr("Links to %1 always stay in the app. Add other sites that should too, such as the one you sign in with.")
                  .arg(app.url.toString().replace(/^https?:\/\//, "").replace(/\/.*$/, ""))
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: field
                Layout.fillWidth: true
                placeholderText: qsTr("Site, like github.com")
                selectByMouse: true
                onAccepted: dialog.add()
            }

            Button {
                text: qsTr("Add")
                enabled: field.text.trim() !== ""
                onClicked: dialog.add()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: dialog.error !== ""
            text: dialog.error
            color: Material.color(Material.Red)
            wrapMode: Text.WordWrap
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(240, Math.max(contentHeight, 40))
            clip: true
            model: app.allowedDomains

            delegate: RowLayout {
                required property string modelData
                width: ListView.view.width

                Label {
                    Layout.fillWidth: true
                    text: modelData
                    elide: Text.ElideRight
                }

                ToolButton {
                    text: "×"
                    Accessible.name: qsTr("Remove %1").arg(modelData)
                    onClicked: app.removeAllowedDomain(modelData)
                }
            }

            Label {
                anchors.centerIn: parent
                visible: parent.count === 0
                text: qsTr("No other sites yet.")
                opacity: 0.5
            }
        }
    }
}
