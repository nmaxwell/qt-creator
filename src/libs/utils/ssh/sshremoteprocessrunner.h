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

#ifndef SSHREMOTEPROCESSRUNNER_H
#define SSHREMOTEPROCESSRUNNER_H

#include "sshconnection.h"
#include "sshremoteprocess.h"

namespace Utils {
namespace Internal {
class SshRemoteProcessRunnerPrivate;
} // namespace Internal

class QTCREATOR_UTILS_EXPORT SshRemoteProcessRunner : public QObject
{
    Q_OBJECT

public:
    SshRemoteProcessRunner(QObject *parent = 0);
    ~SshRemoteProcessRunner();

    void run(const QByteArray &command, const SshConnectionParameters &sshParams);
    void runInTerminal(const QByteArray &command, const SshPseudoTerminal &terminal,
        const SshConnectionParameters &sshParams);
    QByteArray command() const;

    Utils::SshError lastConnectionError() const;
    QString lastConnectionErrorString() const;

    bool isProcessRunning() const;
    void writeDataToProcess(const QByteArray &data);
    void sendSignalToProcess(SshRemoteProcess::Signal signal); // No effect with OpenSSH server.
    void cancel(); // Does not stop remote process, just frees SSH-related process resources.
    SshRemoteProcess::ExitStatus processExitStatus() const;
    SshRemoteProcess::Signal processExitSignal() const;
    int processExitCode() const;
    QString processErrorString() const;

private slots:
    void handleConnected();
    void handleConnectionError(Utils::SshError error);
    void handleDisconnected();
    void handleProcessStarted();
    void handleProcessFinished(int exitStatus);
    void handleStdout();
    void handleStderr();

signals:
    void connectionError();
    void processStarted();
    void processOutputAvailable(const QByteArray &output);
    void processErrorOutputAvailable(const QByteArray &output);
    void processClosed(int exitStatus); // values are of type SshRemoteProcess::ExitStatus

private:
    void runInternal(const QByteArray &command, const Utils::SshConnectionParameters &sshParams);
    void setState(int newState);

    Internal::SshRemoteProcessRunnerPrivate * const d;
};

} // namespace Utils

#endif // SSHREMOTEPROCESSRUNNER_H
