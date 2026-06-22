import QtQuick
import QtQuick.Window
import MediaPlayer

Window {
    id: rootWindow
    width: 1280
    height: 720
    visible: true
    title: qsTr("MediaPlayer")
    color: "#050505"

    function formatTime(seconds) {
        if (isNaN(seconds)) return "00:00";
        var m = Math.floor(seconds / 60);
        var s = Math.floor(seconds % 60);
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "" + s);
    }

    property bool isPlaying: true;

    PlayerController {
        id: controller
        Component.onCompleted: {
            controller.setVideoDisplay(myVideo)
        }
    }

    VideoDisplay {
        id: myVideo
        anchors.fill: parent
    }

    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.right: parent.right
    }
}