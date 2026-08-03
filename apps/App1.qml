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

    function sendCurrentMessage() {
        if (!lora || !input.text || input.text.trim().length === 0)
            return;

        lora.sendMessage(input.text.trim());
        input.text = "";
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

        Rectangle {
            id: chatPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1C4A6B"
            radius: 12
            border.color: "#6FA8DC"
            border.width: 2

            ListView {
                id: messageList
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                model: lora && lora.messages ? lora.messages : []
                spacing: 8

                onCountChanged: {
                    if (count > 0)
                        positionViewAtEnd()
                }

                delegate: Item {
                    width: messageList.width
                    height: Math.max(44, bubbleText.contentHeight + 18)

                    property bool isSent: modelData ? String(modelData).indexOf("TX:") === 0 : false
                    property string displayText: modelData
                        ? (String(modelData).indexOf("TX:") === 0
                           ? String(modelData).substring(4)
                           : (String(modelData).indexOf("RX:") === 0 ? String(modelData).substring(4) : String(modelData)))
                        : ""

                    Rectangle {
                        id: leftBubble
                        visible: !parent.isSent
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: 4
                        color: "#2C5D7F"
                        radius: 10
                        border.color: "#8DB8E6"
                        border.width: 1
                        width: Math.min(parent.width * 0.78, bubbleText.contentWidth + 24)
                        height: bubbleText.contentHeight + 12

                        Text {
                            id: bubbleText
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            text: parent.displayText
                            color: "#A8C7E6"
                            font.pixelSize: 18
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignLeft
                        }
                    }

                    Rectangle {
                        id: rightBubble
                        visible: parent.isSent
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 4
                        color: "#3C7FB5"
                        radius: 10
                        border.color: "#A9D0FF"
                        border.width: 1
                        width: Math.min(parent.width * 0.78, bubbleTextSent.contentWidth + 24)
                        height: bubbleTextSent.contentHeight + 12

                        Text {
                            id: bubbleTextSent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            text: parent.displayText
                            color: "white"
                            font.pixelSize: 18
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignRight
                        }
                    }
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
                onAccepted: root.sendCurrentMessage()
            }

            Button {
                text: "Send"
                onClicked: root.sendCurrentMessage()
            }
        }

        Button {
            text: "Back"
            Layout.alignment: Qt.AlignHCenter
            onClicked: stack.pop()
        }
    }
}
