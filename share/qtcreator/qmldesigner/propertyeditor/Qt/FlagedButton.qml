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

import Qt 4.7
import Bauhaus 1.0

QPushButton {
    id: flagedButton
    property bool flagActive: false
    property variant backendValue    
    property variant theValue: backendValue.value;   
    property bool blueHigh: false    
    property bool baseStateFlag: isBaseState;
    
    onBaseStateFlagChanged: {
        evaluate();
    }
    
    property bool isInModel: backendValue.isInModel;
    onIsInModelChanged: {
        evaluate();
    }
    property bool isInSubState: backendValue.isInSubState;
    onIsInSubStateChanged: {
        evaluate();
    }        
    
    onTheValueChanged: {        
        evaluate();
    }
    
     function evaluate() {
        if (!enabled) {
            fontSelector.setStyleSheet("color: "+scheme.disabledColor);
        } else {
            if (baseStateFlag) {
                if (backendValue != null && backendValue.isInModel)
                    flagActive = true;
                else
                    flagActive = false;
            } else {
                if (backendValue != null && backendValue.isInSubState)
                    flagActive = true;
                else
                    flagActive = false;
            }
        }
    }
}