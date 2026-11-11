import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.11
import Qt.labs.settings 1.0

Window {
    id: dlg

    property string text: ""
    property bool rescan: false
    property string lastText: ""

    Settings {
        category: "Search"
        property alias lastText: dlg.lastText
    }
    signal accepted()

    function open() {
        input.text = lastText
        rescanBox.checked = false
        visible = true
        input.forceActiveFocus()
        input.selectAll()
    }

    function accept() {
        if (input.text.trim() === "")
            return
        text = input.text
        lastText = input.text
        rescan = rescanBox.checked
        close()
        accepted()
    }

    title: qsTr("Search")
    width: 420
    height: 260
    minimumWidth: 360
    minimumHeight: 240
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.WindowCloseButtonHint | Qt.WindowTitleHint

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            text: qsTr("File name contains all of these words:")
        }

        TextField {
            id: input
            Layout.fillWidth: true
            selectByMouse: true
            onAccepted: dlg.accept()
        }

        CheckBox {
            id: rescanBox
            text: qsTr("Rescan")
            checked: false
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 10
            Button {
                text: qsTr("OK")
                highlighted: true
                enabled: input.text.trim() !== ""
                onClicked: dlg.accept()
            }
            Button {
                text: qsTr("Cancel")
                onClicked: dlg.close()
            }
        }
    }
}
