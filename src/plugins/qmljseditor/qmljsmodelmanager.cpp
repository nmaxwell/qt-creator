/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "qmljseditorconstants.h"
#include "qmljsmodelmanager.h"
#include "qmljseditor.h"

#include <coreplugin/icore.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/mimedatabase.h>
#include <qmljs/qmljsinterpreter.h>
#include <qmljs/qmljsbind.h>
#include <qmljs/parser/qmldirparser_p.h>
#include <texteditor/itexteditor.h>
#include <projectexplorer/project.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QtConcurrentRun>
#include <qtconcurrent/runextensions.h>
#include <QTextStream>
#include <QCoreApplication>

#include <QDebug>

using namespace QmlJS;
using namespace QmlJSEditor;
using namespace QmlJSEditor::Internal;

static QStringList environmentImportPaths();

ModelManager::ModelManager(QObject *parent):
        ModelManagerInterface(parent),
        m_core(Core::ICore::instance())
{
    m_synchronizer.setCancelOnWait(true);

    qRegisterMetaType<QmlJS::Document::Ptr>("QmlJS::Document::Ptr");
    qRegisterMetaType<QmlJS::LibraryInfo>("QmlJS::LibraryInfo");

    connect(this, SIGNAL(documentUpdated(QmlJS::Document::Ptr)),
            this, SLOT(onDocumentUpdated(QmlJS::Document::Ptr)));
    connect(this, SIGNAL(libraryInfoUpdated(QString,QmlJS::LibraryInfo)),
            this, SLOT(onLibraryInfoUpdated(QString,QmlJS::LibraryInfo)));

    loadQmlTypeDescriptions();

    m_defaultImportPaths << environmentImportPaths();
    m_defaultImportPaths << QLibraryInfo::location(QLibraryInfo::ImportsPath);
}

void ModelManager::loadQmlTypeDescriptions()
{
    loadQmlTypeDescriptions(Core::ICore::instance()->resourcePath());
    loadQmlTypeDescriptions(Core::ICore::instance()->userResourcePath());
}

void ModelManager::loadQmlTypeDescriptions(const QString &resourcePath)
{
    const QDir typeFileDir(resourcePath + QLatin1String("/qml-type-descriptions"));
    const QStringList xmlExtensions = QStringList() << QLatin1String("*.xml");
    const QFileInfoList xmlFiles = typeFileDir.entryInfoList(xmlExtensions,
                                                             QDir::Files,
                                                             QDir::Name);

    const QStringList errors = Interpreter::CppQmlTypesLoader::load(xmlFiles);
    foreach (const QString &error, errors)
        qWarning() << qPrintable(error);

    // disabled for now: Prefer the xml file until the type dumping functionality
    // has been moved into Qt.
    // loads the builtin types
    //loadQmlPluginTypes(QString());
}

Snapshot ModelManager::snapshot() const
{
    QMutexLocker locker(&m_mutex);

    return _snapshot;
}

void ModelManager::updateSourceFiles(const QStringList &files,
                                     bool emitDocumentOnDiskChanged)
{
    refreshSourceFiles(files, emitDocumentOnDiskChanged);
}

QFuture<void> ModelManager::refreshSourceFiles(const QStringList &sourceFiles,
                                               bool emitDocumentOnDiskChanged)
{
    if (sourceFiles.isEmpty()) {
        return QFuture<void>();
    }

    const QMap<QString, WorkingCopy> workingCopy = buildWorkingCopyList();

    QFuture<void> result = QtConcurrent::run(&ModelManager::parse,
                                              workingCopy, sourceFiles,
                                              this,
                                              emitDocumentOnDiskChanged);

    if (m_synchronizer.futures().size() > 10) {
        QList<QFuture<void> > futures = m_synchronizer.futures();

        m_synchronizer.clearFutures();

        foreach (QFuture<void> future, futures) {
            if (! (future.isFinished() || future.isCanceled()))
                m_synchronizer.addFuture(future);
        }
    }

    m_synchronizer.addFuture(result);

    if (sourceFiles.count() > 1) {
        m_core->progressManager()->addTask(result, tr("Indexing"),
                        QmlJSEditor::Constants::TASK_INDEX);
    }

    return result;
}

