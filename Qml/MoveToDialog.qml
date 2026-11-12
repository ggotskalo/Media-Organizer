import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.11

Window {
    id: moveDialog

    property var helper: null

    title: qsTr("Move to...")
    width: 560
    height: 360
    minimumWidth: 420
    minimumHeight: 260
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.WindowCloseButtonHint | Qt.WindowTitleHint

    onClosing: if (helper) helper.cancel()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: helper ? helper.sourcePath : ""
            font.bold: true
            elide: Text.ElideMiddle
        }
        Label {
            text: qsTr("Size: %1").arg(helper ? helper.sizeText : "")
        }

        Label {
            visible: helper !== null && helper.targets.length === 0
            text: qsTr("No other folders in this united folder")
            color: "#666"
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: helper ? helper.targets : []

            delegate: Rectangle {
                width: ListView.view.width
                height: 64
                radius: 4
                color: modelData.enabled ? (mouse.containsMouse ? "#e3f0ff" : "#f4f8fd") : "#f0f0f0"
                border.color: modelData.enabled ? "#9bbbe0" : "#d0d0d0"
                opacity: modelData.enabled ? 1.0 : 0.6

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2
                    Label {
                        width: parent.width
                        text: modelData.drive + (modelData.label !== "" ? "  " + modelData.label : "")
                              + "   —   " + modelData.freeText
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Label {
                        width: parent.width
                        text: modelData.destPath
                        elide: Text.ElideMiddle
                        color: "#444"
                    }
                    Label {
                        width: parent.width
                        visible: modelData.note !== ""
                        text: modelData.note
                        color: modelData.enabled ? "#a06000" : "#b00000"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    enabled: modelData.enabled
                    hoverEnabled: true
                    onClicked: {
                        if (helper.move(index))
                            moveDialog.close()
                    }
                }
            }
        }

        Button {
            text: qsTr("Cancel")
            Layout.alignment: Qt.AlignRight
            onClicked: moveDialog.close()
        }
    }
}
