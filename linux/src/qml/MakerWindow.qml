import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// WebAppWrap's window: My Apps, the list of apps made here, and building them.
ApplicationWindow {
    id: win

    width: 820
    height: 600
    minimumWidth: 560
    minimumHeight: 400
    visible: true
    title: "WebAppWrap"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property bool dialogOpen: editor.opened || help.opened || rowMenu.opened

    function addApp() {
        apps.newDraft()
        editor.open()
    }

    function editApp(row) {
        if (row < 0)
            return
        apps.editDraft(row)
        editor.open()
    }

    Shortcut { sequences: ["Ctrl+N", "+"]; enabled: !win.dialogOpen; onActivated: win.addApp() }
    Shortcut { sequences: ["Return", "Enter", "E"]; enabled: !win.dialogOpen; onActivated: win.editApp(list.currentIndex) }
    Shortcut { sequences: ["Delete", "-"]; enabled: !win.dialogOpen && !apps.building; onActivated: apps.removeFromList(list.currentIndex) }
    Shortcut { sequences: ["Space", "Ctrl+B"]; enabled: !win.dialogOpen && !apps.building; onActivated: apps.buildPending() }
    Shortcut { sequences: ["O"]; enabled: !win.dialogOpen; onActivated: apps.openApp(list.currentIndex) }
    Shortcut { sequences: ["J", "Down"]; enabled: !win.dialogOpen; onActivated: list.incrementCurrentIndex() }
    Shortcut { sequences: ["K", "Up"]; enabled: !win.dialogOpen; onActivated: list.decrementCurrentIndex() }
    Shortcut { sequences: ["?", "F1"]; enabled: !editor.opened; onActivated: help.opened ? help.close() : help.open() }
    Shortcut { sequences: ["Q", "Ctrl+Q"]; enabled: !win.dialogOpen; onActivated: Qt.quit() }

    // Right-to-left languages mirror the window; the Translator sets the layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: qsTr("My Apps")
                    font.pixelSize: 22
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: qsTr("WebAppWrap %1").arg(apps.version)
                    opacity: 0.45
                    font.pixelSize: 12
                }

                ToolButton {
                    text: "?"
                    font.pixelSize: 16
                    Accessible.name: qsTr("Help")
                    onClicked: help.open()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#161618"
                radius: Math.min(10, theme.radius)

                ListView {
                    id: list

                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    model: apps
                    currentIndex: 0
                    keyNavigationEnabled: false
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        id: row

                        required property int index
                        required property string name
                        required property string url
                        required property string icon
                        required property string status
                        required property bool needsBuild
                        required property bool installed
                        required property bool building
                        required property bool failed

                        width: ListView.view.width
                        height: 64
                        radius: Math.min(8, theme.radius)
                        color: ListView.isCurrentItem ? Qt.rgba(1, 1, 1, 0.07) : "transparent"

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onPressed: list.currentIndex = row.index
                            onDoubleClicked: win.editApp(row.index)
                            onClicked: (mouse) => {
                                if (mouse.button === Qt.RightButton) {
                                    rowMenu.row = row.index
                                    rowMenu.installed = row.installed
                                    rowMenu.popup()
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 14

                            Image {
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                source: row.icon
                                sourceSize: Qt.size(80, 80)
                                cache: false
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: row.name
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: row.url
                                    opacity: 0.5
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }

                            BusyIndicator {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                visible: row.building
                                running: visible
                            }

                            Label {
                                Layout.maximumWidth: list.width * 0.4
                                text: row.status
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignRight
                                color: row.failed ? Material.color(Material.Red)
                                     : row.needsBuild ? theme.accent : Material.foreground
                                opacity: row.failed || row.needsBuild ? 1 : 0.45
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("No apps yet.\nPress + to turn a web app into a desktop app.")
                        opacity: 0.5
                        lineHeight: 1.3
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: qsTr("Add App")
                    onClicked: win.addApp()
                }

                Button {
                    text: qsTr("Edit")
                    flat: true
                    enabled: list.count > 0
                    onClicked: win.editApp(list.currentIndex)
                }

                Label {
                    Layout.fillWidth: true
                    text: apps.message
                    opacity: 0.7
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignRight
                }

                Button {
                    highlighted: true
                    enabled: apps.pendingCount > 0 && !apps.building
                    text: apps.building ? qsTr("Building…")
                        : apps.pendingCount === 0 ? qsTr("All Built")
                        : qsTr("Build %n App(s)", "", apps.pendingCount)
                    onClicked: apps.buildPending()
                }
            }
        }
    }

    Menu {
        id: rowMenu

        property int row: -1
        property bool installed: false

        MenuItem { text: qsTr("Edit…"); onTriggered: win.editApp(rowMenu.row) }
        MenuItem { text: qsTr("Build"); enabled: !apps.building; onTriggered: apps.build(rowMenu.row) }
        MenuItem { text: qsTr("Open"); enabled: rowMenu.installed; onTriggered: apps.openApp(rowMenu.row) }
        MenuSeparator {}
        MenuItem { text: qsTr("Uninstall"); enabled: rowMenu.installed && !apps.building; onTriggered: apps.uninstall(rowMenu.row) }
        MenuItem { text: qsTr("Remove from List"); enabled: !apps.building; onTriggered: apps.removeFromList(rowMenu.row) }
    }

    AppEditor {
        id: editor
        anchors.centerIn: parent
    }

    ShortcutsOverlay {
        id: help
        anchors.centerIn: parent
        entries: [
            ["+  or  Ctrl+N", qsTr("Add an app")],
            ["Enter  or  E", qsTr("Edit the selected app (double-click works too)")],
            ["Delete  or  -", qsTr("Take the app off the list (it stays installed)")],
            ["Space  or  Ctrl+B", qsTr("Build every app that needs it")],
            ["O", qsTr("Open the selected app")],
            ["J / K  or  ↓ / ↑", qsTr("Move through the list")],
            ["Right-click", qsTr("Build, open or uninstall one app")],
            ["?", qsTr("This list")],
            ["Q", qsTr("Quit")],
        ]
        footer: qsTr("An app is rebuilt when its settings change or WebAppWrap is updated. Sign-ins are kept across rebuilds. The list is kept in %1.\n\nCommand line (also as “waw”):\n%2").arg(apps.dataFolder).arg(helpText)
    }
}
