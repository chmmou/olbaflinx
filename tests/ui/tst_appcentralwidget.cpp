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

#include <QtWidgets/QStackedWidget>

using namespace olbaflinx::ui;

namespace olbaflinx::ui::tests {

/**
 * The central area used to be a single page with two widgets on fixed
 * rectangles. It carries two pages now, and the window switches between them
 * when a storage is opened or closed (FR-040).
 */
class AppCentralWidgetTest final : public QObject
{
    Q_OBJECT

private:
    static QStackedWidget *pagesOf(const AppCentralWidget &widget)
    {
        return widget.findChild<QStackedWidget *>(QStringLiteral("stackedWidgetPages"));
    }

private Q_SLOTS:
    void startsOnTheStorageOverview();
    void switchingKeepsBothPagesAlive();
    void switchingToThePageAlreadyShownChangesNothing();
};

void AppCentralWidgetTest::startsOnTheStorageOverview()
{
    const AppCentralWidget widget;

    QCOMPARE(widget.page(), AppCentralWidget::Page::Storages);

    const auto *pages = pagesOf(widget);
    QVERIFY(pages != nullptr);
    QCOMPARE(pages->count(), 2);
    QCOMPARE(pages->currentIndex(), 0);
}

/**
 * The page that is left has to survive being left. FR-015 lets the models drop
 * their records when a storage is closed, not the page drop its widgets.
 */
void AppCentralWidgetTest::switchingKeepsBothPagesAlive()
{
    AppCentralWidget widget;

    const auto *pages = pagesOf(widget);
    QVERIFY(pages != nullptr);

    const QWidget *storagesPage = pages->widget(0);
    const QWidget *bankingPage = pages->widget(1);
    QVERIFY(storagesPage != nullptr);
    QVERIFY(bankingPage != nullptr);

    widget.setPage(AppCentralWidget::Page::Banking);

    QCOMPARE(widget.page(), AppCentralWidget::Page::Banking);
    QCOMPARE(pages->currentIndex(), 1);
    QCOMPARE(pages->count(), 2);
    QCOMPARE(pages->widget(0), storagesPage);
    QCOMPARE(pages->widget(1), bankingPage);

    widget.setPage(AppCentralWidget::Page::Storages);

    QCOMPARE(widget.page(), AppCentralWidget::Page::Storages);
    QCOMPARE(pages->currentIndex(), 0);
    QCOMPARE(pages->count(), 2);
    QCOMPARE(pages->widget(0), storagesPage);
    QCOMPARE(pages->widget(1), bankingPage);
}

void AppCentralWidgetTest::switchingToThePageAlreadyShownChangesNothing()
{
    AppCentralWidget widget;

    widget.setPage(AppCentralWidget::Page::Storages);

    QCOMPARE(widget.page(), AppCentralWidget::Page::Storages);

    widget.setPage(AppCentralWidget::Page::Banking);
    widget.setPage(AppCentralWidget::Page::Banking);

    QCOMPARE(widget.page(), AppCentralWidget::Page::Banking);
    QCOMPARE(pagesOf(widget)->currentIndex(), 1);
}

} // namespace olbaflinx::ui::tests

QTEST_MAIN(olbaflinx::ui::tests::AppCentralWidgetTest)

#include "tst_appcentralwidget.moc"
