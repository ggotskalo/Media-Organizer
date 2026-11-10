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
    property string shownFullUrl: ""
    property bool applyingNavigation: false
    property bool viewerReady: false
    property int shownBuffer: 0
    property int pendingBuffer: 1
    property string pendingUrl: ""
    property string bufferUrl0: ""
    property string bufferUrl1: ""
    property bool imageFailed: false

    readonly property int fitDecodeWidth: Math.max(1, Math.round(width))
    readonly property int fullDecodeWidth: {
        const zoomed = Math.round(fitDecodeWidth * Math.max(zoomScale, 1))
        const dpr = Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
        const nativeDips = nativePixelWidth > 0 ? Math.max(1, Math.round(nativePixelWidth / dpr)) : 0
        if (nativeDips > 0)
            return Math.min(zoomed, nativeDips)
        return zoomed
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

    function pixelWidth(dips) {
        const dpr = Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
        return Math.max(1, Math.round(dips * dpr))
    }

    function viewUrl(path, dips) {
        if (path.length === 0)
            return ""
        return "image://viewer/" + pixelWidth(dips) + "/" + encodeURIComponent(path)
    }

    function neighborPaths() {
        const len = pictureList.length
        if (len === 0 || currentPicturePos < 0)
            return []
        const offsets = [0, 1, 2, 3, -1]
        const paths = []
        const seen = {}
        for (let n = 0; n < offsets.length; ++n) {
            const pos = (currentPicturePos + offsets[n] + len * 8) % len
            const path = pictureList[pos]
            if (seen[path])
                continue
            seen[path] = true
            paths.push(path)
        }
        return paths
    }

    function schedulePrefetch() {
        if (!viewerCache || pictureList.length === 0 || currentPicturePos < 0)
            return
        const fit = pixelWidth(fitDecodeWidth)
        const full = pixelWidth(fullDecodeWidth)
        viewerCache.prefetch(neighborPaths(), fit)
        if (full !== fit)
            viewerCache.bump(imagePath, full)
    }

    function noteBufferStatus(buffer, status) {
        if (buffer !== pendingBuffer)
            return
        if (status === Image.Error) {
            imageFailed = true
            return
        }
        if (status !== Image.Ready)
            return
        const url = buffer === 0 ? bufferUrl0 : bufferUrl1
        if (url !== pendingUrl)
            return
        imageFailed = false
        shownBuffer = buffer
        shownFullUrl = url
    }

    function applyFullImage() {
        if (!viewerReady || imagePath.length === 0)
            return
        const url = viewUrl(imagePath, fullDecodeWidth)
        if (url.length === 0 || url === shownFullUrl)
            return
        const next = shownBuffer === 0 ? 1 : 0
        pendingBuffer = next
        pendingUrl = url
        imageFailed = false
        const item = next === 0 ? bufferA : bufferB
        const loadedUrl = next === 0 ? bufferUrl0 : bufferUrl1
        if (loadedUrl === url && item.status === Image.Ready) {
            shownBuffer = next
            shownFullUrl = url
            return
        }
        if (next === 0)
            bufferUrl0 = url
        else
            bufferUrl1 = url
        item.source = url
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
        applyingNavigation = false
        schedulePrefetch()
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
        shownFullUrl = ""
        pendingUrl = ""
        bufferUrl0 = ""
        bufferUrl1 = ""
        shownBuffer = 0
        pendingBuffer = 1
        imageFailed = false
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
        bufferA.source = ""
        bufferB.source = ""
        if (viewerCache)
            viewerCache.prefetch([], 1)
    }

    Component.onCompleted: viewerReady = true

    onFullDecodeWidthChanged: {
        if (!viewerReady || applyingNavigation)
            return
        schedulePrefetch()
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
                id: bufferA
                anchors.fill: parent
                visible: viewer.shownBuffer === 0
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                source: ""
                onStatusChanged: viewer.noteBufferStatus(0, status)
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

            Image {
                id: bufferB
                anchors.fill: parent
                visible: viewer.shownBuffer === 1
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                source: ""
                onStatusChanged: viewer.noteBufferStatus(1, status)
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

        Label {
            anchors.centerIn: parent
            visible: viewer.imageFailed
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
                    .arg(viewer.shownBuffer === 0
                         ? (bufferA.implicitWidth > 0 ? bufferA.implicitWidth : 0)
                         : (bufferB.implicitWidth > 0 ? bufferB.implicitWidth : 0))
                    .arg(viewer.shownBuffer === 0
                         ? (bufferA.implicitHeight > 0 ? bufferA.implicitHeight : 0)
                         : (bufferB.implicitHeight > 0 ? bufferB.implicitHeight : 0))
                    .arg(viewer.formatSize(viewer.currentFileSize))
            }
        }

    }
}
