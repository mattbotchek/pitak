import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    visibility: "FullScreen"
    title: "PiTAK"

    property alias nav: stack

    Rectangle {
        anchors.fill: parent
        color: "#0A1E2F"
        opacity: 0.85
    }

    Rectangle {
        id: logoBar
        width: parent.width
        height: 100
        color: "#112B3C"

        Row {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 20

            Rectangle {
                width: 70
                height: 70
                radius: 8
                color: "#1F3A52"
                border.color: "#4A90E2"
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: "⚡"
                    font.pixelSize: 40
                    color: "#4A90E2"
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Text {
                    text: "PiTAK TSA System"
                    font.pixelSize: 26
                    color: "#A8C7E6"
                    font.bold: true
                }

                Text {
                    text: "Pi Team Awareness Kit Tactical Signal Analysis System"
                    font.pixelSize: 14
                    color: "#A8C7E6"
                }
            }
        }
    }

    StackView {
        id: stack
        anchors.top: logoBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        initialItem: mainMenu
    }

    Component {
        id: mainMenu

        Rectangle {
            id: mainMenuRoot
            width: parent ? parent.width : 800
            height: parent ? parent.height : 480
            color: "transparent"

            GridLayout {
                anchors.centerIn: parent
                columns: 3
                rowSpacing: 20
                columnSpacing: 20

                Repeater {
                    model: 6
                    delegate: Rectangle {
                        width: 150
                        height: 150
                        radius: 10
                        color: "#1F3A52"
                        border.color: "#4A90E2"
                        border.width: 3

                        Text {
                            anchors.centerIn: parent
                            text: "App " + (index + 1)
                            color: "#A8C7E6"
                            font.pixelSize: 22
                        }

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                if (index === 0) {
                                    stack.push(Qt.resolvedUrl("qrc:/apps/App1.qml"), { "lora": lora, "stack": stack })
                                } else if (index === 1) {
                                    stack.push(Qt.resolvedUrl("qrc:/apps/App2.qml"), { "stack": stack })
                                } else if (index === 2) {
                                    stack.push(Qt.resolvedUrl("qrc:/apps/App3.qml"), { "stack": stack })
                                } else if (index === 3) {
                                    stack.push(Qt.resolvedUrl("qrc:/apps/App4.qml"), { "stack": stack })
                                } else if (index === 4) {
                                    stack.push(Qt.resolvedUrl("qrc:/apps/App5.qml"), { "stack": stack })
                                } else if (index === 5) {
                                    stack.push(Qt.resolvedUrl("qrc:/apps/App6.qml"), { "stack": stack })
                                }
                            }
                        }
                    }
                }
            }

            Button {
                text: "Exit PiTAK"
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 30
                onClicked: Qt.quit()
            }
        }
    }
}