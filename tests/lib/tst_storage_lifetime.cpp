/**
 * Copyright (C) 2021-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <Storage/Storage.h>

#include <QtTest/QtTest>

using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests {

/**
 * Storage::Storage() is protected and only reachable through Singleton<Storage>::instance().
 * Deriving gives access to it without ever setting Singleton<Storage>::_instance, so
 * ~Singleton() runs "delete nullptr" and the double free of the singleton instance
 * (Singleton.h:35) stays out of the way of this test.
 */
class TestableStorage final : public Storage
{};

class StorageLifetimeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void destroyingUnusedStorageDoesNotCrash();
    void destroyingStorageWithSettingsDoesNotCrash();
};

void StorageLifetimeTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);

    QCoreApplication::setOrganizationName("de.chm-projects.olbaflinx.test");
    QCoreApplication::setApplicationName("OlbaFlinxStorageLifetimeTest");
}

/**
 * Storage::Private::m_settings stays null until the first read or write of a setting.
 * Without the null check in ~Private() this call crashes with SIGSEGV instead of failing
 * an assertion; a crash is the only observable form a lifetime fault of this kind takes.
 */
void StorageLifetimeTest::destroyingUnusedStorageDoesNotCrash()
{
    auto *storage = new TestableStorage();
    QVERIFY(storage != nullptr);

    delete storage;

    QVERIFY(true);
}

void StorageLifetimeTest::destroyingStorageWithSettingsDoesNotCrash()
{
    auto *storage = new TestableStorage();
    storage->storeSetting("Probe", QStringList(), "Lifetime");

    delete storage;

    QVERIFY(true);
}

} // namespace olbaflinx::core::storage::tests

QTEST_APPLESS_MAIN(olbaflinx::core::storage::tests::StorageLifetimeTest)

#include "tst_storage_lifetime.moc"
