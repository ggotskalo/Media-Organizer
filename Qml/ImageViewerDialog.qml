import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.11
import Qt.labs.settings 1.0

Window {
    id: viewer
    width: 1000
    height: 700
    visible: false
    modality: Qt.NonModal
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint
    color: "black"
    title: imagePath.length > 0 ? imagePath.split("/").pop() : qsTr("Image Viewer")

    property string imagePath: ""
    property int currentIndex: -1
    property var pictureList: []
    property int currentPicturePos: -1
    property int currentOrdinal: 0
    property int totalPictures: 0
    property real currentFileSize: 0
    property real zoomScale: 1.0
    property real panX: 0
    property real panY: 0
    property bool selecting: false
    property real selectionStartX: 0
    property real selectionStartY: 0
    property real selectionEndX: 0
    property real selectionEndY: 0
    property int nativePixelWidth: 0
    property int pendingWheelSteps: 0
    property var preloadPaths: []
    property string shownFullUrl: ""
    property int appliedDecodeWidth: 0
    property bool applyingNavigation: false
    property bool viewerReady: false

    readonly property int fitDecodeWidth: Math.max(1, Math.round(width))
    readonly property int fullDecodeWidth: {
        const zoomed = Math.round(fitDecodeWidth * Math.max(zoomScale, 1))
        const dpr = Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
        const nativeDips = nativePixelWidth > 0 ? Math.max(1, Math.round(nativePixelWidth / dpr)) : 0
        if (nativeDips > 0)
            return Math.min(zoomed, nativeDips)
        return zoomed
    }

    function toFileUrl(path) {
        return path.length > 0 ? "file:///" + path : ""
    }

    function formatSize(bytes) {
        if (bytes <= 0)
            return "0 B"
        if (bytes >= 1024 * 1024 * 1024)
            return (bytes / (1024 * 1024 * 1024)).toFixed(2) + " GB"
        if (bytes >= 1024 * 1024)
            return (bytes / (1024 * 1024)).toFixed(1) + " MB"
        if (bytes >= 1024)
            return (bytes / 1024).toFixed(0) + " KB"
        return bytes.toFixed(0) + " B"
    }

    function updatePreloadSources() {
        const len = pictureList.length
        if (len <= 1 || currentPicturePos < 0) {
            preloadPaths = []
            return
        }

        const offsets = [-1, 1, 2, 3]
        const paths = []
        const seen = {}
        seen[pictureList[currentPicturePos]] = true
        for (let n = 0; n < offsets.length; ++n) {
            const pos = (currentPicturePos + offsets[n] + len * 4) % len
            const path = pictureList[pos]
            if (seen[path])
                continue
            seen[path] = true
            paths.push(path)
        }
        preloadPaths = paths
    }

    function applyFullImage() {
        if (!viewerReady)
            return
        const url = toFileUrl(imagePath)
        const decodeWidth = fullDecodeWidth
        if (shownFullUrl !== url) {
            fullImage.sourceSize = Qt.size(decodeWidth, 0)
            fullImage.source = url
            shownFullUrl = url
            appliedDecodeWidth = decodeWidth
        } else if (appliedDecodeWidth !== decodeWidth) {
            fullImage.sourceSize = Qt.size(decodeWidth, 0)
            appliedDecodeWidth = decodeWidth
        }
    }

    function resetZoom() {
        zoomScale = 1.0
        panX = 0
        panY = 0
        applyFullImage()
    }

    function normalizedSelectionRect() {
        const x = Math.min(selectionStartX, selectionEndX)
        const y = Math.min(selectionStartY, selectionEndY)
        const w = Math.abs(selectionEndX - selectionStartX)
        const h = Math.abs(selectionEndY - selectionStartY)
        return Qt.rect(x, y, w, h)
    }

    function applySelectionZoom() {
        const sel = normalizedSelectionRect()
        if (sel.width < 8 || sel.height < 8)
            return

        const factor = Math.min(width / sel.width, height / sel.height)
        if (factor <= 1.0)
            return

        const centerX = width / 2
        const centerY = height / 2
        const selCenterX = sel.x + sel.width / 2
        const selCenterY = sel.y + sel.height / 2

        zoomScale = zoomScale * factor
        panX = panX - (selCenterX - centerX) * factor
        panY = panY - (selCenterY - centerY) * factor
        applyFullImage()
    }

    function showPicturePos(pos) {
        if (pictureList.length === 0 || pos < 0 || pos >= pictureList.length)
            return
        const path = pictureList[pos]
        applyingNavigation = true
        pendingWheelSteps = 0
        wheelNavTimer.stop()
        zoomScale = 1.0
        panX = 0
        panY = 0
        imagePath = path
        currentPicturePos = pos
        currentOrdinal = pos + 1
        totalPictures = pictureList.length
        currentFileSize = thumbsModel.imageFileSizeByPath(path)
        nativePixelWidth = thumbsModel.imagePixelWidth(path)
        updatePreloadSources()
        applyingNavigation = false
        applyFullImage()
    }

    function showIndex(index) {
        if (index < 0)
            return
        const path = thumbsModel.imagePathAt(index)
        if (path.length === 0)
            return
        currentIndex = index
        if (pictureList.length === 0)
            pictureList = thumbsModel.picturePaths()
        const pos = pictureList.indexOf(path)
        if (pos < 0)
            return
        showPicturePos(pos)
    }

    function showNext() {
        if (pictureList.length === 0 || currentPicturePos < 0)
            return
        showPicturePos((currentPicturePos + 1) % pictureList.length)
    }

    function showPrev() {
        if (pictureList.length === 0 || currentPicturePos < 0)
            return
        showPicturePos((currentPicturePos - 1 + pictureList.length) % pictureList.length)
    }

    function showFirst() {
        if (pictureList.length === 0)
            return
        showPicturePos(0)
    }

    function showLast() {
        if (pictureList.length === 0)
            return
        showPicturePos(pictureList.length - 1)
    }

    function queueWheel(steps) {
        if (pictureList.length === 0 || steps === 0)
            return
        pendingWheelSteps += steps
        wheelNavTimer.restart()
    }

    function applyPendingWheel() {
        const steps = pendingWheelSteps
        pendingWheelSteps = 0
        if (steps === 0 || pictureList.length === 0 || currentPicturePos < 0)
            return
        const len = pictureList.length
        let pos = (currentPicturePos + steps) % len
        if (pos < 0)
            pos += len
        showPicturePos(pos)
    }

    function start(index) {
        pictureList = thumbsModel.picturePaths()
        currentPicturePos = -1
        showIndex(index)
        if (imagePath.length === 0)
            return
        visible = true
        requestActivate()
    }

    onClosing: {
        applyingNavigation = true
        imagePath = ""
        currentIndex = -1
        pictureList = []
        currentPicturePos = -1
        preloadPaths = []
        shownFullUrl = ""
        appliedDecodeWidth = 0
        nativePixelWidth = 0
        pendingWheelSteps = 0
        wheelNavTimer.stop()
        currentOrdinal = 0
        totalPictures = 0
        currentFileSize = 0
        selecting = false
        zoomScale = 1.0
        panX = 0
        panY = 0
        applyingNavigation = false
        fullImage.source = ""
    }

    Component.onCompleted: viewerReady = true

    onFullDecodeWidthChanged: {
        if (!viewerReady || applyingNavigation)
            return
        applyFullImage()
    }

    Timer {
        id: wheelNavTimer
        interval: 50
        repeat: false
        onTriggered: viewer.applyPendingWheel()
    }

    Shortcut {
        sequence: StandardKey.Cancel
        onActivated: viewer.close()
    }
    Shortcut {
        sequence: "Left"
        onActivated: viewer.showPrev()
    }
    Shortcut {
        sequence: "Right"
        onActivated: viewer.showNext()
    }
    Shortcut {
        sequence: "Home"
        onActivated: viewer.showFirst()
    }
    Shortcut {
        sequence: "End"
        onActivated: viewer.showLast()
    }

    Settings {
        category: "ImageViewerDialog"
        property alias x: viewer.x
        property alias y: viewer.y
        property alias width: viewer.width
        property alias height: viewer.height
    }

    Rectangle {
        anchors.fill: parent
        color: "black"

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
            onWheel: {
                const delta = wheel.angleDelta.y
                if (delta !== 0) {
                    const notches = Math.max(1, Math.round(Math.abs(delta) / 120))
                    viewer.queueWheel(delta > 0 ? -notches : notches)
                }
                wheel.accepted = true
            }
            onPressed: {
                if (mouse.button === Qt.LeftButton) {
                    viewer.selecting = true
                    viewer.selectionStartX = mouse.x
                    viewer.selectionStartY = mouse.y
                    viewer.selectionEndX = mouse.x
                    viewer.selectionEndY = mouse.y
                }
            }
            onPositionChanged: {
                if (viewer.selecting) {
                    viewer.selectionEndX = mouse.x
                    viewer.selectionEndY = mouse.y
                }
            }
            onReleased: {
                if (mouse.button === Qt.LeftButton && viewer.selecting) {
                    viewer.selecting = false
                    const sel = viewer.normalizedSelectionRect()
                    if (sel.width < 10 || sel.height < 10)
                        viewer.resetZoom()
                    else
                        viewer.applySelectionZoom()
                }
            }
            onClicked: {
                if (mouse.button === Qt.MiddleButton) {
                    if (viewer.visibility === Window.FullScreen)
                        viewer.showNormal()
                    else
                        viewer.showFullScreen()
                } else if (mouse.button === Qt.RightButton) {
                    viewerContextMenu.popup()
                }
            }
        }

        Menu {
            id: viewerContextMenu
            MenuItem {
                text: qsTr("Copy to $best")
                onTriggered: thumbsModel.copyImageToBestPath(viewer.imagePath)
            }
            MenuItem {
                text: qsTr("Reset zoom")
                onTriggered: viewer.resetZoom()
            }
        }

        Item {
            id: imageViewport
            anchors.fill: parent
            clip: true

            Image {
                id: fullImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
                autoTransform: true
                transform: [
                    Scale {
                        origin.x: imageViewport.width / 2
                        origin.y: imageViewport.height / 2
                        xScale: viewer.zoomScale
                        yScale: viewer.zoomScale
                    },
                    Translate {
                        x: viewer.panX
                        y: viewer.panY
                    }
                ]
            }
        }

        Rectangle {
            visible: viewer.selecting
            x: Math.min(viewer.selectionStartX, viewer.selectionEndX)
            y: Math.min(viewer.selectionStartY, viewer.selectionEndY)
            width: Math.abs(viewer.selectionEndX - viewer.selectionStartX)
            height: Math.abs(viewer.selectionEndY - viewer.selectionStartY)
            color: "#33ffffff"
            border.color: "#ffffffff"
            border.width: 1
        }

        // Screen-sized neighbors: one back and three ahead.
        Repeater {
            id: preloadRepeater
            model: viewer.preloadPaths
            Image {
                visible: false
                asynchronous: true
                cache: true
                autoTransform: true
                sourceSize.width: viewer.fitDecodeWidth
                sourceSize.height: 0
                source: viewer.toFileUrl(modelData)
            }
        }

        Label {
            anchors.centerIn: parent
            visible: viewer.imagePath.length === 0 || fullImage.status === Image.Error
            color: "white"
            text: qsTr("Unable to display image")
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 30
            color: "transparent"
            visible: viewer.imagePath.length > 0

            Label {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                verticalAlignment: Text.AlignVCenter
                color: "white"
                style: Text.Outline
                styleColor: "#cc000000"
                text: qsTr("%1/%2   %3x%4   %5")
                    .arg(viewer.currentOrdinal)
                    .arg(viewer.totalPictures)
                    .arg(fullImage.implicitWidth > 0 ? fullImage.implicitWidth :
                         (fullImage.sourceSize.width > 0 ? fullImage.sourceSize.width : 0))
                    .arg(fullImage.implicitHeight > 0 ? fullImage.implicitHeight :
                         (fullImage.sourceSize.height > 0 ? fullImage.sourceSize.height : 0))
                    .arg(viewer.formatSize(viewer.currentFileSize))
            }
        }

    }
}
