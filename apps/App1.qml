import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: parent ? parent.width : 800
    height: parent ? parent.height : 480
    color: "#0A1E2F"
    opacity: 0.85

    property var lora
    property var stack

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
            }

            Button {
                text: "Send"
                onClicked: {
                    if (input.text.length > 0) {
                        lora.sendMessage(input.text)
                        input.text = ""
                    }
                }
            }
        }

        Button {
            text: "Back"
            Layout.alignment: Qt.AlignHCenter
            onClicked: nav.pop()
        }
    }
}
