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

#include "ui/Themes/ThemeManager.h"

#include <QtTest/QtTest>

#include <QtWidgets/QApplication>

#include <memory>

using namespace olbaflinx::ui::themes;

namespace olbaflinx::ui::themes::tests {

class ThemeManagerTest final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> workingDirectory;

    static QApplication *application()
    {
        return qobject_cast<QApplication *>(QCoreApplication::instance());
    }

    QString writeStyleSheet(const QString &name, const QString &contents) const
    {
        const QString path = workingDirectory->filePath(name + QStringLiteral(".qss"));

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return {};
        }

        file.write(contents.toUtf8());
        file.close();

        return path;
    }

private Q_SLOTS:
    void init();
    void cleanup();

    void applyPutsTheStyleSheetOnTheApplication();
    void applyLeavesTheStyleSheetAloneForAMissingFile();
    void applyReplacesTheStyleSheetOfAnEarlierTheme();
    void reloadCarriesEveryRegisteredTheme();
    void pixmapIsEmptyForAnUnknownIcon_data();
    void pixmapIsEmptyForAnUnknownIcon();
};

void ThemeManagerTest::init()
{
    workingDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(workingDirectory->isValid());

    QVERIFY(application() != nullptr);
    application()->setStyleSheet(QString());
}

void ThemeManagerTest::cleanup()
{
    application()->setStyleSheet(QString());
    workingDirectory.reset();
}

void ThemeManagerTest::applyPutsTheStyleSheetOnTheApplication()
{
    const QString path = writeStyleSheet(QStringLiteral("light"),
                                         QStringLiteral("QWidget { color: #123456; }"));
    QVERIFY(!path.isEmpty());

    const ThemeManager manager;
    manager.apply(application(), path);

    QCOMPARE(application()->styleSheet(), QStringLiteral("QWidget { color: #123456; }"));
}

/**
 * The failure case. A theme that cannot be read must not clear what is already
 * applied, otherwise a typo in a path leaves the user with an unstyled window.
 */
void ThemeManagerTest::applyLeavesTheStyleSheetAloneForAMissingFile()
{
    const QString path = writeStyleSheet(QStringLiteral("present"),
                                         QStringLiteral("QWidget { color: #abcdef; }"));
    QVERIFY(!path.isEmpty());

    const ThemeManager manager;
    manager.apply(application(), path);

    manager.apply(application(), workingDirectory->filePath(QStringLiteral("absent.qss")));

    QCOMPARE(application()->styleSheet(), QStringLiteral("QWidget { color: #abcdef; }"));
}

void ThemeManagerTest::applyReplacesTheStyleSheetOfAnEarlierTheme()
{
    const QString first = writeStyleSheet(QStringLiteral("first"),
                                          QStringLiteral("QWidget { color: #111111; }"));
    const QString second = writeStyleSheet(QStringLiteral("second"),
                                           QStringLiteral("QWidget { color: #222222; }"));
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());

    const ThemeManager manager;
    manager.apply(application(), first);
    manager.apply(application(), second);

    QCOMPARE(application()->styleSheet(), QStringLiteral("QWidget { color: #222222; }"));
}

/**
 * reload used to look the value of its iterator up as if it were a key, which
 * answers with an empty path, so the body of the loop never opened a file and
 * the style sheet came back empty. Each round also replaced the whole sheet
 * instead of adding to it, which left only one theme in effect.
 */
void ThemeManagerTest::reloadCarriesEveryRegisteredTheme()
{
    const QString first = writeStyleSheet(QStringLiteral("first"),
                                          QStringLiteral("QLabel { color: #111111; }"));
    const QString second = writeStyleSheet(QStringLiteral("second"),
                                           QStringLiteral("QPushButton { color: #222222; }"));
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());

    const ThemeManager manager;
    manager.apply(application(), first);
    manager.apply(application(), second);

    manager.reload();

    const auto styleSheet = application()->styleSheet();

    QVERIFY(!styleSheet.isEmpty());
    QVERIFY(styleSheet.contains(QStringLiteral("QLabel { color: #111111; }")));
    QVERIFY(styleSheet.contains(QStringLiteral("QPushButton { color: #222222; }")));
}

/**
 * An icon name that no resource carries has to answer with an empty pixmap in
 * both modes, not with a painted one of some default size.
 */
void ThemeManagerTest::pixmapIsEmptyForAnUnknownIcon_data()
{
    QTest::addColumn<ThemeManager::Mode>("mode");

    QTest::newRow("light") << ThemeManager::Mode::Light;
    QTest::newRow("dark") << ThemeManager::Mode::Dark;
}

void ThemeManagerTest::pixmapIsEmptyForAnUnknownIcon()
{
    QFETCH(ThemeManager::Mode, mode);

    const QPixmap pixmap = ThemeManager::pixmap(QStringLiteral("no-such-icon"), mode);

    QVERIFY(pixmap.isNull());
}

} // namespace olbaflinx::ui::themes::tests

QTEST_MAIN(olbaflinx::ui::themes::tests::ThemeManagerTest)

#include "tst_thememanager.moc"
