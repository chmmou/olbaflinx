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

#include "ui/Storage/NewStorageDialog.h"

#include "core/ApplicationInfo.h"
#include "core/Storage/Storage.h"

#include <QtTest/QtTest>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>

#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

using namespace olbaflinx::core;
using namespace olbaflinx::core::storage;
using namespace olbaflinx::ui::storage;

namespace olbaflinx::ui::storage::tests {

/**
 * The dialog used to hand nothing to its caller and to accept whatever was
 * typed, because Ok was wired to accept() in the form itself. It reports name
 * and password now and refuses input that cannot carry a storage.
 */
class NewStorageDialogTest final : public QObject
{
    Q_OBJECT

private:
    static ApplicationInfo applicationInfo()
    {
        return {QStringLiteral("de.chm-projects.olbaflinx.test"),
                QStringLiteral("OlbaFlinxNewStorageDialogTest"),
                QStringLiteral("1.0.0")};
    }

    // Lower and upper case letter, digit and special character, twelve long.
    // Short by four of what the guideline asks for, complete in every other
    // respect, so that a rejection can only come from the length.
    static QString validPassword() { return QStringLiteral("Aa1!Aa1!Aa1!"); }
    static QString shortPassword() { return QStringLiteral("Aa1!Aa1!"); }

    static QLineEdit *nameFieldOf(const NewStorageDialog &dialog)
    {
        return dialog.findChild<QLineEdit *>(QStringLiteral("lineEditStorageName"));
    }

    static QLineEdit *passwordFieldOf(const NewStorageDialog &dialog)
    {
        return dialog.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
    }

    static QLineEdit *confirmFieldOf(const NewStorageDialog &dialog)
    {
        return dialog.findChild<QLineEdit *>(QStringLiteral("lineEditPasswordConfirm"));
    }

    static QPushButton *okButtonOf(const NewStorageDialog &dialog)
    {
        return dialog.findChild<QPushButton *>(QStringLiteral("pushButtonOk"));
    }

    static QLabel *validationLabelOf(const NewStorageDialog &dialog)
    {
        return dialog.findChild<QLabel *>(QStringLiteral("labelValidation"));
    }

