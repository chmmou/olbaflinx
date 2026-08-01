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
#include "ui/Storage/NewStorageItem.h"

#include "core/Banking/BankingItem.h"
#include "core/Storage/Storage.h"

#include <QtCore/QDateTime>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <QtGui/QCloseEvent>
#include <QtGui/QFontMetrics>
#include <QtGui/QKeySequence>
#include <QtGui/QMoveEvent>
#include <QtGui/QResizeEvent>

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

// The outer dimensions of the dialog, the header height and the logo size derive
// from the font metrics, so that they hold at a different font size or scaling.
//
// The factors were measured against the fixed values the dialog used to carry,
// 930 by 646 for itself, 82 for the header and 64 for the logo. At the default
// font of this platform, an average character width of 7 and a line height of
// 17, they come to 132.9, 38.0, 4.8 and 3.8. The first two were guessed at 120
// and 30 before, which made the dialog about a tenth narrower and a fifth
// shorter than it was meant to be.
constexpr int DialogWidthInCharacters = 133;
constexpr int DialogHeightInLines = 38;
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
     * Sizes the dialog, its header and its logo from the current font.
     *
     * They used to be set in two places, the constructor and initialize, each
     * measuring for itself, and none of them ran again when the font changed.
     * This one runs from initialize and from changeEvent.
     */
    void applyMetrics()
    {
        const QFontMetrics metrics(q_ptr->font());

        q_ptr->setMinimumSize(metrics.averageCharWidth() * DialogWidthInCharacters,
                              metrics.height() * DialogHeightInLines);

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
        btnNewStorageItem->setShortcut(QKeySequence(QStringLiteral("Ctrl+N")));
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
        auto items = storage
                         ->setting(QStringLiteral("Paths"), QStringLiteral("Items"), QStringList())
                         .toStringList();
        if (items.isEmpty()) {
            removeStorageInfo();
            addStorageInfo();
            return;
        }

        for (const auto &file : std::as_const(items)) {
            addStorageItem(QFileInfo(file).baseName(), file);
        }

        storageContentsLayout->addItem(scrollAreaSpacerBottom);
        storageContentsLayout->update();

        items.clear();
    }

    Storage *storage;

private:
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

        storageItem->setFileInfo(tr("Created on %1").arg(lastModifiedDateTimeString));

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

                    if (creating && !storage->minPasswordGuidelines().match(password).hasMatch()) {
                        QMessageBox::critical(
                            q_ptr,
                            tr("Error"),
                            tr("The password does not meet the guidelines. It needs at least 12 "
                               "characters, among them a lower case and an upper case letter, a "
                               "digit and a special character."));

                        return;
                    }

                    if (const auto error = storage->setKey(password); error.isError()) {
                        qCWarning(lcUiStorage) << "the key was refused:" << error.message();

                        QMessageBox::critical(q_ptr,
                                              tr("Error"),
                                              tr("The password has to be between 12 and 128 "
                                                 "characters long."));

                        return;
                    }

                    storage->setStorageFile(filePath);

                    if (const auto error = storage->initialize(true); error.isError()) {
                        // The technical message goes to the log, the dialog gets
                        // the short form. Opening a vault is what the user just
                        // asked for, so this one does block.
                        qCWarning(lcUiStorage) << "could not open the storage:" << error.message();

                        storage->close();
                        QMessageBox::critical(
                            q_ptr,
                            tr("Error"),
                            tr("Your data vault could not be opened. Check the password, or "
                               "restore a backup if the file is damaged."));

                        return;
                    }

                    if (!storage->isValid()) {
                        storage->close();
                        QMessageBox::critical(
                            q_ptr,
                            tr("Error"),
                            tr("Your data vault is corrupted and or not readable / writeable."));

                        return;
                    }

                    disconnect(storage, &Storage::itemsReceived, nullptr, nullptr);
                    connect(storage, &Storage::itemsReceived, q_ptr, [&](const BankingItems &items) {
                        app->setAccounts(items);
                    });

                    storage->receiveItems(Storage::StorageAccount);
                });

        connect(storageItem,
                &NewStorageItem::storageDeleted,
                q_ptr,
                [this](bool success, NewStorageItem *item, const QString &reason) {
                    if (!success) {
                        qCWarning(lcUiStorage) << "could not remove the storage file:" << reason;

                        QMessageBox::critical(q_ptr,
                                              tr("Storage"),
                                              tr("The data vault could not be removed. Check "
                                                 "the permissions on the file."));
                        return;
                    }

                    item->deleteLater();
                });

        storageContentsLayout->addWidget(storageItem);
    }

    void addNewStorageItem() {}
    void removeStorageItem() {}

    void addStorageInfo()
    {
        storageContentsLayout->addItem(scrollAreaSpacerTop);
        storageContentsLayout->addWidget(storageInfoLabel);
        storageContentsLayout->addItem(scrollAreaSpacerBottom);
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
        }
    }

    static QString dateFormat() { return QStringLiteral("dd.MM.yyyy hh:mm"); }

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
    const auto pos = d_ptr->storage
                         ->setting(QStringLiteral("Position"),
                                   QStringLiteral("StorageDialog"),
                                   QPoint())
                         .toPoint();
    if (!pos.isNull()) {
        move(pos);
    }

    const auto size = d_ptr->storage
                          ->setting(QStringLiteral("Size"), QStringLiteral("StorageDialog"), QSize())
                          .toSize();
    if (!size.isNull() && size.isValid()) {
        resize(size);
    }

    d_ptr->initialize(window);
    d_ptr->loadStorageItems();
}

void StorageDialog::reload()
{
    d_ptr->loadStorageItems();
}

void StorageDialog::moveEvent(QMoveEvent *event)
{
    d_ptr->storage->storeSetting(QStringLiteral("Position"),
                                 event->pos(),
                                 QStringLiteral("StorageDialog"));
    QWidget::moveEvent(event);
}

void StorageDialog::resizeEvent(QResizeEvent *event)
{
    d_ptr->storage->storeSetting(QStringLiteral("Size"),
                                 event->size(),
                                 QStringLiteral("StorageDialog"));
    QWidget::resizeEvent(event);
}

void StorageDialog::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::FontChange) {
        d_ptr->applyMetrics();
    }
}
