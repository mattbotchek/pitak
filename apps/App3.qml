import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "#0A1E2F"
    property var lora
    property var stack

    Timer {
        interval: 3000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: if (lora) lora.requestDevices(false)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Live PiSAT Telemetry / Devices"; color: "#A8C7E6"; font.pixelSize: 27; font.bold: true; Layout.fillWidth: true }
            Button { text: "Restart Paging"; onClicked: lora.requestDevices(true) }
            Button { text: "Back"; onClicked: stack.pop() }
        }
        Text { text: lora ? lora.pisatStatus : ""; color: "#A8C7E6"; textFormat: Text.PlainText }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#112B3C"
            radius: 10
            border.color: "#4A90E2"
            ListView {
                id: records
                anchors.fill: parent
                anchors.margins: 10
                model: lora ? lora.telemetryRecords : []
                clip: true
                spacing: 7
                delegate: Rectangle {
                    required property var modelData
                    width: records.width
                    height: recordText.implicitHeight + 16
                    color: "#1F3A52"
                    radius: 7
                    Text {
                        id: recordText
                        anchors.fill: parent
                        anchors.margins: 8
                        text: modelData.time + "  " + modelData.source + "\n" + modelData.payload
                        color: "#A8C7E6"
                        wrapMode: Text.WrapAnywhere
                        textFormat: Text.PlainText
                    }
                }
            }
        }
    }
}
