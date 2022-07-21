import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14

GridView {
    id: grid
    signal itemClicked(var index)
    signal itemMiddleClicked(var index)
    signal itemRightClicked(var index)
    property int thumbWidth: 100
    property int thumbHeigth: 100
    property int gap: 3
    property int selectionBorder: 3
    property int border: 1
    property alias scrollPosition: scroll.position
    function showItem(index, newScrollPosition) {
        currentIndex = index
        if (newScrollPosition <= (height - scroll.contentItem.height) / height) {
            scrollPosition = newScrollPosition
        }        
    }

    snapMode: GridView.SnapToRow
    Keys.onEscapePressed:  Qt.quit()
    width: parent.width
    ScrollBar.vertical: ScrollBar {
        id: scroll
        width:15
        visible: true
    }
    cellWidth: thumbWidth + gap + border*2
    cellHeight: thumbHeigth + gap + border*2
    rightMargin: -gap
    bottomMargin: -gap
    focus: true
    highlight:
        Rectangle {
            width: 150
            z: 2
            color: "transparent"
            border.color: "crimson"
            border.width: selectionBorder
        }
    highlightMoveDuration: 0
    delegate:
        Rectangle {
        id: del
        width: grid.cellWidth - gap
        height: grid.cellHeight - gap
        color: "transparent"
        border.color: "black"
        border.width: grid.border
        Image {
            id: thumb
            sourceSize.width: width
            sourceSize.height: height
            verticalAlignment: Image.AlignTop
            fillMode: Image.PreserveAspectFit
            anchors{
                fill: parent
                margins: grid.border
            }
            source: model.source ? "image://thumb/" + model.source : ""
            cache: false

        }
        Text {
            id: fileName
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            height: Math.max(fontInfo.pixelSize + grid.border*2, parent.height - thumb.paintedHeight - grid.border)

            anchors{
                bottom: parent.bottom
                left: parent.left
                right: parent.right
                margins: grid.border
            }
            text: model.isFolder ? "["+name+"]" : name
            elide: Text.ElideRight
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            style : Text.Outline
            styleColor: "white"
        }
        MouseArea {
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
            hoverEnabled: true
            anchors.fill: parent
            onClicked: {
                if (mouse.button == Qt.LeftButton)
                    itemClicked(index)
                if (mouse.button == Qt.MiddleButton)
                    itemMiddleClicked(index)
                if (mouse.button == Qt.RightButton) {
                    currentIndex = index
                 //   itemLeftClicked(index)
                }
            }
        }

    }

}
