import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14

ListView {
    signal itemClicked(index: int)
    delegate: Rectangle {
        width: parent.width
        height: Style.favoritesItemHeight
        border.color: "black"
        border.width: 1
        Column {
            anchors.fill: parent
            anchors.margins: 2
            spacing: 0
            Text {
                width: parent.width
                height: model.freeSpace !== "" ? parent.height - freeLabel.height : parent.height
                text: model.name
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                elide: Text.ElideRight
            }
            Text {
                id: freeLabel
                width: parent.width
                visible: model.freeSpace !== ""
                height: visible ? implicitHeight : 0
                text: model.freeSpace
                font.pixelSize: 10
                color: "#666"
                horizontalAlignment: Text.AlignHCenter
            }
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
                    foldersMenu.popup()
                }
            }
        }
    }    

    FavoriteFoldersMenu {
        id: foldersMenu
    }
}
