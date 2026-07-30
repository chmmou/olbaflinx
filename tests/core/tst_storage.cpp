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

    static bool accepts(const QString &password)
    {
        const Storage storage(applicationInfo());
        return storage.minPasswordGuidelines().match(password).hasMatch();
    }

private Q_SLOTS:
    void initTestCase();
    void minPasswordGuidelinesReturnsValidPattern();
    void minPasswordGuidelinesIsStable();
    void passwordPolicyRejectsDigitsOnlyAsSpecialChar();
    void passwordPolicyRejectsTooShort();
    void passwordPolicyRejectsTooLong();
    void passwordPolicyAcceptsUmlautAsSpecialChar();
    void passwordPolicyAcceptsBackslash();
    void passwordPolicyAcceptsTheDocumentedExample();
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

/**
 * The class of special characters used to carry the sequence '#-_', which a
 * character class reads as a range from 0x23 to 0x5F. That covers every digit and
 * every capital letter, so the lookahead for a special character matched on those
 * alone and asked for nothing beyond the two lookaheads before it.
 */
void StorageGuidelinesTest::passwordPolicyRejectsDigitsOnlyAsSpecialChar()
{
    QVERIFY(!accepts(QStringLiteral("Abcdefgh1234")));
}

void StorageGuidelinesTest::passwordPolicyRejectsTooShort()
{
    QVERIFY(!accepts(QStringLiteral("Abcdef1!")));
}

/**
 * An unbounded length is an unchecked size, see QT-SEC-004. There used to be no
 * upper bound at all.
 */
void StorageGuidelinesTest::passwordPolicyRejectsTooLong()
{
    const auto password = QStringLiteral("Ab1!") + QString(125, QLatin1Char('c'));
    QCOMPARE(password.length(), 129);

    QVERIFY(!accepts(password));
    QVERIFY(accepts(password.left(128)));
}

void StorageGuidelinesTest::passwordPolicyAcceptsUmlautAsSpecialChar()
{
    QVERIFY(accepts(QStringLiteral("Paßwort-Ümlaut-2026")));
}

/**
 * The backslash, 0x5C, used to reach the class only through the range described
 * above. Removing that range would have taken it out along with the digits, so it
 * now stands in the class on its own. This test holds that in place.
 */
void StorageGuidelinesTest::passwordPolicyAcceptsBackslash()
{
    QVERIFY(accepts(QStringLiteral("Passwort\\mit1X")));
}

void StorageGuidelinesTest::passwordPolicyAcceptsTheDocumentedExample()
{
    QVERIFY(accepts(QStringLiteral("M'yF13\"stP\\$44W0$3d/")));
}

} // namespace olbaflinx::core::storage::tests

QTEST_APPLESS_MAIN(olbaflinx::core::storage::tests::StorageGuidelinesTest)

#include "tst_storage.moc"
