/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2015 Travis Nickles <nickles.travis@gmail.com>
 * Copyright (C) 2020 Jagoda Górska <juliagoda.pl@protonmail>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "localantimicroserver.h"

#include "common.h"

#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>

LocalAntiMicroServer::LocalAntiMicroServer(QObject *parent)
    : QObject(parent)
{
    localServer = new QLocalServer(this);
}

bool LocalAntiMicroServer::startLocalServer()
{
    if (localServer == nullptr)
    {
        qCritical() << "LocalAntiMicroXServer::startLocalServer(): localServer is nullptr";
        return false;
    }

    if (localServer->isListening())
        return true;

    const bool removedServer = QLocalServer::removeServer(PadderCommon::localSocketKey);

    if (!removedServer)
        qDebug() << "Couldn't remove local server named " << PadderCommon::localSocketKey;

    if (localServer->maxPendingConnections() != 1)
        localServer->setMaxPendingConnections(1);

#if defined(Q_OS_WIN)
    // Allow the same Windows user to reach an elevated server from a non-elevated process.
    // This must be configured before listen() creates the named pipe.
    localServer->setSocketOptions(QLocalServer::UserAccessOption);
#endif

    connect(localServer, &QLocalServer::newConnection, this, &LocalAntiMicroServer::handleOutsideConnection,
            Qt::UniqueConnection);

    if (!localServer->listen(PadderCommon::localSocketKey))
    {
        const QString message = tr("Could not start the signal server. AntiMicroX-Delta will exit to avoid processing "
                                   "input in more than one instance.");
        PRINT_STDERR() << message << "\n";
        qCritical() << message << "Server error:" << localServer->serverError()
                    << "Error text:" << localServer->errorString();
        return false;
    }

    return true;
}

void LocalAntiMicroServer::handleOutsideConnection()
{
    if (localServer != nullptr)
    {
        QLocalSocket *socket = localServer->nextPendingConnection();

        if (socket != nullptr)
        {
            qDebug() << "There is next pending connection: " << socket->socketDescriptor();
            connect(socket, &QLocalSocket::disconnected, this, &LocalAntiMicroServer::handleSocketDisconnect);
            connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
            checkForMessages(socket);
        } else
        {
            qDebug() << "There isn't next pending connection: ";
        }
    } else
    {
        qDebug() << "LocalAntiMicroXServer::handleOutsideConnection(): localServer is nullptr";
    }
}

void LocalAntiMicroServer::handleSocketDisconnect() { emit clientdisconnect(); }

void LocalAntiMicroServer::close() { localServer->close(); }

void LocalAntiMicroServer::checkForMessages(QLocalSocket *socket)
{
    DEBUG() << "Waiting for message";
    socket->waitForConnected(50);
    bool result = socket->waitForReadyRead(200);
    DEBUG() << "Waiting for message ended with result: " << (result ? "true" : "false");
    if (result)
    {
        QString msg = QString(socket->readLine(30));
        DEBUG() << "Received external message:" << msg;
        if (msg == PadderCommon::unhideCommand)
        {
            DEBUG() << "Showing hidden window because of external request";
            emit showHiddenWindow();
        }
    }
}

QLocalServer *LocalAntiMicroServer::getLocalServer() const { return localServer; }
