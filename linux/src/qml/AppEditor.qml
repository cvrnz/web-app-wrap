import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Adds an app to the list or edits one: its name, web address, icon and title bar color.
// Saving doesn't build it.
Dialog {
    id: editor

    readonly property var draft: apps.draft
    property string error: ""

    // Black or white, whichever reads on `c`, as the app's bar picks it.
    function textOn(c) {
        return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b < 0.5 ? "white" : "black"
    }

    function save() {
        error = apps.saveDraft()
        if (error === "")
            close()
    }

    title: draft.editingId === "" ? qsTr("Add App") : qsTr("Edit App")
    modal: true
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }
    background: Rectangle {
        color: Material.dialogColor
        radius: Math.min(12, theme.radius)
    }
    focus: true
    width: Math.min(640, parent ? parent.width - 32 : 640)
    onOpened: {
        error = ""
        nameField.forceActiveFocus()
    }

    // Saving can fail (a name already on the list), so it's handled here rather than by
    // accepting, which would always close the dialog.
    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: qsTr("Save")
            enabled: editor.draft.valid
            highlighted: true
            onClicked: editor.save()
        }
        onRejected: editor.close()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 24

        // The icon, and what it's drawn from.
        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 180
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 160
                Layout.preferredHeight: 160
                Layout.alignment: Qt.AlignHCenter
                color: drop.containsDrag ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                border.color: drop.containsDrag ? theme.accent : "transparent"
                border.width: 2
                radius: Math.min(12, theme.radius)

                Image {
                    anchors.centerIn: parent
                    width: 140
                    height: 140
                    sourceSize: Qt.size(280, 280)
                    source: "image://icons/draft/" + editor.draft.revision
                    cache: false
                }

                DropArea {
                    id: drop
                    anchors.fill: parent
                    onDropped: (event) => {
                        if (event.hasUrls)
                            editor.draft.setImage(event.urls[0])
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: editor.draft.hasImage ? editor.draft.imageName : qsTr("Drop an SVG here")
                elide: Text.ElideMiddle
                opacity: 0.6
                font.pixelSize: 12
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Button {
                    text: qsTr("Choose SVG…")
                    onClicked: editor.draft.chooseImage()
                }
                Button {
                    visible: editor.draft.hasImage
                    text: qsTr("Remove")
                    flat: true
                    onClicked: editor.draft.clearImage()
                }
            }

            Label {
                Layout.fillWidth: true
                visible: editor.draft.imageError !== ""
                text: editor.draft.imageError
                color: Material.color(Material.Red)
                wrapMode: Text.WordWrap
                font.pixelSize: 12
            }

            GridLayout {
                Layout.fillWidth: true
                visible: editor.draft.imageIsSvg
                columns: 2
                columnSpacing: 10

                Label { text: qsTr("Background") }
                ColorSwatch {
                    color: editor.draft.iconBackground
                    dialogTitle: qsTr("Icon Background")
                    onPicked: (picked) => editor.draft.iconBackground = picked
                }

                ComboBox {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    model: [qsTr("Original Colors"), qsTr("One Color")]
                    currentIndex: editor.draft.usesSvgColor ? 1 : 0
                    onActivated: (index) => editor.draft.usesSvgColor = index === 1
                }

                Label { visible: editor.draft.usesSvgColor; text: qsTr("SVG color") }
                ColorSwatch {
                    visible: editor.draft.usesSvgColor
                    color: editor.draft.svgColor
                    dialogTitle: qsTr("SVG Color")
                    onPicked: (picked) => editor.draft.svgColor = picked
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: editor.draft.hasImage
                spacing: 0

                Label { text: qsTr("Zoom"); opacity: 0.7; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: editor.draft.zoomMinimum
                    to: 3
                    value: editor.draft.zoom
                    onMoved: editor.draft.zoom = value
                }
            }
        }

        // Name, address and title bar color.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 12

            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: qsTr("Name")
                text: editor.draft.name
                selectByMouse: true
                onTextEdited: editor.draft.name = text
                onAccepted: if (editor.draft.valid) editor.save()
            }

            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Web address")
                text: editor.draft.url
                selectByMouse: true
                inputMethodHints: Qt.ImhUrlCharactersOnly
                onTextEdited: editor.draft.url = text
                onAccepted: if (editor.draft.valid) editor.save()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("You can leave out https://. Addresses starting with localhost use http://.")
                wrapMode: Text.WordWrap
                opacity: 0.55
                font.pixelSize: 12
            }

            Label { text: qsTr("Title bar color"); Layout.topMargin: 8 }

            // A preview of the app's bar, with the text color the app will use on it.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                color: editor.draft.titleBarColor
                radius: Math.min(6, theme.radius)
                border.color: Qt.rgba(1, 1, 1, 0.15)

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    text: editor.draft.name.split("/").pop().trim() || qsTr("App")
                    color: editor.textOn(editor.draft.titleBarColor)
                    font.pixelSize: 13
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    text: "⋮"
                    color: editor.textOn(editor.draft.titleBarColor)
                }
            }

            RowLayout {
                Layout.fillWidth: true

                ColorSwatch {
                    color: editor.draft.titleBarColor
                    dialogTitle: qsTr("Title Bar Color")
                    onPicked: (picked) => editor.draft.setTitleBarColor(picked)
                }

                Label {
                    Layout.fillWidth: true
                    opacity: 0.6
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    text: {
                        switch (editor.draft.titleBarSource) {
                        case "lookingUp": return qsTr("Looking up the site’s color…")
                        case "site": return qsTr("The site’s theme color")
                        case "custom": return qsTr("Your color")
                        default: return editor.draft.url.trim() === "" ? qsTr("Filled in from the site once you enter its address")
                                                                       : qsTr("The site has no theme color, so this is the default")
                        }
                    }
                }

                Button {
                    visible: editor.draft.titleBarSource === "custom"
                    text: qsTr("Use Site Color")
                    flat: true
                    onClicked: editor.draft.useSiteColor()
                }
            }

            Label {
                Layout.fillWidth: true
                visible: editor.error !== ""
                text: editor.error
                color: Material.color(Material.Red)
                wrapMode: Text.WordWrap
            }
        }
    }
}
