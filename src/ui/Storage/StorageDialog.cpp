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
#include "ui/Storage/NewStorageItem.h"

#include "core/Banking/BankingItem.h"
#include "core/Storage/Storage.h"

#include <QtCore/QDateTime>
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
#include <QtWidgets/QWidget>

using namespace olbaflinx::ui;
using namespace olbaflinx::ui::storage;
using namespace olbaflinx::core::storage;

namespace {

// The outer dimensions of the dialog, the header height and the logo size derive
// from the font metrics, so that they hold at a different font size or scaling.
// The spacings and margins of the layouts below are still fixed pixel values.
// The factors approximate the previous fixed values at the default font; whether
// they reproduce them exactly has not been measured.
constexpr int DialogWidthInCharacters = 120;
constexpr int DialogHeightInLines = 30;
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
    {
        const QFontMetrics metrics(q_ptr->font());
        q_ptr->setMinimumSize(metrics.averageCharWidth() * DialogWidthInCharacters,
                              metrics.height() * DialogHeightInLines);
    }

    // The storage belongs to whoever created the dialog. When it is closed is
    // for the application to decide, not for a window.
    ~Private() = default;

    void initialize(QMainWindow *window)
    {
        app = qobject_cast<App *>(window);

        const QFontMetrics metrics(q_ptr->font());
        const int logoSize = metrics.height() * LogoSizeInLines;

        auto verticalLayoutDataVaults = new QVBoxLayout(q_ptr);
        verticalLayoutDataVaults->setSpacing(0);
        verticalLayoutDataVaults->setObjectName(QStringLiteral("verticalLayoutDataVaults"));
        verticalLayoutDataVaults->setContentsMargins(0, 0, 0, 0);

        auto widgetStorageHeader = new QWidget(q_ptr);
        widgetStorageHeader->setObjectName(QStringLiteral("widgetStorageHeader"));
        widgetStorageHeader->setMinimumSize(0, metrics.height() * HeaderHeightInLines);

        auto hlStorageWidgetInfo = new QHBoxLayout(widgetStorageHeader);
        hlStorageWidgetInfo->setSpacing(12);
        hlStorageWidgetInfo->setObjectName(QStringLiteral("hlStorageWidgetInfo"));

        auto lblStorageInfoIcon = new QLabel(widgetStorageHeader);
        lblStorageInfoIcon->setObjectName(QStringLiteral("lblStorageInfoIcon"));
        lblStorageInfoIcon->setMinimumSize(logoSize, logoSize);
        lblStorageInfoIcon->setMaximumSize(logoSize, logoSize);
        lblStorageInfoIcon->setPixmap(QPixmap(QStringLiteral(":/app/olbaflinx-logo-128")));
        lblStorageInfoIcon->setScaledContents(true);

        hlStorageWidgetInfo->addWidget(lblStorageInfoIcon);

        auto lblStorageInfoTitle = new QLabel(widgetStorageHeader);
        lblStorageInfoTitle->setObjectName(QStringLiteral("lblStorageInfoTitle"));
        lblStorageInfoTitle->setAlignment(Qt::AlignCenter);
        lblStorageInfoTitle->setText(tr("OlbaFlinx - Online Banking For Linux"));

        hlStorageWidgetInfo->addWidget(lblStorageInfoTitle);

        verticalLayoutDataVaults->addWidget(widgetStorageHeader);

        auto scrollAreaStorage = new QScrollArea(q_ptr);
        scrollAreaStorage->setObjectName(QStringLiteral("scrollAreaStorage"));
        scrollAreaStorage->setWidgetResizable(true);

        // QT-CPP-080: no parent on purpose. QScrollArea::setWidget takes over
        // ownership in the next line, a parent here would be undone right away.
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
        hlStoragePage->setContentsMargins(-1, 5, 5, 5);
        auto hsStoragePage = new QSpacerItem(40,
                                             20,
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
                    storage->setKey(password);
                    storage->setStorageFile(filePath);

                    (void) storage->initialize(true);

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

        // The third parameter carries the error message. It stays unused for
        // now and is therefore unnamed; acting on it belongs to the error
        // handling, which is not in place yet.
        connect(storageItem,
                &NewStorageItem::storageDeleted,
                q_ptr,
                [](bool success, NewStorageItem *item, const QString &) {
                    if (!success) {
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