QMap<QString, ModelManager::WorkingCopy> ModelManager::buildWorkingCopyList()
{
    QMap<QString, WorkingCopy> workingCopy;
    Core::EditorManager *editorManager = m_core->editorManager();

    foreach (Core::IEditor *editor, editorManager->openedEditors()) {
        const QString key = editor->file()->fileName();

        if (TextEditor::ITextEditor *textEditor = qobject_cast<TextEditor::ITextEditor*>(editor)) {
            if (QmlJSTextEditor *ed = qobject_cast<QmlJSTextEditor *>(textEditor->widget())) {
                workingCopy[key].contents = ed->toPlainText();
                workingCopy[key].documentRevision = ed->document()->revision();
            }
        }
    }

    return workingCopy;
}

void ModelManager::fileChangedOnDisk(const QString &path)
{
    QtConcurrent::run(&ModelManager::parse,
                      buildWorkingCopyList(), QStringList() << path,
                      this, true);
}

void ModelManager::removeFiles(const QStringList &files)
{
    emit aboutToRemoveFiles(files);

    QMutexLocker locker(&m_mutex);

    foreach (const QString &file, files)
        _snapshot.remove(file);
}

QList<ModelManager::ProjectInfo> ModelManager::projectInfos() const
{
    QMutexLocker locker(&m_mutex);

    return m_projects.values();
}

ModelManager::ProjectInfo ModelManager::projectInfo(ProjectExplorer::Project *project) const
{
    QMutexLocker locker(&m_mutex);

    return m_projects.value(project, ProjectInfo(project));
}

void ModelManager::updateProjectInfo(const ProjectInfo &pinfo)
{
    if (! pinfo.isValid())
        return;

    {
        QMutexLocker locker(&m_mutex);
        m_projects.insert(pinfo.project, pinfo);
    }

    updateImportPaths();
}

void ModelManager::emitDocumentChangedOnDisk(Document::Ptr doc)
{ emit documentChangedOnDisk(doc); }

void ModelManager::emitDocumentUpdated(Document::Ptr doc)
{ emit documentUpdated(doc); }

void ModelManager::onDocumentUpdated(Document::Ptr doc)
{
    QMutexLocker locker(&m_mutex);

    _snapshot.insert(doc);
}

void ModelManager::emitLibraryInfoUpdated(const QString &path, const LibraryInfo &info)
{ emit libraryInfoUpdated(path, info); }

void ModelManager::onLibraryInfoUpdated(const QString &path, const LibraryInfo &info)
{
    QMutexLocker locker(&m_mutex);

    _snapshot.insertLibraryInfo(path, info);
}

static QStringList qmlFilesInDirectory(const QString &path)
{
    // ### It would suffice to build pattern once. This function needs to be thread-safe.
    Core::MimeDatabase *db = Core::ICore::instance()->mimeDatabase();
    Core::MimeType jsSourceTy = db->findByType(QmlJSEditor::Constants::JS_MIMETYPE);
    Core::MimeType qmlSourceTy = db->findByType(QmlJSEditor::Constants::QML_MIMETYPE);

    QStringList pattern;
    foreach (const Core::MimeGlobPattern &glob, jsSourceTy.globPatterns())
        pattern << glob.regExp().pattern();
    foreach (const Core::MimeGlobPattern &glob, qmlSourceTy.globPatterns())
        pattern << glob.regExp().pattern();

    QStringList files;

    const QDir dir(path);
    foreach (const QFileInfo &fi, dir.entryInfoList(pattern, QDir::Files))
        files += fi.absoluteFilePath();

    return files;
}

static void findNewImplicitImports(const Document::Ptr &doc, const Snapshot &snapshot,
                            QStringList *importedFiles, QSet<QString> *scannedPaths)
{
    // scan files that could be implicitly imported
    // it's important we also do this for JS files, otherwise the isEmpty check will fail
    if (snapshot.documentsInDirectory(doc->path()).isEmpty()) {
        if (! scannedPaths->contains(doc->path())) {
            *importedFiles += qmlFilesInDirectory(doc->path());
            scannedPaths->insert(doc->path());
        }
    }
}

