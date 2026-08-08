/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include "ui/App.h"
#include "ui/Logging.h"
#include "ui/Storage/NewStorageDialog.h"
#include "ui/Storage/NewStorageItem.h"

#include "core/Banking/BankingItem.h"
#include "core/Storage/Storage.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <QtGui/QCloseEvent>
#include <QtGui/QFontMetrics>
#include <QtGui/QKeySequence>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayoutItem>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QStyle>
#include <QtWidgets/QWidget>

using namespace olbaflinx::ui;
using namespace olbaflinx::ui::storage;
using namespace olbaflinx::core::storage;

namespace {

// The header height and the logo size derive from the font metrics, so that they
// hold at a different font size or scaling. The factors were measured against the
// fixed values the dialog used to carry, 82 for the header and 64 for the logo,
// at an average character width of 7 and a line height of 17.
//
// The outer dimensions used to be measured the same way, from 930 by 646. They
// belonged to a window; this is a page now, and a page that carries the minimum
// size of a window hands it on to the window it sits in.
constexpr int HeaderHeightInLines = 5;
constexpr int LogoSizeInLines = 4;

} // namespace

class StorageDialog::Private
{
public:
    explicit Private(StorageDialog *storageDialog, Storage *dialogStorage)
        : storage(dialogStorage)
        , q_ptr(storageDialog)
        , app(nullptr)
        , scrollAreaSpacerTop(nullptr)
        , scrollAreaSpacerBottom(nullptr)
        , storageContentsLayout(nullptr)
        , btnNewStorageItem(nullptr)
        , storageInfoLabel(nullptr)
    {}

    // The storage belongs to whoever created the dialog. When it is closed is
    // for the application to decide, not for a window.
    ~Private() = default;

    /**
     * Sizes the header and the logo from the current font.
     *
     * They used to be set in two places, the constructor and initialize, each
     * measuring for itself, and none of them ran again when the font changed.
     * This one runs from initialize and from changeEvent.
     *
     * The scroll area is what handles a window too small for the entries.
     */
    void applyMetrics()
    {
        const QFontMetrics metrics(q_ptr->font());

        if (headerWidget) {
            headerWidget->setMinimumSize(0, metrics.height() * HeaderHeightInLines);
        }

        if (logoLabel) {
            const int logoSize = metrics.height() * LogoSizeInLines;
            logoLabel->setMinimumSize(logoSize, logoSize);
            logoLabel->setMaximumSize(logoSize, logoSize);
        }
    }

