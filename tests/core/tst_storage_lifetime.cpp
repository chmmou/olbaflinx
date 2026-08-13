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
#include "core/Storage/Storage.h"

#include "TestHelpers.h"

#include <QtTest/QtTest>

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests {

using namespace olbaflinx::core::tests;

class StorageLifetimeTest final : public QObject
{
    Q_OBJECT

private:
    static ApplicationInfo applicationInfo()
    {
        return TestHelpers::applicationInfo(QStringLiteral("OlbaFlinxStorageLifetimeTest"));
    }

private Q_SLOTS:
    void initTestCase();
    void destroyingUnusedStorageDoesNotCrash();
    void destroyingStorageWithSettingsDoesNotCrash();
    void twoConsecutiveStoragesDoNotCrash();
    void storageIsUsableWithoutAnyApplicationInstance();
};

void StorageLifetimeTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);
}

/**
 * Storage::Private::m_settings stays null until the first read or write of a setting.
 * Without the null check in ~Private() this call crashes with SIGSEGV instead of failing
 * an assertion; a crash is the only observable form a lifetime fault of this kind takes.
 */
void StorageLifetimeTest::destroyingUnusedStorageDoesNotCrash()
{
    auto *storage = new Storage(applicationInfo());
    QVERIFY(storage != nullptr);

    delete storage;

    QVERIFY(true);
}

void StorageLifetimeTest::destroyingStorageWithSettingsDoesNotCrash()
{
    auto *storage = new Storage(applicationInfo());
    storage->storeSetting("Probe", QStringList(), "Lifetime");

    delete storage;

    QVERIFY(true);
}

/**
 * While Storage was a singleton, the destructor of the base class released the
 * very same instance a second time. The second run therefore hit an already
 * released pointer. This fault, too, only ever shows up as a crash.
 */
void StorageLifetimeTest::twoConsecutiveStoragesDoNotCrash()
{
    {
        Storage first(applicationInfo());
        first.storeSetting("Probe", QStringList(), "Lifetime");
    }

    {
        Storage second(applicationInfo());
        second.storeSetting("Probe", QStringList(), "Lifetime");
    }

    QVERIFY(true);
}

/**
 * Shows that the storage path comes from ApplicationInfo and not from a running
 * application instance. This target uses QTEST_APPLESS_MAIN, so there is none.
 */
void StorageLifetimeTest::storageIsUsableWithoutAnyApplicationInstance()
{
    QVERIFY(QCoreApplication::instance() == nullptr);

    const Storage storage(applicationInfo());

    QVERIFY(storage.storagePath().endsWith("de.chm-projects.olbaflinx.test"));
}

} // namespace olbaflinx::core::storage::tests

QTEST_APPLESS_MAIN(olbaflinx::core::storage::tests::StorageLifetimeTest)

#include "tst_storage_lifetime.moc"