static void findNewFileImports(const Document::Ptr &doc, const Snapshot &snapshot,
                        QStringList *importedFiles, QSet<QString> *scannedPaths)
{
    // scan files and directories that are explicitly imported
    foreach (const Bind::ImportInfo &import, doc->bind()->imports()) {
        if (import.type == Bind::ImportInfo::FileImport) {
            if (! snapshot.document(import.name))
                *importedFiles += import.name;
        } else if (import.type == Bind::ImportInfo::DirectoryImport) {
            if (snapshot.documentsInDirectory(import.name).isEmpty()) {
                if (! scannedPaths->contains(import.name)) {
                    *importedFiles += qmlFilesInDirectory(import.name);
                    scannedPaths->insert(import.name);
                }
            }
        }
    }
}

static void findNewLibraryImports(const Document::Ptr &doc, const Snapshot &snapshot,
                           ModelManager *modelManager,
                           QStringList *importedFiles, QSet<QString> *scannedPaths)
{
    // scan library imports
    const QStringList importPaths = modelManager->importPaths();
    foreach (const Bind::ImportInfo &import, doc->bind()->imports()) {
        if (import.type != Bind::ImportInfo::LibraryImport)
            continue;
        foreach (const QString &importPath, importPaths) {
            QDir dir(importPath);
            dir.cd(import.name);
            const QString targetPath = dir.absolutePath();

            // if we know there is a library, done
            if (snapshot.libraryInfo(targetPath).isValid())
                break;

            // if there is a qmldir file, we found a new library!
            if (dir.exists("qmldir")) {
                QFile qmldirFile(dir.filePath("qmldir"));
                qmldirFile.open(QFile::ReadOnly);
                QString qmldirData = QString::fromUtf8(qmldirFile.readAll());

                QmlDirParser qmldirParser;
                qmldirParser.setSource(qmldirData);
                qmldirParser.parse();

                modelManager->emitLibraryInfoUpdated(QFileInfo(qmldirFile).absolutePath(),
                                                     LibraryInfo(qmldirParser));

                // scan the qml files in the library
                foreach (const QmlDirParser::Component &component, qmldirParser.components()) {
                    if (! component.fileName.isEmpty()) {
                        QFileInfo componentFileInfo(dir.filePath(component.fileName));
                        const QString path = componentFileInfo.absolutePath();
                        if (! scannedPaths->contains(path)) {
                            *importedFiles += qmlFilesInDirectory(path);
                            scannedPaths->insert(path);
                        }
                    }
                }
            }
        }
    }
}

void ModelManager::parse(QFutureInterface<void> &future,
                            QMap<QString, WorkingCopy> workingCopy,
                            QStringList files,
                            ModelManager *modelManager,
                            bool emitDocChangedOnDisk)
{
    Core::MimeDatabase *db = Core::ICore::instance()->mimeDatabase();
    Core::MimeType jsSourceTy = db->findByType(QLatin1String("application/javascript"));
    Core::MimeType qmlSourceTy = db->findByType(QLatin1String("application/x-qml"));

    int progressRange = files.size();
    future.setProgressRange(0, progressRange);

    Snapshot snapshot = modelManager->_snapshot;

    // paths we have scanned for files and added to the files list
    QSet<QString> scannedPaths;

    for (int i = 0; i < files.size(); ++i) {
        future.setProgressValue(qreal(i) / files.size() * progressRange);

        const QString fileName = files.at(i);

        const QFileInfo fileInfo(fileName);
        Core::MimeType fileMimeTy = db->findByFile(fileInfo);

        bool isQmlFile = true;

        if (matchesMimeType(fileMimeTy, jsSourceTy))
            isQmlFile = false;

        else if (! matchesMimeType(fileMimeTy, qmlSourceTy))
            continue; // skip it. it's not a QML or a JS file.

        QString contents;
        int documentRevision = 0;

        if (workingCopy.contains(fileName)) {
            WorkingCopy wc = workingCopy.value(fileName);
            contents = wc.contents;
            documentRevision = wc.documentRevision;
        } else {
            QFile inFile(fileName);

            if (inFile.open(QIODevice::ReadOnly)) {
                QTextStream ins(&inFile);
                contents = ins.readAll();
                inFile.close();
            }
        }

        Document::Ptr doc = Document::create(fileName);
        doc->setEditorRevision(documentRevision);
        doc->setSource(contents);
        doc->parse();

        // get list of referenced files not yet in snapshot or in directories already scanned
        QStringList importedFiles;
        findNewImplicitImports(doc, snapshot, &importedFiles, &scannedPaths);
        findNewFileImports(doc, snapshot, &importedFiles, &scannedPaths);
        findNewLibraryImports(doc, snapshot, modelManager, &importedFiles, &scannedPaths);

        // add new files to parse list
        foreach (const QString &file, importedFiles) {
            if (! files.contains(file))
                files.append(file);
        }

        modelManager->emitDocumentUpdated(doc);
        if (emitDocChangedOnDisk)
            modelManager->emitDocumentChangedOnDisk(doc);
    }

    future.setProgressValue(progressRange);
}

