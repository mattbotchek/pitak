import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "#0A1E2F"
    property var lora
    property var stack

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 16

        Text { text: "PiSAT Control Panel"; color: "#A8C7E6"; font.pixelSize: 28; font.bold: true }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 92
            radius: 10
            color: "#112B3C"
            border.color: "#4A90E2"
            Column {
                anchors.fill: parent; anchors.margins: 12; spacing: 7
                Text { text: lora ? lora.securityStatus : "Unavailable"; color: "#A8C7E6"; textFormat: Text.PlainText }
                Text { text: lora ? lora.pisatStatus : "Unavailable"; color: "#7ED6A5"; textFormat: Text.PlainText; wrapMode: Text.Wrap }
            }
        }
        Button { text: "Refresh Authenticated Status"; onClicked: lora.requestStatus() }

        GroupBox {
            title: "Monitor"
            Layout.fillWidth: true
            Layout.preferredHeight: 88
            label: Text { text: "Monitor"; color: "#A8C7E6"; font.bold: true }
            background: Rectangle { color: "#112B3C"; radius: 10; border.color: "#4A90E2" }
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                TextField { id: channel; Layout.fillWidth: true; placeholderText: "Channel"; text: "1" }
                Button { text: "Monitor ON"; onClicked: lora.setMonitor(true, channel.text) }
                Button { text: "Monitor OFF"; onClicked: lora.setMonitor(false, channel.text) }
            }
        }

        GroupBox {
            title: "Capture"
            Layout.fillWidth: true
            Layout.preferredHeight: 88
            label: Text { text: "Capture"; color: "#A8C7E6"; font.bold: true }
            background: Rectangle { color: "#112B3C"; radius: 10; border.color: "#4A90E2" }
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                ComboBox { id: captureTool; model: ["tcpdump", "airodump", "wireshark"]; Layout.fillWidth: true }
                Button { text: "START"; onClicked: lora.startCapture(captureTool.currentText) }
                Button { text: "STOP"; onClicked: lora.stopCapture() }
            }
        }
        Item { Layout.fillHeight: true }
        Button { text: "Back"; Layout.alignment: Qt.AlignHCenter; onClicked: stack.pop() }
    }
}