    void initialize(QMainWindow *window)
    {
        app = qobject_cast<App *>(window);

        const auto style = q_ptr->style();
        const int horizontalSpacing = style->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
        const int layoutMargin = style->pixelMetric(QStyle::PM_LayoutRightMargin);

        auto verticalLayoutDataVaults = new QVBoxLayout(q_ptr);
        verticalLayoutDataVaults->setSpacing(0);
        verticalLayoutDataVaults->setObjectName(QStringLiteral("verticalLayoutDataVaults"));
        verticalLayoutDataVaults->setContentsMargins(0, 0, 0, 0);

        headerWidget = new QWidget(q_ptr);
        headerWidget->setObjectName(QStringLiteral("widgetStorageHeader"));

        auto hlStorageWidgetInfo = new QHBoxLayout(headerWidget);
        hlStorageWidgetInfo->setSpacing(horizontalSpacing);
        hlStorageWidgetInfo->setObjectName(QStringLiteral("hlStorageWidgetInfo"));

        logoLabel = new QLabel(headerWidget);
        logoLabel->setObjectName(QStringLiteral("lblStorageInfoIcon"));
        logoLabel->setPixmap(QPixmap(QStringLiteral(":/app/olbaflinx-logo-128")));
        logoLabel->setScaledContents(true);

        hlStorageWidgetInfo->addWidget(logoLabel);

        auto lblStorageInfoTitle = new QLabel(headerWidget);
        lblStorageInfoTitle->setObjectName(QStringLiteral("lblStorageInfoTitle"));
        lblStorageInfoTitle->setAlignment(Qt::AlignCenter);
        lblStorageInfoTitle->setText(tr("OlbaFlinx - Online Banking For Linux"));

        hlStorageWidgetInfo->addWidget(lblStorageInfoTitle);

        verticalLayoutDataVaults->addWidget(headerWidget);

        auto scrollAreaStorage = new QScrollArea(q_ptr);
        scrollAreaStorage->setObjectName(QStringLiteral("scrollAreaStorage"));
        scrollAreaStorage->setWidgetResizable(true);

        // No parent on purpose. QScrollArea::setWidget takes over ownership in
        // the next line, a parent here would be undone right away.
        auto scrollAreaStorageContents = new QWidget();
        scrollAreaStorageContents->setObjectName(QStringLiteral("scrollAreaStorageContents"));

        scrollAreaStorage->setWidget(scrollAreaStorageContents);
        scrollAreaStorage->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollAreaStorage->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollAreaStorage->setFrameShape(QScrollArea::NoFrame);
        scrollAreaStorage->setFrameShadow(QScrollArea::Plain);

        verticalLayoutDataVaults->addWidget(scrollAreaStorage);

        auto hlStoragePage = new QHBoxLayout();
        hlStoragePage->setObjectName(QStringLiteral("hlStoragePage"));
        hlStoragePage->setContentsMargins(-1, layoutMargin, layoutMargin, layoutMargin);

        // An expanding spacer. Its numbers are the minimum it takes, not a
        // measure of anything, so they come from the layout metrics too.
        auto hsStoragePage = new QSpacerItem(horizontalSpacing,
                                             horizontalSpacing,
                                             QSizePolicy::Policy::Expanding,
                                             QSizePolicy::Policy::Minimum);

        hlStoragePage->addItem(hsStoragePage);

        btnNewStorageItem = new QPushButton(q_ptr);
        btnNewStorageItem->setObjectName(QStringLiteral("btnNewStorageItem"));

        QIcon icon;
        icon.addFile(QStringLiteral(":/datavault/add"), QSize(), QIcon::Normal, QIcon::Off);

        btnNewStorageItem->setIcon(icon);
        btnNewStorageItem->setFlat(true);

        // The shortcut used to sit on this button. It belongs to the menu entry
        // now, which carries the same command; two widgets on one sequence make
        // it ambiguous and neither of them fires.
        connect(btnNewStorageItem, &QPushButton::clicked, q_ptr, [&] { addNewStorageItem(); });

        hlStoragePage->addWidget(btnNewStorageItem);

        verticalLayoutDataVaults->addLayout(hlStoragePage);

        storageContentsLayout = new QVBoxLayout(scrollAreaStorageContents);

        scrollAreaSpacerTop = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
        scrollAreaSpacerBottom = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);

        createStorageInfoLabel();

