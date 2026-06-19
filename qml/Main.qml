import QtQuick
import QtQuick.Window
import MediaPlayer

Window {
    width: 1280
    height: 720
    visible: true
    title: qsTr("MediaPlayer")
    color: "#1E1E1E"

    VideoDisplay {
        objectName: "myVideo"
        anchors.fill: parent
    }
}