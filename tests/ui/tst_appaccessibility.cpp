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

#include "ui/AppCentralWidget.h"

#include <QtTest/QtTest>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>

#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>

using namespace olbaflinx::ui;

namespace olbaflinx::ui::tests {

/**
 * A view says nothing about itself to an assistive tool unless it is told to.
 * Neither of the two carries a visible label that a name could be taken from, so
 * both need one of their own.
 */
class AppAccessibilityTest final : public QObject
{
    Q_OBJECT

private:
    static QAccessibleInterface *interfaceOf(QWidget *widget)
    {
        return QAccessible::queryAccessibleInterface(widget);
    }

private Q_SLOTS:
    void theAccountViewCarriesANameAndARole();
    void theTransactionViewCarriesANameAndARole();
};

void AppAccessibilityTest::theAccountViewCarriesANameAndARole()
{
    AppCentralWidget widget;

    auto *view = widget.accountWidget();
    QVERIFY(view != nullptr);

    auto *accessible = interfaceOf(view);
    QVERIFY(accessible != nullptr);

    QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
    QCOMPARE(accessible->role(), QAccessible::Tree);
}

void AppAccessibilityTest::theTransactionViewCarriesANameAndARole()
{
    AppCentralWidget widget;

    auto *view = widget.findChild<QTableView *>(QStringLiteral("tableViewTransactions"));
    QVERIFY(view != nullptr);

    auto *accessible = interfaceOf(view);
    QVERIFY(accessible != nullptr);

    QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
    QCOMPARE(accessible->role(), QAccessible::Table);
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppAccessibilityTest)

#include "tst_appaccessibility.moc"
