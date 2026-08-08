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

#include "core/Banking/Account/Account.h"
#include "ui/Assistant/Pages/OptionBankingPage.h"

#include "TestHelpers.h"

#include <QtWidgets/QTreeWidget>

#include <QtTest/QtTest>

using namespace olbaflinx::core::banking;
using namespace olbaflinx::core::banking::account;
using namespace olbaflinx::ui::assistant::pages;

namespace olbaflinx::ui::assistant::tests {

using namespace olbaflinx::core::tests;

/**
 * What the wizard hands over once the user is through with it. The page is
 * driven directly rather than through a run of the wizard: the wizard sets its
 * banking page up in its constructor, and that page opens a connection to the
 * institution, which no test may depend on.
 *
 * setAccounts is the same entry the backend uses. initialize connects
 * Banking::itemsReceived to it, so what this test drives is the path the
 * application drives.
 */
class SetupAssistantTest final : public QObject
{
    Q_OBJECT

private:
    /**
     * The tree the page fills. Reached by the name uic gives it, because the
     * page keeps its form to itself.
     */
    static QTreeWidget *treeOf(const OptionBankingPage &page)
    {
        return page.findChild<QTreeWidget *>(QStringLiteral("treeWidgetAccounts"));
    }

    static void selectByUniqueId(const OptionBankingPage &page, quint32 uniqueId)
    {
        QTreeWidget *const tree = treeOf(page);
        QVERIFY(tree != nullptr);

        for (int row = 0; row < tree->topLevelItemCount(); ++row) {
            QTreeWidgetItem *const item = tree->topLevelItem(row);
            if (item->data(0, Qt::UserRole).toUInt() == uniqueId) {
                item->setSelected(true);
                return;
            }
        }

        QFAIL("no entry carries that unique id");
    }

private Q_SLOTS:
    void aPageNobodyFilledOffersNothing();
    void theOfferedAccountsAreTheOnesHandedOver();
    void onlyTheChosenAccountsComeBack();
    void anAccountThatIsNoLongerOfferedDropsOutOfBothLists();
};

/**
 * The state before the backend has reported anything, and the state after a
 * connection that failed. Neither list may invent an account.
 */
void SetupAssistantTest::aPageNobodyFilledOffersNothing()
{
    const OptionBankingPage page;

    QVERIFY(page.offeredAccounts().isEmpty());
    QVERIFY(page.selectedAccounts().isEmpty());
    QVERIFY(page.selectedAccountIds().isEmpty());
    QVERIFY(!page.isComplete());
}

void SetupAssistantTest::theOfferedAccountsAreTheOnesHandedOver()
{
    OptionBankingPage page;

    auto offered = BankingItems();
    for (int i = 0; i < 3; ++i) {
        offered << TestHelpers::createFakeAccount();
    }

    page.setAccounts(offered);

    QCOMPARE(page.offeredAccounts().size(), 3);
    QCOMPARE(treeOf(page)->topLevelItemCount(), 3);

    // Nothing is chosen until the user chooses. An account on offer is not an
    // account the user keeps.
    QVERIFY(page.selectedAccounts().isEmpty());
}

/**
 * The heart of it. Whoever stores the result has to be able to tell three groups
 * apart, and two of them come from here.
 */
void SetupAssistantTest::onlyTheChosenAccountsComeBack()
{
    OptionBankingPage page;

    auto offered = BankingItems();
    for (int i = 0; i < 3; ++i) {
        offered << TestHelpers::createFakeAccount();
    }

    page.setAccounts(offered);

    const auto chosen = std::dynamic_pointer_cast<Account>(offered.at(1));
    QVERIFY(chosen != nullptr);

    selectByUniqueId(page, chosen->uniqueId());

    const auto selected = page.selectedAccounts();
    QCOMPARE(selected.size(), 1);

    const auto readBack = std::dynamic_pointer_cast<Account>(selected.at(0));
    QVERIFY(readBack != nullptr);
    QCOMPARE(readBack->uniqueId(), chosen->uniqueId());

    // The account itself, not a copy built from its id. Its properties are what
    // the store is about to be given.
    QCOMPARE(readBack->accountName(), chosen->accountName());
    QCOMPARE(readBack->iban(), chosen->iban());

    QCOMPARE(page.offeredAccounts().size(), 3);
}

/**
 * A second run of the backend reports what it finds then. An account it no
 * longer reports is not turned down, it was not on offer, and the lists have to
 * say so. What is already in the store stays untouched.
 */
void SetupAssistantTest::anAccountThatIsNoLongerOfferedDropsOutOfBothLists()
{
    OptionBankingPage page;

    const auto first = TestHelpers::createFakeAccount();
    const auto second = TestHelpers::createFakeAccount();

    page.setAccounts(BankingItems{first, second});
    selectByUniqueId(page, first->uniqueId());
    QCOMPARE(page.selectedAccounts().size(), 1);

    page.setAccounts(BankingItems{second});

    QCOMPARE(page.offeredAccounts().size(), 1);
    QVERIFY(page.selectedAccounts().isEmpty());

    const auto stillOffered = std::dynamic_pointer_cast<Account>(page.offeredAccounts().at(0));
    QVERIFY(stillOffered != nullptr);
    QCOMPARE(stillOffered->uniqueId(), second->uniqueId());
}

} // namespace olbaflinx::ui::assistant::tests

QTEST_MAIN(olbaflinx::ui::assistant::tests::SetupAssistantTest)

#include "tst_setupassistant.moc"
