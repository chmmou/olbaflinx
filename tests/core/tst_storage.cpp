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

#include <QtTest/QtTest>

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;

namespace olbaflinx::core::storage::tests {

class StorageGuidelinesTest final : public QObject
{
    Q_OBJECT

private:
    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxStorageGuidelinesTest"),
                QStringLiteral("1.0.0")};
    }

private Q_SLOTS:
    void initTestCase();
    void minPasswordGuidelinesReturnsValidPattern();
    void minPasswordGuidelinesIsStable();
};

void StorageGuidelinesTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);
}

/**
 * The pattern used to be built by a macro on every call. A typo in the escaping
 * would only have shown up as a never matching password. The check on
 * QRegularExpression::isValid catches that at the source.
 */
void StorageGuidelinesTest::minPasswordGuidelinesReturnsValidPattern()
{
    const Storage storage(applicationInfo());

    const QRegularExpression pattern = storage.minPasswordGuidelines();

    QVERIFY(pattern.isValid());
    QVERIFY(!pattern.pattern().isEmpty());
    QCOMPARE(pattern.errorString(), QStringLiteral("no error"));
}

/**
 * The pattern is now held in a function local static. Two calls have to yield
 * the same pattern, otherwise the compiled form is not shared.
 */
void StorageGuidelinesTest::minPasswordGuidelinesIsStable()
{
    const Storage storage(applicationInfo());

    const QRegularExpression first = storage.minPasswordGuidelines();
    const QRegularExpression second = storage.minPasswordGuidelines();

    QCOMPARE(first.pattern(), second.pattern());
    QCOMPARE(first, second);
}

} // namespace olbaflinx::core::storage::tests

QTEST_APPLESS_MAIN(olbaflinx::core::storage::tests::StorageGuidelinesTest)

#include "tst_storage.moc"
