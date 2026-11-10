import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.11
import QtQuick.Dialogs 1.2 as Dialogs
import Qt.labs.platform 1.1 as Platform

Menu {
    id: foldersMenu

    property int index
    property string name

    MenuItem {
        text: qsTr("Rename...")
        onTriggered: {
            inputDialog.text = foldersMenu.name
            inputDialog.index = foldersMenu.index
            inputDialog.open()
        }
    }

    MenuItem {
        text: qsTr("Unite with...")
        onTriggered: {
            uniteFolderDialog.index = foldersMenu.index
            uniteFolderDialog.open()
        }
    }

    MenuItem {
        text: qsTr("Manage united folders...")
        onTriggered: {
            manageDialog.index = foldersMenu.index
            manageDialog.reloadPaths()
            manageDialog.visible = true
        }
    }

    MenuItem {
        text: qsTr("Remove from favorites")
        onTriggered: foldersModel.removeFolder(foldersMenu.index)
    }

    Platform.FolderDialog {
        id: uniteFolderDialog
        property int index
        onAccepted: foldersModel.uniteWithFolder(index, folder)
    }

    Platform.FolderDialog {
        id: manageAddFolderDialog
        property int targetIndex: -1
        onAccepted: {
            foldersModel.addPathToFolder(targetIndex, folder)
            manageDialog.reloadPaths()
        }
    }

    Window {
        id: manageDialog
        title: qsTr("Manage united folders")
        modality: Qt.ApplicationModal
        flags: Qt.Dialog | Qt.WindowCloseButtonHint | Qt.WindowTitleHint
        visible: false
        width: 560
        height: 360
        property int index: -1
        property var folderPaths: []
        property string selectedPath: ""

        function reloadPaths() {
            folderPaths = foldersModel.getFolderPaths(index)
            selectedPath = ""
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            Label {
                text: qsTr("Folders in this group:")
                font.bold: true
            }

            ListView {
                id: pathsList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: manageDialog.folderPaths

                delegate: ItemDelegate {
                    width: pathsList.width
                    text: modelData
                    highlighted: manageDialog.selectedPath === modelData
                    onClicked: manageDialog.selectedPath = modelData
                }
            }

            Label {
                Layout.fillWidth: true
                visible: manageDialog.folderPaths.length <= 1
                color: "#666"
                text: qsTr("At least one folder must remain in the group.")
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: qsTr("Add folder...")
                    onClicked: {
                        manageAddFolderDialog.targetIndex = manageDialog.index
                        manageAddFolderDialog.open()
                    }
                }

                Button {
                    text: qsTr("Remove selected")
                    enabled: manageDialog.selectedPath.length > 0 && manageDialog.folderPaths.length > 1
                    onClicked: {
                        if (foldersModel.removePathFromFolder(manageDialog.index, manageDialog.selectedPath)) {
                            manageDialog.reloadPaths()
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Close")
                    onClicked: manageDialog.close()
                }
            }
        }
    }

    Dialogs.Dialog {
        id: inputDialog
        property alias text: input.text
        property int index
        modality: Qt.WindowModal
        standardButtons: Dialog.Apply | Dialog.Cancel
        TextInput {
            id:input
            anchors.fill:parent
        }
        onApply: {
            accept()
        }
        onAccepted: {
            foldersModel.renameFolder(index, text)
            close()
        }
        onVisibleChanged: {
            if (visible) input.forceActiveFocus()
        }
    }
}
