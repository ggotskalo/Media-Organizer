import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.11

Window {
    id: repairDialog

    title: qsTr("Repair Broken Shortcuts")
    width: 520
    height: 260
    minimumWidth: 400
    minimumHeight: 220
    modality: Qt.ApplicationModal
    flags: Qt.Dialog

    property var shortcutPaths: []
    property var targetNames: []
    property var searchFolders: []

    function start(paths, names, folders) {
        shortcutPaths = paths
        targetNames = names
        searchFolders = folders
        matchPanel.visible = false
        finishedPanel.visible = false
        visible = true
        shortcutFixer.startRepair(paths, names, folders)
    }

    onClosing: {
        if (shortcutFixer.running)
            shortcutFixer.cancel()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // --- Progress panel ---
        ColumnLayout {
            id: progressPanel
            Layout.fillWidth: true
            visible: shortcutFixer.running && !matchPanel.visible
            spacing: 6

            Label {
                text: qsTr("Scanning folders for matching files...")
                font.bold: true
            }

            ProgressBar {
                Layout.fillWidth: true
                value: shortcutFixer.progress
            }

            Label {
                Layout.fillWidth: true
                text: shortcutFixer.currentFolder
                elide: Text.ElideMiddle
                color: "#666"
            }
        }

        // --- Match panel ---
        ColumnLayout {
            id: matchPanel
            Layout.fillWidth: true
            visible: false
            spacing: 8

            property string shortcutPath
            property string targetName
            property string candidatePath

            Label {
                text: qsTr("Match found — fix shortcut?")
                font.bold: true
                font.pixelSize: 14
            }

            GridLayout {
                columns: 2
                Layout.fillWidth: true
                columnSpacing: 8
                rowSpacing: 4

                Label { text: qsTr("Shortcut:"); font.bold: true }
                Label {
                    text: matchPanel.shortcutPath
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Label { text: qsTr("Looking for:"); font.bold: true }
                Label {
                    text: matchPanel.targetName
                    Layout.fillWidth: true
                }

                Label { text: qsTr("Found at:"); font.bold: true }
                Label {
                    text: matchPanel.candidatePath
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }

            RowLayout {
                spacing: 10
                Layout.topMargin: 4

                Button {
                    text: qsTr("Fix")
                    highlighted: true
                    onClicked: {
                        shortcutFixer.acceptMatch(matchPanel.shortcutPath, matchPanel.candidatePath)
                        matchPanel.visible = false
                    }
                }
                Button {
                    text: qsTr("Fix all")
                    onClicked: {
                        shortcutFixer.fixAll(matchPanel.shortcutPath, matchPanel.candidatePath)
                        matchPanel.visible = false
                    }
                }
                Button {
                    text: qsTr("Skip")
                    onClicked: {
                        shortcutFixer.skipMatch()
                        matchPanel.visible = false
                    }
                }
            }
        }

        // --- Finished panel ---
        ColumnLayout {
            id: finishedPanel
            Layout.fillWidth: true
            visible: false
            spacing: 10

            property int repairedCount: 0

            Label {
                text: qsTr("Done! Repaired shortcuts: %1").arg(finishedPanel.repairedCount)
                font.bold: true
                font.pixelSize: 14
            }

            Button {
                text: qsTr("Close")
                onClicked: repairDialog.close()
            }
        }

        Item { Layout.fillHeight: true }

        // --- Cancel button ---
        Button {
            text: qsTr("Cancel")
            visible: shortcutFixer.running
            Layout.alignment: Qt.AlignRight
            onClicked: {
                shortcutFixer.cancel()
                repairDialog.close()
            }
        }
    }

    Connections {
        target: shortcutFixer
        function onMatchFound(shortcutPath, targetName, candidatePath) {
            matchPanel.shortcutPath = shortcutPath
            matchPanel.targetName = targetName
            matchPanel.candidatePath = candidatePath
            matchPanel.visible = true
        }
        function onFinished(repairedCount) {
            finishedPanel.repairedCount = repairedCount
            finishedPanel.visible = true
            matchPanel.visible = false
        }
    }
}
