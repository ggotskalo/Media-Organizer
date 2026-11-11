import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.11

Window {
    id: copyDialog

    property var manager: null

    title: manager && manager.moving ? qsTr("Moving") : qsTr("Copying")
    width: 460
    height: 170
    minimumWidth: 360
    minimumHeight: 150
    modality: Qt.ApplicationModal
    flags: Qt.Dialog
    visible: manager !== null && manager.running

    onClosing: {
        if (manager && manager.running)
            manager.cancel()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            text: (manager && manager.moving ? qsTr("Moving files...") : qsTr("Copying files...")) + (manager ? "  " + manager.summary : "")
            font.bold: true
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 1
            value: manager ? manager.progress : 0
        }

        Label {
            Layout.fillWidth: true
            text: manager ? manager.currentFile : ""
            elide: Text.ElideMiddle
            color: "#666"
        }

        Item { Layout.fillHeight: true }

        Button {
            text: qsTr("Cancel")
            Layout.alignment: Qt.AlignRight
            onClicked: if (manager) manager.cancel()
        }
    }
}
