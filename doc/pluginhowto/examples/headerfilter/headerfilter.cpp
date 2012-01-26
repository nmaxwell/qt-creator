/***************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the documentation of Qt Creator.
**
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
****************************************************************************/

#include "headerfilter.h"

#include <extensionsystem/pluginmanager.h>
#include <find/searchresultwindow.h>
#include <projectexplorer/iprojectmanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/session.h>
#include <texteditor/basetexteditor.h>
#include <utils/filesearch.h>

#include <QFutureWatcher>
#include <QLabel>


using namespace Core;
using namespace Utils;


class HeaderFilterPrivate
{
public:
    HeaderFilterPrivate()
        : m_projectPlugin(0), m_searchResultWindow(0)
    {}

    ProjectExplorer::ProjectExplorerPlugin *projectExplorer()
     {
         if (m_projectPlugin)
             return m_projectPlugin;

         ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
         m_projectPlugin = pm->getObject<ProjectExplorer::ProjectExplorerPlugin>();
         return m_projectPlugin;
     }

    // Method to search and return the search window
    Find::SearchResultWindow *searchResultWindow()
    {
        if (m_searchResultWindow)
            return m_searchResultWindow;

        ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
        m_searchResultWindow = pm->getObject<Find::SearchResultWindow>();
        return m_searchResultWindow;
    }

    QFutureWatcher<FileSearchResult> watcher;

private:
    ProjectExplorer::ProjectExplorerPlugin *m_projectPlugin;
    Find::SearchResultWindow *m_searchResultWindow;
};

HeaderFilter::HeaderFilter()
{
    d = new HeaderFilterPrivate;
    d->watcher.setPendingResultsLimit(1);

    // displayResult(int) is called when every a new
    // search result is generated
    connect(&d->watcher, SIGNAL(resultReadyAt(int)),this, SLOT(displayResult(int)));
}

HeaderFilter::~HeaderFilter()
{
    delete d;
}

QString HeaderFilter::id() const
{
    return "HeaderFilter";
}

QString HeaderFilter::displayName() const
{
    return tr("Header Filter");
}

bool HeaderFilter::canCancel() const
{
    return false;
}

void HeaderFilter::cancel()
{
}

bool HeaderFilter::isEnabled() const
{
    QList<ProjectExplorer::Project *> projects = d->projectExplorer()->session()->projects();
    return !projects.isEmpty();
}

QKeySequence HeaderFilter::defaultShortcut() const
{
    return QKeySequence();
}

QWidget *HeaderFilter::createConfigWidget()
{
    return new QLabel("This is a header filter");
}

void HeaderFilter::findAll(const QString &text, Find::FindFlags findFlags)
{
    // Fetch a list of all open projects
    QList<ProjectExplorer::Project *> projects = d->projectExplorer()->session()->projects();

    // Make a list of files in each project
    QStringList files;
    foreach (ProjectExplorer::Project *project, projects)
        files += project->files(ProjectExplorer::Project::AllFiles);

    // Remove duplicates
    files.removeDuplicates();

    //------------------------------------------------------------
    // Begin searching
    QString includeline = "#include <" + text + '>';
    Find::SearchResult *result = d->searchResultWindow()->startNewSearch();

    d->watcher.setFuture(QFuture<FileSearchResult>());

    // When result gets activated it invokes the openEditor function
    connect(result, SIGNAL(activated(Find::SearchResultItem)),
            this, SLOT(openEditor(Find::SearchResultItem)));

    d->searchResultWindow()->popup(true);

    // Let the watcher monitor the search results
    d->watcher.setFuture(Utils::findInFiles(includeline, files, findFlags));
}

void HeaderFilter::displayResult(int index)
{
    FileSearchResult result = d->watcher.future().resultAt(index);

    d->searchResultWindow()->addResult(result.fileName,
                                       result.lineNumber,
                                       result.matchingLine,
                                       result.matchStart,
                                       result.matchLength);
}

void HeaderFilter::openEditor(const Find::SearchResultItem &item)
{
    TextEditor::BaseTextEditor::openEditorAt(item.fileName, item.lineNumber, item.index);
}