// Check whether fileMimeType is the same or extends knownMimeType
bool ModelManager::matchesMimeType(const Core::MimeType &fileMimeType, const Core::MimeType &knownMimeType)
{
    Core::MimeDatabase *db = Core::ICore::instance()->mimeDatabase();

    const QStringList knownTypeNames = QStringList(knownMimeType.type()) + knownMimeType.aliases();

    foreach (const QString knownTypeName, knownTypeNames)
        if (fileMimeType.matchesType(knownTypeName))
            return true;

    // recursion to parent types of fileMimeType
    foreach (const QString &parentMimeType, fileMimeType.subClassesOf()) {
        if (matchesMimeType(db->findByType(parentMimeType), knownMimeType))
            return true;
    }

    return false;
}

QStringList ModelManager::importPaths() const
{
    return m_allImportPaths;
}

static QStringList environmentImportPaths()
{
    QStringList paths;

    QByteArray envImportPath = qgetenv("QML_IMPORT_PATH");

#if defined(Q_OS_WIN)
    QLatin1Char pathSep(';');
#else
    QLatin1Char pathSep(':');
#endif
    foreach (const QString &path, QString::fromLatin1(envImportPath).split(pathSep, QString::SkipEmptyParts)) {
        QString canonicalPath = QDir(path).canonicalPath();
        if (!canonicalPath.isEmpty() && !paths.contains(canonicalPath))
            paths.append(canonicalPath);
    }

    return paths;
}

void ModelManager::loadPluginTypes(const QString &libraryPath, const QString &importPath, const QString &importUri)
{
    // make sure loading is always triggered in ModelManager's thread
    metaObject()->invokeMethod(this, "onLoadPluginTypes",
                               Q_ARG(QString, libraryPath),
                               Q_ARG(QString, importPath),
                               Q_ARG(QString, importUri));
}

