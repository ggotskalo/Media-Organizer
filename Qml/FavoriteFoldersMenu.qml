import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Dialogs 1.2 as Dialogs
import Qt.labs.platform 1.1
import Qt.labs.settings 1.0

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
}
