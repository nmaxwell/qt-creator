/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

import QtQuick 1.0
import qtcomponents 1.0

ScrollArea {

    property alias model: repeater.model

    property alias listHeight: column.height

    height: Math.min(listHeight, 276)

    frame: false
    horizontalScrollBar.visible: false
    clip: true

    Column {
        id: column

        spacing: 8

        Repeater {
            id: repeater
            SessionItem {

                function fullSessionName()
                {
                    var newSessionName = sessionName
                    if (model.lastSession && sessionList.isDefaultVirgin())
                        newSessionName = qsTr("%1 (last session)").arg(sessionName);
                    else if (model.activeSession && !sessionList.isDefaultVirgin())
                        newSessionName = qsTr("%1 (current session)").arg(sessionName);
                    return newSessionName;
                }

                name: fullSessionName()
            }
        }
    }
}