    static void enter(const NewStorageDialog &dialog,
                      const QString &name,
                      const QString &password,
                      const QString &confirm)
    {
        nameFieldOf(dialog)->setText(name);
        passwordFieldOf(dialog)->setText(password);
        confirmFieldOf(dialog)->setText(confirm);
    }

private Q_SLOTS:
    void initTestCase();
    void nameAndPasswordReportWhatWasEntered();
    void nothingCanBeConfirmedBeforeAnythingIsEntered();
    void anEmptyNameCannotBeConfirmed();
    void differingPasswordsCannotBeConfirmed();
    void aPasswordBelowTheGuidelineCannotBeConfirmed();
    void aNameThatCannotBeAFileNameCannotBeConfirmed_data();
    void aNameThatCannotBeAFileNameCannotBeConfirmed();
    void bothPasswordFieldsHideTheirContent();
    void theGuidelineIsNamedWithTheNumberTheCoreEnforces();
    void bothPasswordFieldsKeepTheirContentFromTools();
    void everyFieldIsReachedThroughItsLabel_data();
    void everyFieldIsReachedThroughItsLabel();
    void theDialogCarriesATitle();
    void theDialogOpensWithTheFocusOnTheNameField();
    void theFieldsFollowTheArrangementOfTheForm();
};

void NewStorageDialogTest::initTestCase()
{
    // Keeps QSettings out of the real user configuration, see QStandardPaths docs.
    QStandardPaths::setTestModeEnabled(true);
}

/**
 * Without this the caller learns that the dialog was confirmed but not what
 * with, and there is nothing to create.
 */
void NewStorageDialogTest::nameAndPasswordReportWhatWasEntered()
{
    Storage storage(applicationInfo());
    NewStorageDialog dialog(&storage);

    enter(dialog, QStringLiteral("Privat"), validPassword(), validPassword());

    auto *okButton = okButtonOf(dialog);
    QVERIFY(okButton != nullptr);
    QVERIFY(okButton->isEnabled());

    okButton->click();

    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(dialog.name(), QStringLiteral("Privat"));
    QCOMPARE(dialog.password(), validPassword());
}

void NewStorageDialogTest::nothingCanBeConfirmedBeforeAnythingIsEntered()
{
    Storage storage(applicationInfo());
    const NewStorageDialog dialog(&storage);

    const auto *okButton = okButtonOf(dialog);
    QVERIFY(okButton != nullptr);
    QVERIFY(!okButton->isEnabled());
}

/**
 * The reason is shown, not only the refusal.
 */
void NewStorageDialogTest::anEmptyNameCannotBeConfirmed()
{
    Storage storage(applicationInfo());
    const NewStorageDialog dialog(&storage);

    enter(dialog, QString(), validPassword(), validPassword());

    QVERIFY(!okButtonOf(dialog)->isEnabled());

    const auto *validation = validationLabelOf(dialog);
    QVERIFY(validation != nullptr);
    QVERIFY(!validation->text().isEmpty());
}

void NewStorageDialogTest::differingPasswordsCannotBeConfirmed()
{
    Storage storage(applicationInfo());
    const NewStorageDialog dialog(&storage);

    enter(dialog, QStringLiteral("Privat"), validPassword(), validPassword() + QStringLiteral("x"));

    QVERIFY(!okButtonOf(dialog)->isEnabled());
    QVERIFY(!validationLabelOf(dialog)->text().isEmpty());
}

void NewStorageDialogTest::aPasswordBelowTheGuidelineCannotBeConfirmed()
{
    Storage storage(applicationInfo());

    // The two passwords used here have to sit on either side of the guideline,
    // otherwise the test would pass for the wrong reason.
    QVERIFY(storage.minPasswordGuidelines().match(validPassword()).hasMatch());
    QVERIFY(!storage.minPasswordGuidelines().match(shortPassword()).hasMatch());

    const NewStorageDialog dialog(&storage);

    enter(dialog, QStringLiteral("Privat"), shortPassword(), shortPassword());

    QVERIFY(!okButtonOf(dialog)->isEnabled());
    QVERIFY(!validationLabelOf(dialog)->text().isEmpty());
}

/**
 * These five are the names that cannot be a file name under Linux. The dialog
 * names the reason instead of silently replacing the name, because the message
 * about a taken name announces one and the storage has to carry the one the user
 * read.
 */
void NewStorageDialogTest::aNameThatCannotBeAFileNameCannotBeConfirmed_data()
{
    QTest::addColumn<QString>("name");

    QTest::newRow("path separator") << QStringLiteral("Privat/Konten");
    QTest::newRow("null character") << (QStringLiteral("Privat") + QChar(QChar::Null));
    QTest::newRow("spaces only") << QStringLiteral("   ");
    QTest::newRow("current directory") << QStringLiteral(".");
    QTest::newRow("parent directory") << QStringLiteral("..");
}

void NewStorageDialogTest::aNameThatCannotBeAFileNameCannotBeConfirmed()
{
    QFETCH(QString, name);

    Storage storage(applicationInfo());
    const NewStorageDialog dialog(&storage);

    enter(dialog, name, validPassword(), validPassword());

    QVERIFY(!okButtonOf(dialog)->isEnabled());
    QVERIFY(!validationLabelOf(dialog)->text().isEmpty());
}

/**
 * Both fields used to show the pass phrase in clear text.
 */
void NewStorageDialogTest::bothPasswordFieldsHideTheirContent()
{
    Storage storage(applicationInfo());
    const NewStorageDialog dialog(&storage);

    QCOMPARE(passwordFieldOf(dialog)->echoMode(), QLineEdit::Password);
    QCOMPARE(confirmFieldOf(dialog)->echoMode(), QLineEdit::Password);
}

/**
 * The form used to name six characters where the core asks for twelve.
 * The number is taken from the core now, so the two cannot drift apart again.
 */
void NewStorageDialogTest::theGuidelineIsNamedWithTheNumberTheCoreEnforces()
{
    Storage storage(applicationInfo());
    const NewStorageDialog dialog(&storage);

    const QString expected = QString::number(storage.minPasswordLength());

    const auto *description = dialog.findChild<QLabel *>(
        QStringLiteral("labelStoragePasswordDescriptionTop"));
    QVERIFY(description != nullptr);
    QVERIFY(description->text().contains(expected));
    QVERIFY(!description->text().contains(QStringLiteral("%1")));

    for (const auto *help :
         {dialog.findChild<QLabel *>(QStringLiteral("labelHelpPassword")),
          dialog.findChild<QLabel *>(QStringLiteral("labelHelpPasswordConfirm"))}) {
        QVERIFY(help != nullptr);
        QVERIFY(help->toolTip().contains(expected));
        QVERIFY(!help->toolTip().contains(QStringLiteral("%1")));
    }
}

/**
 * Hiding the characters on screen is one thing, keeping them out of the
 * accessibility interface is another. A tool that reads the value of the field
 * would otherwise speak the password out loud.
 */
void NewStorageDialogTest::bothPasswordFieldsKeepTheirContentFromTools()
{
    Storage storage(applicationInfo());
    NewStorageDialog dialog(&storage);

    const QString secret = validPassword();
    enter(dialog, QStringLiteral("Vault"), secret, secret);

    for (auto *field : {passwordFieldOf(dialog), confirmFieldOf(dialog)}) {
        QVERIFY(field != nullptr);

        QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(field);
        QVERIFY(accessible != nullptr);

        QVERIFY(accessible->state().passwordEdit);
        QVERIFY(!accessible->text(QAccessible::Value).contains(secret));
    }
}

/**
 * A label that sits next to a field looks like it belongs to it. Nothing carries
 * that over to a tool but the buddy, and the name the field reports is the one
 * the buddy holds.
 */
void NewStorageDialogTest::everyFieldIsReachedThroughItsLabel_data()
{
    QTest::addColumn<QString>("fieldName");
    QTest::addColumn<QString>("labelName");

    QTest::newRow("name") << QStringLiteral("lineEditStorageName")
                          << QStringLiteral("labelVaultName");
    QTest::newRow("password") << QStringLiteral("lineEditPassword")
                              << QStringLiteral("labelPassword");
    QTest::newRow("confirm") << QStringLiteral("lineEditPasswordConfirm")
                             << QStringLiteral("labelPasswordConfirm");
}

void NewStorageDialogTest::everyFieldIsReachedThroughItsLabel()
{
    QFETCH(QString, fieldName);
    QFETCH(QString, labelName);

    Storage storage(applicationInfo());
    NewStorageDialog dialog(&storage);

    auto *field = dialog.findChild<QLineEdit *>(fieldName);
    QVERIFY(field != nullptr);

    auto *label = dialog.findChild<QLabel *>(labelName);
    QVERIFY(label != nullptr);

    QCOMPARE(label->buddy(), field);

    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(field);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->text(QAccessible::Name), label->text());
}

