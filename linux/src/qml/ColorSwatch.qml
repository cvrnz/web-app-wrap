import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs

// A color well. Clicking it opens a color picker; `picked` reports the choice.
AbstractButton {
    id: swatch

    property color color: "black"
    property string dialogTitle: qsTr("Choose a Color")

    signal picked(color color)

    implicitWidth: 44
    implicitHeight: 28
    focusPolicy: Qt.StrongFocus
    Accessible.name: dialogTitle

    background: Rectangle {
        radius: Math.min(4, theme.radius)
        color: swatch.color
        border.width: swatch.visualFocus ? 2 : 1
        border.color: swatch.visualFocus ? theme.accent : Qt.rgba(1, 1, 1, 0.25)
    }

    onClicked: {
        picker.selectedColor = swatch.color
        picker.open()
    }

    ColorDialog {
        id: picker
        title: swatch.dialogTitle
        onAccepted: swatch.picked(selectedColor)
    }
}