void ModelManager::onLoadPluginTypes(const QString &libraryPath, const QString &importPath, const QString &importUri)
{
    const QString canonicalLibraryPath = QDir::cleanPath(libraryPath);
    if (m_runningQmldumps.values().contains(canonicalLibraryPath))
        return;

    static QString qmldumpPath;
    if (qmldumpPath.isNull()) {
        QDir qmldumpExecutable(QCoreApplication::applicationDirPath());
#ifdef Q_OS_WIN
        qmldumpPath = qmldumpExecutable.absoluteFilePath(QLatin1String("qmldump.exe"));
#else // !Q_OS_WIN
#  ifdef Q_OS_MAC
        qmldumpPath = qmldumpExecutable.absoluteFilePath(QLatin1String("qmldump.app/Contents/MacOS/qmldump"));
#  else // !Q_OS_MAC
        qmldumpPath = qmldumpExecutable.absoluteFilePath(QLatin1String("qmldump"));
#  endif // Q_OS_MAC
#endif // Q_OS_WIN
        QFileInfo qmldumpFileInfo(qmldumpPath);
        if (!qmldumpFileInfo.exists()) {
            qWarning() << "ModelManager::loadQmlPluginTypes: qmldump executable does not exist at" << qmldumpPath;
            qmldumpPath.clear();
        } else if (!qmldumpFileInfo.isFile()) {
            qWarning() << "ModelManager::loadQmlPluginTypes: " << qmldumpPath << " is not a file";
            qmldumpPath.clear();
        }

    }
    if (qmldumpPath.isEmpty())
        return;

    QProcess *process = new QProcess(this);
    connect(process, SIGNAL(finished(int)), SLOT(qmlPluginTypeDumpDone(int)));
    connect(process, SIGNAL(error(QProcess::ProcessError)), SLOT(qmlPluginTypeDumpError(QProcess::ProcessError)));
    QStringList args;
    args << importPath;
    args << importUri;
    process->start(qmldumpPath, args);
    m_runningQmldumps.insert(process, canonicalLibraryPath);
}

void ModelManager::updateImportPaths()
{
    m_allImportPaths.clear();
    QMapIterator<ProjectExplorer::Project *, ProjectInfo> it(m_projects);
    while (it.hasNext()) {
        it.next();
        m_allImportPaths += it.value().importPaths;
    }
    m_allImportPaths += m_defaultImportPaths;
    m_allImportPaths.removeDuplicates();

    // check if any file in the snapshot imports something new in the new paths
    Snapshot snapshot = _snapshot;
    QStringList importedFiles;
    QSet<QString> scannedPaths;
    foreach (const Document::Ptr &doc, snapshot)
        findNewLibraryImports(doc, snapshot, this, &importedFiles, &scannedPaths);

    updateSourceFiles(importedFiles, true);
}

static QString qmldumpErrorMessage(const QString &libraryPath, const QString &error)
{
    return ModelManager::tr("Type dump of QML plugin in %0 failed.\nErrors:\n%1\n").arg(libraryPath, error);
}

void ModelManager::qmlPluginTypeDumpDone(int exitCode)
{
    QProcess *process = qobject_cast<QProcess *>(sender());
    if (!process)
        return;
    process->deleteLater();

    const QString libraryPath = m_runningQmldumps.take(process);

    if (exitCode != 0) {
        Core::MessageManager *messageManager = Core::MessageManager::instance();
        messageManager->printToOutputPane(qmldumpErrorMessage(libraryPath, process->readAllStandardError()));
        return;
    }

    const QByteArray output = process->readAllStandardOutput();
    QMap<QString, Interpreter::FakeMetaObject *> newObjects;
    const QString error = Interpreter::CppQmlTypesLoader::parseQmlTypeXml(output, &newObjects);
    if (!error.isEmpty())
        return;

    // convert from QList<T *> to QList<const T *>
    QList<const Interpreter::FakeMetaObject *> objectsList;
    QMapIterator<QString, Interpreter::FakeMetaObject *> it(newObjects);
    while (it.hasNext()) {
        it.next();
        objectsList.append(it.value());
    }

    QMutexLocker locker(&m_mutex);

    if (!libraryPath.isEmpty()) {
        LibraryInfo libraryInfo = _snapshot.libraryInfo(libraryPath);
        libraryInfo.setMetaObjects(objectsList);
        _snapshot.insertLibraryInfo(libraryPath, libraryInfo);
    } else {
        Interpreter::CppQmlTypesLoader::builtinObjects.append(objectsList);
    }
}

void ModelManager::qmlPluginTypeDumpError(QProcess::ProcessError)
{
    QProcess *process = qobject_cast<QProcess *>(sender());
    if (!process)
        return;
    process->deleteLater();

    const QString libraryPath = m_runningQmldumps.take(process);

    Core::MessageManager *messageManager = Core::MessageManager::instance();
    messageManager->printToOutputPane(qmldumpErrorMessage(libraryPath, process->readAllStandardError()));
}
