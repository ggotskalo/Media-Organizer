import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.11
import Qt.labs.platform 1.1 as Platform
import Qt.labs.settings 1.0

Window {
    id: window

    width: 640
    height: 480
    visible: true
    title: qsTr("Media Organizer")

    Item {
        id: mainItem
        anchors.fill: parent
        focus: true

            SplitView {
            
            anchors {
                fill: parent
                bottomMargin: statusBar.height
            }
            orientation: Qt.Horizontal

            ColumnLayout {
                id:favoriteFolders

                height: parent.height
                width: Style.favoritesColumnWidth
                SplitView.maximumWidth: Style.favoritesColumnMaximumWidth
                spacing:0

                Button {
                    id: addFolderButton

                    Layout.fillWidth: true
                    Layout.minimumHeight: Style.toolboxHeight

                    text: qsTr("Add Folder")

                    onClicked: {
                        addFolderDialog.open()
                    }
                }

                FavoriteFolders {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip:true
                    model: foldersModel
                    onItemClicked: {
                        foldersModel.itemClicked(index)
                    }
                }

            Column {
                id: toolbox
                
                width: parent.width

                RowLayout {
                    id: filterRow
                    width: parent.width
                    height: 35
                    spacing: 0
                    visible: false
                    
                    TextField {
                        id: filterField
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        placeholderText: qsTr("Filter files...")
                        selectByMouse: true
                        rightPadding: clearButton.visible ? clearButton.width + 5 : 5
                        
                        onTextChanged: {
                            thumbsModel.filterText = text
                        }
                        
                        Keys.onEscapePressed: {
                            text = ""
                            focus = false
                            filterRow.visible = false
                        }
                        
                        // Clear button (X icon) inside TextField
                        Text {
                            id: clearButton
                            anchors {
                                right: parent.right
                                rightMargin: 8
                                verticalCenter: parent.verticalCenter
                            }
                            visible: filterField.text.length > 0
                            text: "×"
                            font.pixelSize: 20
                            color: "#666"
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            
                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -5
                                onClicked: {
                                    filterField.text = ""
                                    filterField.focus = true
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    id: toolboxRow
                    width: parent.width
                    height: Style.toolboxHeight
                    spacing: 0

                    BigToolButton {
                        enabled: browsing.upEnabled
                        text: qsTr("[..]")
                        onClicked: browsing.goParent()
                    }

                    BigToolButton {
                        enabled: browsing.backEnabled
                        text: qsTr("<-")
                        onClicked: browsing.goBack()
                    }

                    BigToolButton {
                        enabled: browsing.forwardEnabled
                        text: qsTr("->")
                        onClicked: browsing.goForward()
                    }

                    BigToolButton {
                        enabled: browsing.prevFolderEnabled
                        text: qsTr("[]<-")
                        onClicked: browsing.goPrevFolder()
                    }

                    BigToolButton {
                        enabled: browsing.nextFolderEnabled
                        text: qsTr("[]->")
                        onClicked: browsing.goNextFolder()
                    }

                    BigToolButton {
                        visible: thumbsModel.brokenShortcutsCount > 0
                        text: qsTr("Dead links: %1").arg(thumbsModel.brokenShortcutsCount)
                        onClicked: {
                            brokenShortcutsDialog.start(
                                thumbsModel.brokenShortcutPaths(),
                                thumbsModel.brokenTargetNames(),
                                foldersModel.getAllFolderPaths()
                            )
                        }
                        palette.button: "#ffe0e0"
                    }

                    Label {
                        id: dir

                        Layout.fillWidth: true
                        Layout.fillHeight: true

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
                    height: parent.height - Style.toolboxHeight - (filterRow.visible ? filterRow.height : 0)
                    clip: true
                    onScrollPositionChanged: browsing.setScrollPos(scrollPosition)

                    model: thumbsModel
                    Connections {
                            target: thumbsModel
                            function onShowItem(pos, newScrollPosition) {
                                grid.showItem(pos, newScrollPosition)
                            }
                            function onClearSelection() {
                                grid.currentIndex = -1
                            }
                    }
                    onItemClicked: {
                        grid.currentIndex = index
                        if (thumbsModel.isPictureIndex(index)) {
                            imageViewerDialog.start(index)
                        } else {
                            thumbsModel.selectItem(index)
                        }
                    }
                    onItemMiddleClicked: {
                        grid.currentIndex = index
                        thumbsModel.setItemAsThumb(index)
                    }
                    onRemoveSelected: function(index, posX, posY) {
                        if (thumbsModel.isFolderIndex(index)) {
                            removeFolderConfirmDialog.indexToRemove = index
                            var p = grid.mapToItem(window.contentItem, posX, posY)
                            removeFolderConfirmDialog.x = Math.max(0, Math.min(window.width - removeFolderConfirmDialog.width, p.x))
                            removeFolderConfirmDialog.y = Math.max(0, Math.min(window.height - removeFolderConfirmDialog.height, p.y))
                            removeFolderConfirmDialog.open()
                        } else {
                            thumbsModel.removeItem(index)
                        }
                    }
                    onMoveToParent: {
                        thumbsModel.moveToParent(index)
                    }
                    onShowInExplorer: {
                        thumbsModel.showInExplorer(index)
                    }
                    onShowShortcutInExplorer: {
                        thumbsModel.showShortcutInExplorer(index)
                    }
                    onCutItem: {
                        thumbsModel.cutItem(index)
                    }
                    onCopyItem: {
                        thumbsModel.copyItem(index)
                    }
                    onPasteItems: {
                        thumbsModel.pasteItems()
                    }
                    onPasteLinkItems: {
                        thumbsModel.pasteLinkItems()
                    }
                }
            }
        }
        
        // Status bar at the bottom
        Rectangle {
            id: statusBar
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: statusText.text.length > 0 ? 25 : 0
            color: "#f0f0f0"
            border.color: "#d0d0d0"
            border.width: 1
            
            Behavior on height {
                NumberAnimation { duration: 150 }
            }
            
            Label {
                id: statusText
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    margins: 5
                }
                text: statusManager.statusText
                elide: Text.ElideRight
                color: "#333"
            }
        }
        
        Keys.onPressed: {
            if (event.key == Qt.Key_Backspace) {
                browsing.goBack();
                event.accepted = true;
            }
        }
        
        Shortcut {
            sequence: StandardKey.Copy
            onActivated: {
                if (grid.currentIndex >= 0)
                    thumbsModel.copyItem(grid.currentIndex)
            }
        }
        Shortcut {
            sequence: StandardKey.Cut
            onActivated: {
                if (grid.currentIndex >= 0)
                    thumbsModel.cutItem(grid.currentIndex)
            }
        }
        Shortcut {
            sequence: StandardKey.Paste
            onActivated: thumbsModel.pasteItems()
        }
        Shortcut {
            sequence: "Ctrl+F"
            onActivated: {
                if (filterRow.visible) {
                    // Hide filter: clear text first
                    filterField.text = ""
                    filterField.focus = false
                    filterRow.visible = false
                    mainItem.focus = true
                } else {
                    // Show filter
                    filterRow.visible = true
                    // Use Qt.callLater to ensure field is visible before setting focus
                    Qt.callLater(function() {
                        filterField.forceActiveFocus()
                    })
                }
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

    BrokenShortcutsDialog {
        id: brokenShortcutsDialog
    }

    Dialog {
        id: removeFolderConfirmDialog
        modal: true
        width: 360
        title: qsTr("Confirm folder deletion")
        standardButtons: Dialog.Yes | Dialog.No
        property int indexToRemove: -1

        contentItem: Label {
            text: qsTr("Delete this folder and all its contents?")
            wrapMode: Text.WordWrap
            padding: 12
            width: 320
        }

        onAccepted: {
            if (indexToRemove >= 0)
                thumbsModel.removeItem(indexToRemove)
            indexToRemove = -1
        }
        onRejected: indexToRemove = -1
    }

    ImageViewerDialog {
        id: imageViewerDialog
    }

    Settings {
        property alias x: window.x
        property alias y: window.y
        property alias width: window.width
        property alias height: window.height
        property alias favoriteFoldersWidth: favoriteFolders.width
    }
}
