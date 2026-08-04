import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0A1E2F"
    property var lora
    property var stack

    function sendMessage() {
        if (lora && input.text.trim().length > 0) {
            lora.sendPeerMessage(input.text)
            input.clear()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Text { text: "Peer Discovery + Messaging"; color: "#A8C7E6"; font.pixelSize: 27; font.bold: true }
        Text {
            text: lora ? lora.securityStatus : "LoRa unavailable"
            color: text.indexOf("enabled") >= 0 ? "#7ED6A5" : "#FFB36B"
            textFormat: Text.PlainText
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Compatible peer:"; color: "#A8C7E6" }
            ComboBox {
                id: peerBox
                Layout.fillWidth: true
                model: lora ? lora.peers : []
                textRole: "name"
                valueRole: "id"
                delegate: ItemDelegate {
                    width: peerBox.width
                    enabled: modelData.compatible
                    text: (modelData.name || modelData.id) + "  [" + modelData.id + "]" +
                          (modelData.compatible ? "" : " (incompatible)")
                }
                onActivated: lora.setSelectedPeer(currentValue)
            }
            Text {
                text: lora && lora.selectedPeer ? "Online" : "Select a peer"
                color: lora && lora.selectedPeer ? "#7ED6A5" : "#A8C7E6"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#112B3C"
            radius: 10
            border.color: "#4A90E2"

            ListView {
                id: messageList
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8
                clip: true
                model: lora ? lora.messages : []
                onCountChanged: if (count) positionViewAtEnd()
                delegate: Rectangle {
                    required property var modelData
                    width: messageList.width
                    height: messageText.implicitHeight + 18
                    radius: 8
                    color: modelData.direction === "sent" ? "#1F3A52" : "#173D55"
                    border.color: "#4A90E2"
                    Text {
                        id: messageText
                        anchors.fill: parent
                        anchors.margins: 9
                        text: modelData.time + "  " + modelData.name + ": " + modelData.text
                        color: "#A8C7E6"
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: input
                Layout.fillWidth: true
                maximumLength: 160
                placeholderText: "Message (up to 160; radio-sized automatically)"
                enabled: lora && lora.selectedPeer
                onAccepted: root.sendMessage()
            }
            Button { text: "Send"; enabled: input.enabled && input.text.length > 0; onClicked: root.sendMessage() }
            Button { text: "Back"; onClicked: stack.pop() }
        }
    }
}
