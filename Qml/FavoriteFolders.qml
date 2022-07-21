import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Dialogs 1.2 as Dialogs
import Qt.labs.platform 1.1
import Qt.labs.settings 1.0

ListView {
    signal itemClicked(index: int)
    delegate: Rectangle {
        width: parent.width
        height:50
        border.color: "black"
        border.width: 1
        Text {
            width: parent.width
            height: parent.height
            text: model.name
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            elide: Text.ElideRight
        }
        MouseArea {
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            anchors.fill: parent
            onClicked: {
                if (mouse.button == Qt.LeftButton)
                    itemClicked(index)
                if (mouse.button == Qt.RightButton) {
                    foldersMenu.index = index
                    foldersMenu.name = model.name
                    foldersMenu.open()
                }
            }
        }
    }
}
