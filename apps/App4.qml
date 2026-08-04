import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "#0A1E2F"
    property var lora
    property var stack

    Component.onCompleted: if (lora) lora.refreshSessions()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Saved PiSAT Sessions"; color: "#A8C7E6"; font.pixelSize: 27; font.bold: true; Layout.fillWidth: true }
            Button { text: "Refresh"; onClicked: lora.refreshSessions() }
            Button { text: "Back"; onClicked: stack.pop() }
        }
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            Rectangle {
                SplitView.preferredWidth: 300
                color: "#112B3C"
                border.color: "#4A90E2"
                ListView {
                    id: sessionList
                    anchors.fill: parent
                    anchors.margins: 8
                    model: lora ? lora.sessions : []
                    spacing: 6
                    clip: true
                    delegate: ItemDelegate {
                        required property var modelData
                        width: sessionList.width
                        text: "Session " + modelData.id + "\n" + modelData.startedAt + "\n" + modelData.count + " observations"
                        onClicked: lora.selectSession(modelData.id)
                    }
                }
            }
            Rectangle {
                SplitView.fillWidth: true
                color: "#112B3C"
                border.color: "#4A90E2"
                ListView {
                    id: history
                    anchors.fill: parent
                    anchors.margins: 8
                    model: lora ? lora.sessionObservations : []
                    spacing: 6
                    clip: true
                    delegate: Rectangle {
                        required property var modelData
                        width: history.width
                        height: observationText.implicitHeight + 16
                        radius: 7
                        color: "#1F3A52"
                        Text {
                            id: observationText
                            anchors.fill: parent
                            anchors.margins: 8
                            text: modelData.observedAt + "  " + modelData.source + "\n" + modelData.payload
                            color: "#A8C7E6"
                            wrapMode: Text.WrapAnywhere
                            textFormat: Text.PlainText
                        }
                    }
                }
            }
        }
    }
}
