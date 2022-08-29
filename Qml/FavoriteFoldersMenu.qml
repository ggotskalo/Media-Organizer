import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
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
        text: qsTr("Remove from favorites")
        onTriggered: foldersModel.removeFolder(foldersMenu.index)
    }

    Platform.FolderDialog {
        id: uniteFolderDialog
        property int index
        onAccepted: foldersModel.uniteWithFolder(index, folder)
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
