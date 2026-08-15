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
#include "ui/BankingGui.h"

#include "TestHelpers.h"

#include <gwenhywfar/db.h>
#include <gwenhywfar/dialog.h>
#include <gwenhywfar/gui.h>

#include <QtConcurrent/QtConcurrentRun>

#include <QtCore/QFuture>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>

#include <QtTest/QtTest>

#include <memory>

using namespace olbaflinx::core;
using namespace olbaflinx::core::banking;

namespace olbaflinx::ui::tests {

using namespace olbaflinx::core::tests;

namespace {

constexpr auto passwordName = "probe-token";
constexpr auto shortLifetimeMs = 50;

/**
 * Makes the forwarded callbacks reachable for a test. It changes nothing about
 * them: the point of the test is what BankingGui does with them, so overriding
 * one here would measure the test instead of the class.
 */
class ProbeGui final : public BankingGui
{
public:
    using BankingGui::BankingGui;

    using BankingGui::closeDialog;
    using BankingGui::openDialog;
};

/** A dialog of the backend, with no widget in it. */
using DialogPtr = std::unique_ptr<GWEN_DIALOG, decltype(&GWEN_Dialog_free)>;

DialogPtr emptyDialog()
{
    return {GWEN_Dialog_new("probe"), &GWEN_Dialog_free};
}

} // namespace

class BankingGuiTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> bankingHome;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void aCallFromAWorkerThreadWaitsForTheOwningThread();
    void aCallFromTheOwningThreadAnswersOnTheSpotAndDoesNotDeadlock();
    void aProgressFromAWorkerThreadWaitsForTheOwningThread();
    void aProgressFromTheOwningThreadAnswersOnTheSpotAndDoesNotDeadlock();
    void theInterfaceAndTheBackendComeUpAndGoDownRepeatedly();

    void theCachedCredentialIsGoneWhenTheSpanHasRun();
    void aFetchWithinTheSpanKeepsTheCachedCredential();
};

/**
 * AqBanking keeps its configuration below AQBANKING_HOME. Without pointing that
 * at a directory of our own, every run would write into the configuration of
 * whoever started it.
 */
void BankingGuiTest::initTestCase()
{
    bankingHome = std::make_unique<QTemporaryDir>();
    QVERIFY(bankingHome->isValid());

    QVERIFY(qputenv("AQBANKING_HOME", bankingHome->path().toUtf8()));
}

void BankingGuiTest::cleanupTestCase()
{
    qunsetenv("AQBANKING_HOME");
    bankingHome.reset();
}

/**
 * The promise is that no widget is touched from the thread of a session. What
 * shows it here is that the call cannot finish while the owning thread does
 * nothing: it is waiting for exactly that thread to take the work.
 */
void BankingGuiTest::aCallFromAWorkerThreadWaitsForTheOwningThread()
{
    ProbeGui gui;
    const DialogPtr dialog = emptyDialog();

    QThread *ownerThread = QThread::currentThread();
    QThread *callThread = nullptr;

    QFuture<void> handed = QtConcurrent::run([&gui, &dialog, &callThread] {
        callThread = QThread::currentThread();
        gui.openDialog(dialog.get(), 0);
    });

    // Long enough for the run to have reached the call, short enough not to
    // stretch the suite. Nothing here waits for the answer, which is the point.
    QThread::msleep(100);
    QVERIFY(!handed.isFinished());

    // Only now does the owning thread serve its queue, and only now can the call
    // come through.
    QTRY_VERIFY_WITH_TIMEOUT(handed.isFinished(), 5000);

    QVERIFY(callThread != nullptr);
    QVERIFY(callThread != ownerThread);
}

/**
 * The wizard reaches the same interface from the thread that owns it. Handing
 * that over would wait for a thread that is waiting for itself.
 */
void BankingGuiTest::aCallFromTheOwningThreadAnswersOnTheSpotAndDoesNotDeadlock()
{
    ProbeGui gui;
    const DialogPtr dialog = emptyDialog();

    // No event loop runs during this call. A forwarded one would never come
    // back, and the test would time out instead of finishing.
    gui.openDialog(dialog.get(), 0);

    QVERIFY(true);
}

/**
 * The progress of a session is the second way into the widgets, next to the
 * dialogs, and the one a fetch takes on every run: the backend reports its
 * course from the thread of the session, and the report reaches the log of the
 * progress dialog. Measured the same way as the dialogs above, by a call that
 * cannot finish while the owning thread does nothing.
 *
 * The delay flag keeps the dialog from being shown, so what is left to measure
 * is the handover alone.
 */
