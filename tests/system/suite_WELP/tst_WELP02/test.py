#############################################################################
##
## Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/legal
##
## This file is part of Qt Creator.
##
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and Digia.  For licensing terms and
## conditions see http://qt.digia.com/licensing.  For further information
## use the contact form at http://qt.digia.com/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 2.1 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL included in the
## packaging of this file.  Please review the following information to
## ensure the GNU Lesser General Public License version 2.1 requirements
## will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## In addition, as a special exception, Digia gives you certain additional
## rights.  These rights are described in the Digia Qt LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
#############################################################################

source("../../shared/qtcreator.py")
source("../../shared/suites_qtta.py")

def checkTypeAndProperties(typePropertiesDetails):
    for (qType, props, detail) in typePropertiesDetails:
        test.verify(checkIfObjectExists(getQmlItem(qType, ":WelcomePage.scrollView_ScrollView",
                                                   False, props)),
                    "Verifying: Qt Creator displays %s." % detail)

def main():
    if isQt4Build or not canTestEmbeddedQtQuick():
        test.log("Welcome mode is not available or not scriptable with this Squish version")
        return
    # prepare example project
    sourceExample = os.path.join(sdkPath, "Examples", "4.7", "declarative", "animation", "basics",
                                 "property-animation")
    if not neededFilePresent(sourceExample):
        return
    # open Qt Creator
    startApplication("qtcreator" + SettingsPath)
    if not startedWithoutPluginError():
        return

    typePropDet = (("Button", "text='Get Started Now' id='gettingStartedButton'",
                    "Get Started Now button"),
                   ("Text", "text='Sessions' id='sessionsTitle'", "Sessions section"),
                    ("Text", "text='default' id='text'", "default session listed"),
                   ("Text", "text='Recent Projects' id='recentProjectsTitle'", "Projects section"),
                   )
    checkTypeAndProperties(typePropDet)

    # select "Create Project" and try to create a new project
    createNewQtQuickApplication(tempDir(), "SampleApp", fromWelcome = True)
    test.verify(checkIfObjectExists("{column='0' container=':Qt Creator_Utils::NavigationTreeView'"
                                    " text~='SampleApp( \(.*\))?' type='QModelIndex'}"),
                "Verifying: The project is opened in 'Edit' mode after configuring.")
    # go to "Welcome page" again and verify updated information
    switchViewTo(ViewConstants.WELCOME)
    typePropDet = (("Text", "text='Sessions' id='sessionsTitle'", "Sessions section"),
                   ("Text", "text='default (current session)' id='text'",
                    "default session as current listed"),
                   ("Text", "text='Recent Projects' id='recentProjectsTitle'", "Projects section"),
                   ("LinkedText", "text='SampleApp' id='projectNameText'",
                    "current project listed in projects section")
                   )
    checkTypeAndProperties(typePropDet)

    # select "Open project" and select a project
    examplePath = os.path.join(prepareTemplate(sourceExample), "propertyanimation.pro")
    openQmakeProject(examplePath, fromWelcome = True)
    progressBarWait(30000)
    test.verify(checkIfObjectExists("{column='0' container=':Qt Creator_Utils::NavigationTreeView'"
                                    " text~='propertyanimation( \(.*\))?' type='QModelIndex'}"),
                "Verifying: The project is opened in 'Edit' mode after configuring.")
    # go to "Welcome page" again and check if there is an information about recent projects
    switchViewTo(ViewConstants.WELCOME)
    test.verify(checkIfObjectExists(getQmlItem("LinkedText", ":WelcomePage.scrollView_ScrollView",
                                               False,
                                               "text='propertyanimation' id='projectNameText'")) and
                checkIfObjectExists(getQmlItem("LinkedText", ":WelcomePage.scrollView_ScrollView",
                                               False, "text='SampleApp' id='projectNameText'")),
                "Verifying: 'Welcome page' displays information about recently created and "
                "opened projects.")
    # exit Qt Creator
    invokeMenuItem("File", "Exit")
