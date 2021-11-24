/**
 * Copyright (c) 2021, Alexander Saal <alexander.saal@chm-projects.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtWidgets/QLayout>

#include "core/Banking/Banking.h"

#include "OptionPage.h"

using namespace olbaflinx::core::banking;
using namespace olbaflinx::app::banking::assistant::page;

QTimer *accountTimer = nullptr;
QThread *accountTimerThread = nullptr;
bool isOptionPageComplete = false;

OptionPage::OptionPage(QWidget *parent)
    : QWizardPage(parent)
{}

OptionPage::~OptionPage()
{
    delete accountTimer;

    accountTimerThread->quit();
    accountTimerThread->wait();
    accountTimerThread->deleteLater();
}

void OptionPage::initialize()
{
    connect(Banking::instance(), &Banking::accountIdsReceived, this, &OptionPage::checkForAccounts);

    auto configWidget = Banking::instance()->createSetupDialog(this);

    layout()->setContentsMargins(QMargins(0, 0, 0, 0));
    layout()->addWidget(configWidget);

    accountTimer = new QTimer(nullptr);
    accountTimer->setInterval(750);
    connect(accountTimer, &QTimer::timeout, Banking::instance(), &Banking::receiveAccountIds);

    accountTimerThread = new QThread(this);
    accountTimer->moveToThread(accountTimerThread);

    connect(accountTimerThread, &QThread::finished, accountTimer, &QTimer::stop);
    connect(accountTimerThread, &QThread::started, accountTimer, qOverload<>(&QTimer::start));

    accountTimerThread->start();
}

bool OptionPage::isComplete() const
{
    return isOptionPageComplete;
}

void OptionPage::checkForAccounts(const AccountIds &accountIds)
{
    isOptionPageComplete = !accountIds.empty();
    if (isOptionPageComplete) {
        accountTimerThread->quit();
        accountTimerThread->wait();
        accountTimer->stop();
        Q_EMIT completeChanged();
    }
}

