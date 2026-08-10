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
    void everyControlOfTheFilterBarCarriesANameAndARole_data();
    void everyControlOfTheFilterBarCarriesANameAndARole();
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

/**
 * The bar above the transactions carries four controls and one readout, and
 * none of them stands next to a visible label. Each says what it is and what
 * kind of thing it is.
 */
void AppAccessibilityTest::everyControlOfTheFilterBarCarriesANameAndARole_data()
{
    QTest::addColumn<QString>("objectName");
    QTest::addColumn<QAccessible::Role>("role");

    QTest::newRow("search") << QStringLiteral("lineEditTransactionSearch")
                            << QAccessible::EditableText;
    QTest::newRow("period") << QStringLiteral("comboBoxTransactionPeriod") << QAccessible::ComboBox;
    QTest::newRow("direction") << QStringLiteral("comboBoxTransactionDirection")
                               << QAccessible::ComboBox;
    QTest::newRow("reset") << QStringLiteral("pushButtonTransactionFilterReset")
                           << QAccessible::Button;
    QTest::newRow("counter") << QStringLiteral("labelTransactionCount") << QAccessible::StaticText;
}

void AppAccessibilityTest::everyControlOfTheFilterBarCarriesANameAndARole()
{
    QFETCH(QString, objectName);
    QFETCH(QAccessible::Role, role);

    AppCentralWidget widget;

    auto *control = widget.findChild<QWidget *>(objectName);
    QVERIFY(control != nullptr);

    auto *accessible = interfaceOf(control);
    QVERIFY(accessible != nullptr);

    QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
    QCOMPARE(accessible->role(), role);
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppAccessibilityTest)

#include "tst_appaccessibility.moc"
