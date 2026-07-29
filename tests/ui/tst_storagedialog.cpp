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

#include "ui/Storage/StorageDialog.h"

#include "core/ApplicationInfo.h"
#include "core/Storage/Storage.h"

#include <QtTest/QtTest>

#include <QtWidgets/QLabel>

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui::storage;

namespace olbaflinx::ui::storage::tests {

class StorageDialogTest final : public QObject
{
    Q_OBJECT

private:
    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxStorageDialogTest"),
                QStringLiteral("1.0.0")};
    }

private Q_SLOTS:
    void initTestCase();
    void repeatedReloadKeepsTheInfoLabelUsable();
    void dialogDoesNotCloseTheStorageItDoesNotOwn();
};

void StorageDialogTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);
}

/**
 * Nachholung des Tests zu B00-03b: removeStorageInfo() gab das Label frei, ohne
 * den Member auf nullptr zu setzen. Der zweite Durchlauf traf damit auf einen
 * freigegebenen Zeiger. Der Fehler zeigt sich nur als Absturz, nicht als
 * fehlgeschlagene Zusicherung.
 *
 * Der Pfad ist ueber reload() erreichbar, weil loadStorageItems() bei leerer
 * Liste removeStorageInfo() und addStorageInfo() nacheinander aufruft.
 */
void StorageDialogTest::repeatedReloadKeepsTheInfoLabelUsable()
{
    Storage storage(applicationInfo());
    storage.storeSetting("Paths", QStringList(), "Items");

    StorageDialog dialog(&storage);
    dialog.initialize(nullptr);

    dialog.reload();
    dialog.reload();

    const auto labels = dialog.findChildren<QLabel *>();
    const bool hasInfoLabel = std::any_of(labels.cbegin(), labels.cend(), [](const QLabel *label) {
        return label->textFormat() == Qt::RichText && label->text().contains("OlbaFlinx");
    });

    QVERIFY(hasInfoLabel);
}

/**
 * Der Dialog besass den Datenspeicher frueher mit und gab ihn im eigenen
 * Destruktor frei. Nach dem Umbau bleibt er nach der Zerstoerung des Fensters
 * benutzbar.
 */
void StorageDialogTest::dialogDoesNotCloseTheStorageItDoesNotOwn()
{
    Storage storage(applicationInfo());
    storage.storeSetting("Paths", QStringList(), "Items");

    {
        StorageDialog dialog(&storage);
        dialog.initialize(nullptr);
    }

    storage.storeSetting("Probe", QStringList(), "Lifetime");

    QCOMPARE(storage.setting("Probe", "Lifetime", QStringList()).toStringList().size(), 0);
}

} // namespace olbaflinx::ui::storage::tests

QTEST_MAIN(olbaflinx::ui::storage::tests::StorageDialogTest)

#include "tst_storagedialog.moc"
