import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One tab in the bar: the page's icon and title, and a close button on the selected or hovered
// tab. Every tab uses the bar's color; the selected one has brighter text. Middle-click closes.
Item {
    id: tab

    property var view
    property bool selected: false
    property color textColor: "white"
    property bool showDivider: false

    signal activated()
    signal closeRequested()

    HoverHandler { id: hover }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: tab.activated()
    }

    TapHandler {
        acceptedButtons: Qt.MiddleButton
        onTapped: tab.closeRequested()
    }

    Rectangle {
        visible: tab.showDivider
        width: 1
        height: parent.height * 0.5
        anchors.verticalCenter: parent.verticalCenter
        color: tab.textColor
        opacity: 0.2
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 4
        spacing: 6

        Image {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            source: tab.view ? tab.view.icon : ""
            sourceSize: Qt.size(32, 32)
            visible: status === Image.Ready
            opacity: tab.selected ? 1 : 0.7
        }

        Label {
            Layout.fillWidth: true
            text: tab.view && tab.view.title ? tab.view.title : (tab.view && tab.view.loading ? qsTr("Loading…") : qsTr("New Tab"))
            color: tab.textColor
            opacity: tab.selected ? 1 : 0.55
            elide: Text.ElideRight
            font.pixelSize: 13
        }

        ToolButton {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            padding: 0
            visible: tab.selected || hover.hovered
            text: "×"
            font.pixelSize: 16
            Accessible.name: qsTr("Close Tab")
            contentItem: Label {
                text: parent.text
                font: parent.font
                color: tab.textColor
                opacity: tab.selected ? 0.9 : 0.6
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: tab.closeRequested()
        }
    }
}
