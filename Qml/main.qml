import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import Qt.labs.platform 1.1 as Platform
import Qt.labs.settings 1.0

Window {
    id: window

    width: 640
    height: 480
    visible: true
    title: qsTr("Media Organizer")

    property int toolboxHeight: 50
    property int toolboxButtonWidth: 100

    Item {

        anchors.fill: parent              

        SplitView {

            anchors.fill: parent
            orientation: Qt.Horizontal

            Column {
                id:favoriteFolders

                height: parent.height
                width: toolboxButtonWidth
                SplitView.maximumWidth: 400

                Button {
                    id: addFolderButton

                    width:parent.width
                    height:toolboxHeight

                    text: qsTr("Add Folder")

                    onClicked: {
                        addFolderDialog.open()
                    }
                }

                FavoriteFolders {
                    width: parent.width
                    height: parent.height - toolboxButtonWidth
                    model: foldersModel
                    onItemClicked: {
                        foldersModel.itemClicked(index)
                    }
                }
            }

            Column {
                id: toolbox

                height: toolboxHeight

                Row {
                    id: toolboxRow
                    width: parent.width
                    height: toolboxHeight

                    Button {
                        width: toolboxButtonWidth
                        height: parent.height

                        enabled: browsing.upEnabled
                        text: qsTr("[..]")

                        onClicked: {
                            browsing.goParent()
                        }
                    }

                    Button {
                        width: toolboxButtonWidth
                        height: parent.height

                        enabled: browsing.backEnabled
                        text: qsTr("<-")

                        onClicked: {
                            browsing.goBack()
                        }
                    }

                    Button {
                        width: toolboxButtonWidth
                        height: parent.height

                        enabled: browsing.forwardEnabled
                        text: qsTr("->")

                        onClicked: {
                            browsing.goForward()
                        }
                    }

                    Button {
                        width: toolboxButtonWidth
                        height: parent.height

                        enabled: browsing.prevFolderEnabled
                        text: qsTr("[]<-")

                        onClicked: {
                            browsing.goPrevFolder()
                        }
                    }

                    Button {
                        width: toolboxButtonWidth
                        height: parent.height

                        enabled: browsing.nextFolderEnabled
                        text: qsTr("[]->")

                        onClicked: {
                            browsing.goNextFolder()
                        }
                    }

                    Label {
                        id: dir

                        width: parent.width - x
                        height: parent.height

                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: browsing.currentPath
                        background: Rectangle {
                            color: "white"
                            opacity: 1
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: browsing.openCurrentFolder()
                        }
                    }
                }

                ThumbsGrid {
                    id: grid

                    width: parent.width
                    height: parent.height - toolboxHeight
                    clip: true
                    onScrollPositionChanged: browsing.setScrollPos(scrollPosition)

                    model: thumbsModel
                    Connections {
                            target: thumbsModel
                            onShowItem: grid.showItem(pos, newScrollPosition)
                            onClearSelection: grid.currentIndex = -1
                    }
                    onItemClicked: {
                        grid.currentIndex = index
                        thumbsModel.selectItem(index)
                    }
                    onItemMiddleClicked: {
                        grid.currentIndex = index
                        thumbsModel.setItemAsThumb(index)
                    }
                    onRemoveSelected: {
                        thumbsModel.removeItem(index)
                    }
                }
            }
        }
        Keys.onPressed: {
                 if (event.key == Qt.Key_Backspace) {
                     browsing.goBack();
                     event.accepted = true;
                 }
             }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.BackButton | Qt.ForwardButton
            onClicked: {
                   if (mouse.button == Qt.BackButton) {
                       browsing.goBack()
                   } else if (mouse.button == Qt.ForwardButton) {
                       browsing.goForward()
                   }
            }
        }
    }

    Platform.FolderDialog {
        id: addFolderDialog
        onAccepted: foldersModel.addFolder(folder)
    }

    Settings {
        property alias x: window.x
        property alias y: window.y
        property alias width: window.width
        property alias height: window.height
        property alias favoriteFoldersWidth: favoriteFolders.width
    }
}
