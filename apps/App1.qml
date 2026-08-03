import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.VirtualKeyboard 2.15

Rectangle {
    id: root
    width: parent ? parent.width : 800
    height: parent ? parent.height : 480
    color: "#0A1E2F"
    opacity: 0.85

    property var lora
    property var stack

    Item {
        id: contentHost
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height
        y: keyboard.active ? -Math.min(260, keyboard.height - 40) : 0

        Behavior on y {
            NumberAnimation {
                duration: 220
                easing.type: Easing.OutQuad
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 20

            Text {
                text: "LoRa Messaging"
                font.pixelSize: 28
                color: "#A8C7E6"
                Layout.alignment: Qt.AlignHCenter
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: lora.messages

                delegate: Rectangle {
                    width: parent.width
                    height: 40
                    color: index % 2 ? "#1F3A52" : "#112B3C"

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: "#A8C7E6"
                        font.pixelSize: 18
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                TextField {
                    id: input
                    Layout.fillWidth: true
                    placeholderText: "Enter message..."
                    color: "white"
                    onActiveFocusChanged: {
                        keyboard.active = activeFocus
                    }
                    onPressed: {
                        keyboard.active = true
                    }
                }

                Button {
                    text: "Send"
                    onClicked: {
                        if (input.text.length > 0) {
                            lora.sendMessage(input.text)
                            input.text = ""
                        }
                        keyboard.active = false
                        input.focus = false
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 12

                Button {
                    text: "Back"
                    onClicked: nav.pop()
                }

                Button {
                    text: keyboard.active ? "Hide keyboard" : "Show keyboard"
                    onClicked: {
                        keyboard.active = !keyboard.active
                        if (keyboard.active) {
                            input.forceActiveFocus()
                        } else {
                            input.focus = false
                        }
                    }
                }
            }
        }
    }

    InputPanel {
        id: keyboard
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: active
        active: false
        z: 100
        onActiveChanged: {
            if (!active) {
                input.focus = false
            }
        }
    }
}