        // Runs last, when every widget it sizes exists.
        applyMetrics();
    }

    void loadStorageItems()
    {
        // The overview used to be appended to rather than built, so a second
        // call showed every entry a second time.
        clearStorageItems();

        const QStringList stored = storedPaths();

        QStringList existing;
        existing.reserve(stored.size());

        for (const auto &file : stored) {
            if (QFileInfo::exists(file)) {
                existing.append(file);
            }
        }

        // Building the overview is the only moment the application looks at the
        // files, so it is the moment an entry whose file was removed elsewhere
        // leaves the list.
        if (existing.size() != stored.size()) {
            storePaths(existing);
        }

        if (existing.isEmpty()) {
            addStorageInfo();
            return;
        }

        for (const auto &file : std::as_const(existing)) {
            addStorageItem(QFileInfo(file).baseName(), file);
        }

        storageContentsLayout->addItem(scrollAreaSpacerBottom);
        storageContentsLayout->update();
    }

    QString availableName(const QString &name) const
    {
        if (!QFileInfo::exists(storageFilePath(name))) {
            return name;
        }

        // Every step asks about a different file, so the loop ends as soon as
        // one of the names is free.
        for (int number = 2;; ++number) {
            const QString candidate = QStringLiteral("%1%2%3").arg(name,
                                                                   nameSeparator(),
                                                                   QString::number(number));

            if (!QFileInfo::exists(storageFilePath(candidate))) {
                return candidate;
            }
        }
    }

    bool createStorage(const QString &name, const QString &password)
    {
        const QString directory = storage->storagePath();

        if (!QDir().mkpath(directory)) {
            qCWarning(lcUiStorage) << "could not create the directory for the storages";

            Q_EMIT q_ptr->message(
                tr("The directory for your data vaults could not be created. Check the permissions "
                   "on your home directory."));

            return false;
        }

        const QString filePath = storageFilePath(name);

        // The dialog refuses a name that cannot be a file name, but this call is
        // reachable without it, and a name carrying ".." would write outside the
        // directory the storages live in. No message box: a user cannot reach
        // this through the dialog, so the log is where it belongs.
        if (QFileInfo(filePath).absoluteDir().canonicalPath() != QDir(directory).canonicalPath()) {
            qCWarning(lcUiStorage)
                << "the name does not stay inside the directory for the storages";

            return false;
        }

        if (const auto error = storage->setKey(password); error.isError()) {
            qCWarning(lcUiStorage) << "the key was refused:" << error.message();

            Q_EMIT q_ptr->message(
                tr("The password does not meet the guidelines. Choose a longer one."));

            return false;
        }

        storage->setStorageFile(filePath);

        // The file is written here and not on the first open. An entry without a
        // file would be dropped again the next time the overview is built.
        if (const auto error = storage->initialize(true); error.isError()) {
            qCWarning(lcUiStorage) << "could not create the storage:" << error.message();

            storage->close();
            Q_EMIT q_ptr->message(tr("The data vault \"%1\" could not be created. Check the "
                                     "permissions on the directory it belongs in.")
                                      .arg(name));

            return false;
        }

        storage->close();

        QStringList paths = storedPaths();
        paths.append(filePath);
        storePaths(paths);

        loadStorageItems();

        return true;
    }

    void addNewStorageItem()
    {
        NewStorageDialog dialog(storage, q_ptr);

        // The dialog is shown again rather than built again, so that cancelling
        // the conflict message leaves it standing with what was entered.
        while (dialog.exec() == QDialog::Accepted) {
            const QString wanted = dialog.name();
            const QString available = availableName(wanted);

            if (available != wanted) {
                const auto answer = QMessageBox::question(
                    q_ptr,
                    tr("Storage"),
                    tr("A data vault named \"%1\" already exists. The new one is created as "
                       "\"%2\".")
                        .arg(wanted, available));

                if (answer != QMessageBox::Yes) {
                    continue;
                }
            }

            createStorage(available, dialog.password());
            return;
        }
    }

    Storage *storage;

