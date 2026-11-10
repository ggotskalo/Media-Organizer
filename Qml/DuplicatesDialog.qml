import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.11

Window {
    id: root
    width: 600
    height: 400
    title: qsTr("Find Duplicates")
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.WindowCloseButtonHint | Qt.WindowTitleHint

    property var currentPaths: []
    property string currentFileName: ""
    property string currentFileSize: ""
    property int selectedIndex: 0

    function start(folders) {
        selectedIndex = 0
        currentPaths = []
        root.visible = true
        duplicateFinder.startScan(folders)
    }

    onClosing: {
        if (duplicateFinder.running)
            duplicateFinder.cancel()
    }

    Connections {
        target: duplicateFinder

        function onDuplicateGroupFound(fileName, fileSize, paths) {
            currentFileName = fileName
            currentFileSize = fileSize
            currentPaths = paths
            selectedIndex = 0
            pathsList.model = paths
        }

        function onFinished(replacedCount) {
            resultLabel.text = replacedCount > 0
                ? qsTr("Done. Replaced %1 files with shortcuts.").arg(replacedCount)
                : qsTr("Done. No files were replaced.")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        // --- Scanning phase ---
        ColumnLayout {
            Layout.fillWidth: true
            visible: duplicateFinder.state === "scanning"
            spacing: 8

            Label {
                text: qsTr("Scanning for duplicate videos...")
                font.bold: true
                font.pixelSize: 14
            }
            ProgressBar {
                Layout.fillWidth: true
                value: duplicateFinder.progress
            }
            Label {
                text: duplicateFinder.currentFolder
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                color: "#666"
            }
        }

        // --- Choosing phase ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: duplicateFinder.state === "choosing"
            spacing: 8

            Label {
                text: qsTr("Group %1 of %2").arg(duplicateFinder.processedGroups + 1).arg(duplicateFinder.totalGroups)
                color: "#666"
            }

            Label {
                text: currentFileName + "  (" + currentFileSize + ")"
                font.bold: true
                font.pixelSize: 13
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Select the file to keep. Others will be replaced with shortcuts:")
            }

            ListView {
                id: pathsList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2

                delegate: Rectangle {
                    width: pathsList.width
                    height: 30
                    color: index === selectedIndex ? "#e0e8ff" : (mouseArea.containsMouse ? "#f0f0f0" : "transparent")
                    radius: 3

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 5
                        spacing: 8

                        RadioButton {
                            checked: index === selectedIndex
                            onClicked: selectedIndex = index
                        }

                        Label {
                            text: modelData
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: selectedIndex = index
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    text: qsTr("Keep selected")
                    enabled: currentPaths.length > 0
                    onClicked: {
                        if (selectedIndex >= 0 && selectedIndex < currentPaths.length) {
                            duplicateFinder.keepFile(currentPaths[selectedIndex])
                        }
                    }
                }
                Button {
                    text: qsTr("Skip")
                    onClicked: duplicateFinder.skipGroup()
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel")
                    onClicked: {
                        duplicateFinder.cancel()
                        root.close()
                    }
                }
            }
        }

        // --- Finished phase ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: duplicateFinder.state === "finished" || duplicateFinder.state === "cancelled"
            spacing: 15

            Item { Layout.fillHeight: true }

            Label {
                id: resultLabel
                Layout.alignment: Qt.AlignHCenter
                font.pixelSize: 14
                text: duplicateFinder.state === "cancelled" ? qsTr("Cancelled.") : ""
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                visible: duplicateFinder.replacedCount > 0
                text: qsTr("Processed %1 groups, replaced %2 files")
                    .arg(duplicateFinder.processedGroups)
                    .arg(duplicateFinder.replacedCount)
                color: "#666"
            }

            Item { Layout.fillHeight: true }

            Button {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
