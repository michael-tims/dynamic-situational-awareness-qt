
// Copyright 2016 ESRI
//
// All rights reserved under the copyright laws of the United States
// and applicable international laws, treaties, and conventions.
//
// You may freely redistribute and use this sample code, with or
// without modification, provided you include the original copyright
// notice and use restrictions.
//
// See the Sample code usage restrictions document for further information.
//

import QtQuick 2.6
import QtQuick.Controls 2.1
import QtQuick.Controls.Material 2.1
import Esri.DSA 1.0

DsaToolBase {
    id: messageFeedsRoot
    width: 272 * scaleFactor
    title: qsTr("Message Feeds")

    // Create the controller
    MessageFeedsController {
        id: toolController
    }

    // Declare the ListView, which will display the list of message overlay feeds
    ListView {
        id: messageFeedsList

        anchors {
            top: titleBar.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 8 * scaleFactor
        }

        clip: true
        model: toolController.messageFeeds
        width: parent.width
        delegate:  SwitchDelegate {
            id: control
            text: feedName
            checked: feedEnabled
            width: messageFeedsList.width

            contentItem: Label {
                rightPadding: control.indicator.width + control.spacing
                text: control.text
                font: control.font
                opacity: enabled ? 1.0 : 0.3
                color: Material.primary
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: feedEnabled = !feedEnabled
        }
    }
}