/**
 * The form left the title at the word the designer puts there, and nothing
 * replaced it. A tool announces a window by its title, and "Dialog" says
 * nothing about which one just opened.
 */
void NewStorageDialogTest::theDialogCarriesATitle()
{
    Storage storage(applicationInfo());
    NewStorageDialog dialog(&storage);

    QVERIFY(!dialog.windowTitle().isEmpty());
    QVERIFY(dialog.windowTitle() != QStringLiteral("Dialog"));
}

/**
 * The name is what the user starts with. A tab order cannot say so: it orders
 * the stops and leaves the first one to whatever the build order produced.
 */
void NewStorageDialogTest::theDialogOpensWithTheFocusOnTheNameField()
{
    Storage storage(applicationInfo());
    NewStorageDialog dialog(&storage);

    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    QCOMPARE(dialog.focusWidget(), nameFieldOf(dialog));
}

/**
 * The form carried a tab order of its own that put Ok in front of Cancel, while
 * the buttons stand the other way round on the screen. The order comes from the
 * arrangement now, and the list that said otherwise is gone.
 */
void NewStorageDialogTest::theFieldsFollowTheArrangementOfTheForm()
{
    Storage storage(applicationInfo());
    NewStorageDialog dialog(&storage);

    // Ok stands greyed out until the input can carry a storage, and a disabled
    // button is no stop of the chain.
    enter(dialog, QStringLiteral("Privat"), validPassword(), validPassword());

    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    const QStringList expected{QStringLiteral("lineEditStorageName"),
                               QStringLiteral("lineEditPassword"),
                               QStringLiteral("lineEditPasswordConfirm"),
                               QStringLiteral("pushButtonCancel"),
                               QStringLiteral("pushButtonOk")};

    QWidget *const start = nameFieldOf(dialog);
    QVERIFY(start != nullptr);

    QStringList reached{start->objectName()};
    for (QWidget *widget = start->nextInFocusChain(); widget != start;
         widget = widget->nextInFocusChain()) {
        if (widget->isEnabled() && widget->isVisible()
            && (widget->focusPolicy() & Qt::TabFocus) == Qt::TabFocus
            && expected.contains(widget->objectName())) {
            reached.append(widget->objectName());
        }
    }

    QCOMPARE(reached, expected);
}

} // namespace olbaflinx::ui::storage::tests

QTEST_MAIN(olbaflinx::ui::storage::tests::NewStorageDialogTest)

#include "tst_newstoragedialog.moc"