private:
    [[nodiscard]] QStringList storedPaths() const
    {
        return storage->setting(QStringLiteral("Paths"), QStringLiteral("Items"), QStringList())
            .toStringList();
    }

    void storePaths(const QStringList &paths) const
    {
        // storeSetting takes key, value, group and setting takes key, group,
        // default. A swapped pair files the list where the reading side never
        // looks for it.
        storage->storeSetting(QStringLiteral("Paths"), paths, QStringLiteral("Items"));
    }

    [[nodiscard]] QString storageFilePath(const QString &name) const
    {
        return QStringLiteral("%1/%2%3").arg(storage->storagePath(), name, storageFileSuffix());
    }

    void clearStorageItems()
    {
        const auto items = q_ptr->findChildren<NewStorageItem *>();

        for (auto *item : items) {
            storageContentsLayout->removeWidget(item);
            delete item;
        }

        removeStorageInfo();
    }

    void addStorageItem(const QString &title, const QString &fileName)
    {
        auto storageItem = new NewStorageItem(storage, q_ptr);
        storageItem->setTitle(title);
        storageItem->setFilePath(fileName);

        QFileInfo fi(fileName);
        QString lastModifiedDateTimeString = fi.lastModified().toString(Private::dateFormat());

        if (lastModifiedDateTimeString.isEmpty()) {
            // Local time, not UTC. The line above reads the modification time of
            // the file, which QFileInfo answers in local time, and the two
            // branches of the same label have to name the same zone.
            lastModifiedDateTimeString = QDateTime::currentDateTime().toString(
                Private::dateFormat());
        }

        // The label used to say "Created on" while showing this value. The two
        // fall together only until something is written; from the first account
        // onwards the file carries a later time than the day it was made, and no
        // creation time is kept anywhere.
        storageItem->setFileInfo(tr("Changed on %1").arg(lastModifiedDateTimeString));

        disconnect(storageItem, &NewStorageItem::storageOpened, nullptr, nullptr);
        disconnect(storageItem, &NewStorageItem::storageDeleted, nullptr, nullptr);

        connect(storageItem,
                &NewStorageItem::storageOpened,
                q_ptr,
                [&](const QString &filePath, const QString &password) {
                    // Two checks, and both are needed. The character classes are
                    // checked here, where a vault is created and the user can
                    // still choose another phrase. The length is checked in the
                    // core, where no caller can walk past it.
                    const bool creating = !QFile::exists(filePath);

                    // The name stands for the file in everything the user gets to
                    // read. The full path names his home directory and stays out
                    // of it, and out of the log.
                    const QString name = QFileInfo(filePath).baseName();

                    if (creating && !storage->minPasswordGuidelines().match(password).hasMatch()) {
                        Q_EMIT q_ptr->message(
                            tr("The password needs at least %1 characters, among them a lower and "
                               "an upper case letter, a digit and a special character.")
                                .arg(storage->minPasswordLength()));

                        return;
                    }

                    if (const auto error = storage->setKey(password); error.isError()) {
                        qCWarning(lcUiStorage) << "the key was refused:" << error.message();

                        Q_EMIT q_ptr->message(
                            tr("The password has to be at least %1 characters long, so this one "
                               "cannot be the right one.")
                                .arg(storage->minPasswordLength()));

                        return;
                    }

                    storage->setStorageFile(filePath);

                    if (const auto error = storage->initialize(true); error.isError()) {
                        // The technical message names the file and the statement
                        // and goes to the log. What reaches the screen is the name
                        // the user gave the vault and what he can do about it.
                        qCWarning(lcUiStorage) << "could not open a storage:" << error.message();

                        storage->close();
                        Q_EMIT q_ptr->message(
                            tr("\"%1\" could not be opened. Check the password, or restore a "
                               "backup if the file is damaged.")
                                .arg(name));

                        return;
                    }

                    if (!storage->isValid()) {
                        storage->close();
                        Q_EMIT q_ptr->message(
                            tr("\"%1\" is damaged, or it cannot be read and written.").arg(name));

                        return;
                    }

                    disconnect(storage, &Storage::itemsReceived, nullptr, nullptr);
                    connect(storage, &Storage::itemsReceived, q_ptr, [&](const BankingItems &items) {
                        app->setAccounts(items);
                    });

                    storage->receiveItems(Storage::StorageAccount);

                    // Last, and only on the way that got through. The window turns
                    // to the page with the accounts on it when it sees this.
                    Q_EMIT q_ptr->storageOpened();
                });

        connect(storageItem,
                &NewStorageItem::storageDeleted,
                q_ptr,
                [this](bool success, NewStorageItem *item, const QString &reason) {
                    if (!success) {
                        qCWarning(lcUiStorage) << "could not remove a storage file:" << reason;

                        Q_EMIT q_ptr->message(tr("\"%1\" could not be removed. Check the "
                                                 "permissions on the file.")
                                                  .arg(QFileInfo(item->filePath()).baseName()));
                        return;
                    }

                    QStringList paths = storedPaths();
                    paths.removeAll(item->filePath());
                    storePaths(paths);

                    item->deleteLater();
                });

        storageContentsLayout->addWidget(storageItem);
    }

    void addStorageInfo()
    {
        storageContentsLayout->addItem(scrollAreaSpacerTop);
        storageContentsLayout->addWidget(storageInfoLabel);
        storageContentsLayout->addItem(scrollAreaSpacerBottom);

        // The layout does not undo the hide() from createStorageInfoLabel.
        storageInfoLabel->show();
    }

    void removeStorageInfo()
    {
        int indexOf = storageContentsLayout->indexOf(scrollAreaSpacerTop);
        if (indexOf >= 0) {
            auto item = storageContentsLayout->takeAt(indexOf);
            delete item;

            scrollAreaSpacerTop = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
        }

        indexOf = storageContentsLayout->indexOf(storageInfoLabel);
        if (indexOf >= 0) {
            auto item = storageContentsLayout->takeAt(indexOf);
            delete item->widget();
            storageInfoLabel = nullptr;
            delete item;

            createStorageInfoLabel();
        }

        indexOf = storageContentsLayout->indexOf(scrollAreaSpacerBottom);
        if (indexOf >= 0) {
            auto item = storageContentsLayout->takeAt(indexOf);
            delete item;

            scrollAreaSpacerBottom = new QSpacerItem(1,
                                                     1,
                                                     QSizePolicy::Fixed,
                                                     QSizePolicy::Expanding);
        }

        storageContentsLayout->update();
    }

    void createStorageInfoLabel()
    {
        if (storageInfoLabel == nullptr) {
            storageInfoLabel = new QLabel(q_ptr);
            storageInfoLabel->setTextFormat(Qt::RichText);
            storageInfoLabel->setAlignment(Qt::AlignCenter);
            storageInfoLabel->setText(
                tr("<h1>Welcome to OlbaFlinx</h1>"
                   "<p>Click the plus sign or type Ctrl+N to create a new data storage.</p>"
                   "<p>You can create as many vaults as you like, each with its own password, e.g. "
                   "for different user and or accounts.</p>"));

            // Until addStorageInfo puts it into the layout, nothing places this
            // label. A child that no layout places sits in the top left corner
            // of its parent, and one built before the window is first shown
            // becomes visible along with it. That is what happened at every
            // start that found an entry in the list.
            storageInfoLabel->hide();
        }
    }

    static QString dateFormat() { return QStringLiteral("dd.MM.yyyy hh:mm"); }

    static QString storageFileSuffix() { return QStringLiteral(".olbflx"); }

    // A name that is taken only needs a number at the end. The hyphen keeps that
    // number apart from a name that already ends in a digit, where "Konto2020"
    // and a 2 would otherwise read as "Konto20202".
    static QString nameSeparator() { return QStringLiteral("-"); }

    StorageDialog *q_ptr;
    App *app;

    QSpacerItem *scrollAreaSpacerTop;
    QSpacerItem *scrollAreaSpacerBottom;
    QVBoxLayout *storageContentsLayout;
    QPushButton *btnNewStorageItem;
    QLabel *storageInfoLabel;

    // Kept so that applyMetrics can size them again after a font change. Owned by
    // the dialog through the widget hierarchy.
    QWidget *headerWidget = nullptr;
    QLabel *logoLabel = nullptr;
};

StorageDialog::StorageDialog(Storage *storage, QWidget *parent)
    : QWidget(parent)
    , d_ptr(new Private(this, storage))
{}

StorageDialog::~StorageDialog()
{
    delete d_ptr;
}

void StorageDialog::initialize(QMainWindow *window)
{
    // Position and size used to be read here and written back in moveEvent and
    // resizeEvent. The overview is a page of the window now, and a page gets
    // neither event in any useful way. The window keeps its own geometry.
    d_ptr->initialize(window);
    d_ptr->loadStorageItems();
}

void StorageDialog::reload()
{
    d_ptr->loadStorageItems();
}

void StorageDialog::addStorage()
{
    d_ptr->addNewStorageItem();
}

QString StorageDialog::availableName(const QString &name) const
{
    return d_ptr->availableName(name);
}

bool StorageDialog::createStorage(const QString &name, const QString &password)
{
    return d_ptr->createStorage(name, password);
}

void StorageDialog::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::FontChange) {
        d_ptr->applyMetrics();
    }
}
