/**
 * Copyright (C) 2021-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "core/ApplicationInfo.h"
#include "core/Banking/Banking.h"
#include "core/Error.h"

#include <QtTest/QtTest>

#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;

namespace olbaflinx::core::banking::tests {

class BankingTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> bankingHome;

    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxBankingTest"),
                QStringLiteral("1.0.0")};
    }

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void bankingIsConstructibleWithoutAnyApplicationInstance();
    void accountsWithoutInitializationReportsAnError();
    void initializeOpensTheBackendUnderTheTemporaryHome();
    void accountsWithoutAnyConfiguredAccountReportsNotFound();
    void initializeWithoutAnInterfaceReportsAnError();
};

namespace {

/**
 * The non-interactive interface of gwenhywfar. It answers no prompt and shows no
 * dialog, which is all a headless test needs, and it carries no Qt: that is the
 * point of handing the interface in from outside instead of letting the core
 * build a Qt one.
 *
 * Ownership stays here. Banking takes the pointer and never frees it.
 */
class ScopedConsoleGui
{
public:
    ScopedConsoleGui()
        : m_gui(GWEN_Gui_new())
    {}

    ~ScopedConsoleGui() { GWEN_Gui_free(m_gui); }

    ScopedConsoleGui(const ScopedConsoleGui &) = delete;
    ScopedConsoleGui &operator=(const ScopedConsoleGui &) = delete;

    [[nodiscard]] GWEN_GUI *get() const { return m_gui; }

private:
    GWEN_GUI *m_gui;
};

} // namespace

/**
 * AqBanking keeps its configuration below AQBANKING_HOME. Without pointing that
 * at a directory of our own, every run would write into the configuration of
 * whoever started it.
 */
void BankingTest::initTestCase()
{
    bankingHome = std::make_unique<QTemporaryDir>();
    QVERIFY(bankingHome->isValid());

    QVERIFY(qputenv("AQBANKING_HOME", bankingHome->path().toUtf8()));
}

void BankingTest::cleanupTestCase()
{
    qunsetenv("AQBANKING_HOME");
    bankingHome.reset();
}

/**
 * Shows that Banking takes name and version from ApplicationInfo. This target
 * uses QTEST_APPLESS_MAIN, so there is no application instance the values could
 * otherwise come from.
 */
void BankingTest::bankingIsConstructibleWithoutAnyApplicationInstance()
{
    QVERIFY(QCoreApplication::instance() == nullptr);

    Banking banking(applicationInfo());

    QVERIFY(banking.parent() == nullptr);
}

/**
 * The failure case: without the backend being set up, accounts() must report no
 * accounts and pass the error on instead.
 */
void BankingTest::accountsWithoutInitializationReportsAnError()
{
    Banking banking(applicationInfo());

    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy itemsSpy(&banking, &Banking::itemsReceived);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    banking.accounts();

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);

    QCOMPARE(errorSpy.takeFirst().at(0).value<ErrorCode>(), ErrorCode::BankingFailure);
}

void BankingTest::initializeOpensTheBackendUnderTheTemporaryHome()
{
    Banking banking(applicationInfo());

    const ScopedConsoleGui gui;

    const auto error = banking.initialize(QStringLiteral("OlbaFlinxBankingTest"),
                                          QStringLiteral("1.0.0"),
                                          QStringLiteral("0123456789ABCDEF"),
                                          gui.get());

    QVERIFY2(!error.isError(), qPrintable(error.message()));

    banking.finalize();

    // The backend writes its configuration where AQBANKING_HOME points, and
    // nowhere else.
    QVERIFY(!QDir(bankingHome->path()).isEmpty());
}

/**
 * A fresh home carries no account. AqBanking answers that with
 * GWEN_ERROR_NOT_FOUND, which is -51 and therefore not AB_SUCCESS. Every value
 * other than AB_SUCCESS used to become BankingFailure, so a user who had not set
 * up an account yet was told their banking backend was broken.
 */
void BankingTest::accountsWithoutAnyConfiguredAccountReportsNotFound()
{
    Banking banking(applicationInfo());

    const ScopedConsoleGui gui;

    QVERIFY(!banking
                 .initialize(QStringLiteral("OlbaFlinxBankingTest"),
                             QStringLiteral("1.0.0"),
                             QStringLiteral("0123456789ABCDEF"),
                             gui.get())
                 .isError());

    QSignalSpy errorSpy(&banking, &Banking::errorOccurred);
    QSignalSpy itemsSpy(&banking, &Banking::itemsReceived);
    QSignalSpy finishedSpy(&banking, &Banking::finished);

    banking.accounts();

    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    const auto arguments = errorSpy.takeFirst();
    QCOMPARE(arguments.at(0).value<ErrorCode>(), ErrorCode::NotFound);

    // The raw return value of the backend no longer belongs in the message. The
    // code carries what happened, and -51 tells a user nothing.
    QVERIFY(arguments.at(1).toString().contains(QStringLiteral("No accounts")));

    banking.finalize();
}

/**
 * The failure case for the interface. A missing one is refused here, where the
 * caller can be told about it, and not left to gwenhywfar. gwenhywfar asserts on
 * it the moment it takes a file lock, which AB_Banking_Fini does, so the process
 * aborted inside finalize with a message about gui.c and nothing about the cause.
 */
void BankingTest::initializeWithoutAnInterfaceReportsAnError()
{
    Banking banking(applicationInfo());

    const auto error = banking.initialize(QStringLiteral("OlbaFlinxBankingTest"),
                                          QStringLiteral("1.0.0"),
                                          QStringLiteral("0123456789ABCDEF"),
                                          nullptr);

    QVERIFY(error.isError());
    QCOMPARE(error.code(), ErrorCode::InvalidInput);
    QVERIFY(error.message().contains(QStringLiteral("user interface")));
}

} // namespace olbaflinx::core::banking::tests

QTEST_APPLESS_MAIN(olbaflinx::core::banking::tests::BankingTest)

#include "tst_banking.moc"
