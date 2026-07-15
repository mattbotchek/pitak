import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    width: parent ? parent.width : 800
    height: parent ? parent.height : 480
    color: "transparent"

    property var stack   // always present

    Text {
        text: "App 6 Placeholder"
        anchors.centerIn: parent
        color: "#A8C7E6"
        font.pixelSize: 32
    }

    Button {
        text: "Back"
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 30
        onClicked: nav.pop()
    }
}