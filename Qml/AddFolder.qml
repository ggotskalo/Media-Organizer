import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Dialogs 1.2
Dialog {
    visible: false
    title: "Add folder"
    contentItem: Item {
        property var folders:["1","2","3"]
        TextInput{
            id: folderInput
            width: parent.width
            text: "Folder Name"
        }

        ListView {
            anchors{
                top: folderInput.bottom
                bottom: parent.bottom
            }
            width: parent.width
            model: folders
            }

    }
    standardButtons: StandardButton.Save | StandardButton.Cancel
    /*footer: Rectangle {
                color: "blue"
                height: 50
            }
            */
}