void BankingGuiTest::aProgressFromAWorkerThreadWaitsForTheOwningThread()
{
    ProbeGui gui;

    QThread *ownerThread = QThread::currentThread();
    QThread *callThread = nullptr;

    QFuture<void> handed = QtConcurrent::run([&gui, &callThread] {
        callThread = QThread::currentThread();

        // The interface of gwenhywfar lives per thread, and a session sets it
        // in its own the same way before it reports anything.
        GWEN_Gui_SetGui(gui.getCInterface());

        const uint32_t progress = GWEN_Gui_ProgressStart(GWEN_GUI_PROGRESS_DELAY,
                                                         "probe",
                                                         "probe",
                                                         1,
                                                         0);
        GWEN_Gui_ProgressLog(progress, GWEN_LoggerLevel_Info, "probe");
        GWEN_Gui_ProgressEnd(progress);

        GWEN_Gui_SetGui(nullptr);
    });

    QThread::msleep(100);
    QVERIFY(!handed.isFinished());

    QTRY_VERIFY_WITH_TIMEOUT(handed.isFinished(), 5000);

    QVERIFY(callThread != nullptr);
    QVERIFY(callThread != ownerThread);
}

/**
 * The counterpart of the dialog case: a progress reported from the thread that
 * owns the interface is answered on the spot. Handing it over would wait for a
 * thread that is waiting for itself.
 */
void BankingGuiTest::aProgressFromTheOwningThreadAnswersOnTheSpotAndDoesNotDeadlock()
{
    ProbeGui gui;

    GWEN_Gui_SetGui(gui.getCInterface());

    // No event loop runs during these calls. A forwarded one would never come
    // back, and the test would time out instead of finishing.
    const uint32_t progress = GWEN_Gui_ProgressStart(GWEN_GUI_PROGRESS_DELAY, "probe", "probe", 1, 0);
    GWEN_Gui_ProgressLog(progress, GWEN_LoggerLevel_Info, "probe");
    GWEN_Gui_ProgressEnd(progress);

    GWEN_Gui_SetGui(nullptr);

    QVERIFY(true);
}

/**
 * The order of the shutdown is not checked by any compiler and crashes rather
 * than reports when it is wrong: the backend reaches into the interface while it
 * goes down. Repeating it is what makes a leak show up under a sanitizer.
 */
void BankingGuiTest::theInterfaceAndTheBackendComeUpAndGoDownRepeatedly()
{
    const auto info = TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxBankingGuiTest"));

    for (int round = 0; round < 3; ++round) {
        auto gui = std::make_unique<ProbeGui>();
        auto banking = std::make_unique<Banking>(info);

        const Error error = banking->initialize(info.name,
                                                info.version,
                                                QStringLiteral("probe-key"),
                                                gui->getCInterface());
        QVERIFY2(!error.isError(), qPrintable(error.message()));

        // The backend first, the interface after it.
        banking.reset();
        gui.reset();
    }
}

/**
 * gwenhywfar caches a PIN and never empties that cache by itself. The interface
 * owns it, so the interface is what lets it go.
 */
void BankingGuiTest::theCachedCredentialIsGoneWhenTheSpanHasRun()
{
    ProbeGui gui(shortLifetimeMs);

    GWEN_DB_NODE *cache = GWEN_Gui_GetPasswordDb(gui.getCInterface());
    QVERIFY(cache != nullptr);

    GWEN_DB_SetCharValue(cache, GWEN_DB_FLAGS_OVERWRITE_VARS, passwordName, "1234");
    QVERIFY(GWEN_DB_GetCharValue(cache, passwordName, 0, nullptr) != nullptr);

    gui.expirePasswordCacheLater();
    QVERIFY(gui.isPasswordCacheExpiring());

    QTRY_VERIFY_WITH_TIMEOUT(GWEN_DB_GetCharValue(cache, passwordName, 0, nullptr) == nullptr, 5000);
    QVERIFY(!gui.isPasswordCacheExpiring());
}

/**
 * A fetch that starts while the span is still running finds the PIN and asks
 * for nothing, and the span begins anew when that fetch has ended.
 */
void BankingGuiTest::aFetchWithinTheSpanKeepsTheCachedCredential()
{
    ProbeGui gui(shortLifetimeMs);

    GWEN_DB_NODE *cache = GWEN_Gui_GetPasswordDb(gui.getCInterface());
    QVERIFY(cache != nullptr);

    GWEN_DB_SetCharValue(cache, GWEN_DB_FLAGS_OVERWRITE_VARS, passwordName, "1234");

    gui.expirePasswordCacheLater();
    gui.holdPasswordCache();
    QVERIFY(!gui.isPasswordCacheExpiring());

    // Well past the span. Without the hold the cache would be empty by now.
    QTest::qWait(shortLifetimeMs * 4);
    QVERIFY(GWEN_DB_GetCharValue(cache, passwordName, 0, nullptr) != nullptr);

    // The fetch has ended, and the span starts over.
    gui.expirePasswordCacheLater();
    QTRY_VERIFY_WITH_TIMEOUT(GWEN_DB_GetCharValue(cache, passwordName, 0, nullptr) == nullptr, 5000);
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::BankingGuiTest)

#include "tst_bankinggui.moc"
