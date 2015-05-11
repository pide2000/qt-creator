/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://www.qt.io/licensing.  For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/


#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"

#include <QString>
#include <QBuffer>
#include <QSignalSpy>
#include <vector>
#include <QVariant>

#include "gtest-qt-printing.h"

#include <cmbendcommand.h>
#include <cmbalivecommand.h>
#include <cmbcommands.h>
#include <cmbechocommand.h>
#include <cmbregistertranslationunitsforcodecompletioncommand.h>
#include <cmbunregistertranslationunitsforcodecompletioncommand.h>
#include <cmbregisterprojectsforcodecompletioncommand.h>
#include <cmbunregisterprojectsforcodecompletioncommand.h>
#include <cmbcodecompletedcommand.h>
#include <cmbcompletecodecommand.h>
#include <writecommandblock.h>
#include <readcommandblock.h>
#include <connectionclient.h>
#include <translationunitdoesnotexistcommand.h>
#include <projectpartsdonotexistcommand.h>

#include <mockipclient.h>

using namespace CodeModelBackEnd;

using ::testing::Eq;

class ClientServerOutsideProcess : public ::testing::Test
{
protected:
    void SetUp();
    void TearDown();

    static void SetUpTestCase();
    static void TearDownTestCase();

    static MockIpcClient mockIpcClient;
    static CodeModelBackEnd::ConnectionClient client;
};

MockIpcClient ClientServerOutsideProcess::mockIpcClient;
CodeModelBackEnd::ConnectionClient ClientServerOutsideProcess::client(&ClientServerOutsideProcess::mockIpcClient);

TEST_F(ClientServerOutsideProcess, RestartProcess)
{
    client.restartProcess();

    ASSERT_TRUE(client.isProcessIsRunning());
    ASSERT_TRUE(client.isConnected());
}

TEST_F(ClientServerOutsideProcess, ResetAliveTimer)
{
    QSignalSpy clientSpy(&client, SIGNAL(processRestarted()));

    client.setProcessAliveTimerInterval(1);

    ASSERT_TRUE(clientSpy.wait(100000));
}

TEST_F(ClientServerOutsideProcess, SendRegisterTranslationUnitForCodeCompletionCommand)
{
    CodeModelBackEnd::FileContainer fileContainer(Utf8StringLiteral("foo"), Utf8StringLiteral("pathToProjectPart.pro"));
    CodeModelBackEnd::RegisterTranslationUnitForCodeCompletionCommand registerTranslationUnitForCodeCompletionCommand({fileContainer});
    EchoCommand echoCommand(QVariant::fromValue(registerTranslationUnitForCodeCompletionCommand));

    EXPECT_CALL(mockIpcClient, echo(echoCommand))
            .Times(1);

    client.serverProxy().registerTranslationUnitsForCodeCompletion(registerTranslationUnitForCodeCompletionCommand);
    ASSERT_TRUE(client.waitForEcho());
}

TEST_F(ClientServerOutsideProcess, SendUnregisterTranslationUnitsForCodeCompletionCommand)
{
    FileContainer fileContainer(Utf8StringLiteral("foo.cpp"), Utf8StringLiteral("bar.pro"));
    CodeModelBackEnd::UnregisterTranslationUnitsForCodeCompletionCommand unregisterTranslationUnitsForCodeCompletionCommand ({fileContainer});
    EchoCommand echoCommand(QVariant::fromValue(unregisterTranslationUnitsForCodeCompletionCommand));

    EXPECT_CALL(mockIpcClient, echo(echoCommand))
            .Times(1);

    client.serverProxy().unregisterTranslationUnitsForCodeCompletion(unregisterTranslationUnitsForCodeCompletionCommand);
    ASSERT_TRUE(client.waitForEcho());
}

TEST_F(ClientServerOutsideProcess, SendCompleteCodeCommand)
{
    CompleteCodeCommand codeCompleteCommand(Utf8StringLiteral("foo.cpp"), 24, 33, Utf8StringLiteral("do what I want"));
    EchoCommand echoCommand(QVariant::fromValue(codeCompleteCommand));

    EXPECT_CALL(mockIpcClient, echo(echoCommand))
            .Times(1);

    client.serverProxy().completeCode(codeCompleteCommand);
    ASSERT_TRUE(client.waitForEcho());
}

TEST_F(ClientServerOutsideProcess, SendRegisterProjectPartsForCodeCompletionCommand)
{
    CodeModelBackEnd::ProjectPartContainer projectContainer(Utf8StringLiteral("data/complete.pro"));
    CodeModelBackEnd::RegisterProjectPartsForCodeCompletionCommand registerProjectPartsForCodeCompletionCommand({projectContainer});
    EchoCommand echoCommand(QVariant::fromValue(registerProjectPartsForCodeCompletionCommand));

    EXPECT_CALL(mockIpcClient, echo(echoCommand))
            .Times(1);

    client.serverProxy().registerProjectPartsForCodeCompletion(registerProjectPartsForCodeCompletionCommand);
    ASSERT_TRUE(client.waitForEcho());
}

TEST_F(ClientServerOutsideProcess, SendUnregisterProjectPartsForCodeCompletionCommand)
{
    CodeModelBackEnd::UnregisterProjectPartsForCodeCompletionCommand unregisterProjectPartsForCodeCompletionCommand({Utf8StringLiteral("data/complete.pro")});
    EchoCommand echoCommand(QVariant::fromValue(unregisterProjectPartsForCodeCompletionCommand));

    EXPECT_CALL(mockIpcClient, echo(echoCommand))
            .Times(1);

    client.serverProxy().unregisterProjectPartsForCodeCompletion(unregisterProjectPartsForCodeCompletionCommand);
    ASSERT_TRUE(client.waitForEcho());
}

void ClientServerOutsideProcess::SetUpTestCase()
{
    client.setProcessPath(QStringLiteral("../echoserver/echoserver"));

    ASSERT_TRUE(client.connectToServer());
}

void ClientServerOutsideProcess::TearDownTestCase()
{
    client.finishProcess();
}
void ClientServerOutsideProcess::SetUp()
{
    client.setProcessAliveTimerInterval(1000000);

    ASSERT_TRUE(client.isConnected());
}

void ClientServerOutsideProcess::TearDown()
{
    ASSERT_TRUE(client.isProcessIsRunning());
    ASSERT_TRUE(client.isConnected());
}
