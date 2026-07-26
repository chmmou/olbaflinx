/**
 * Copyright (C) 2022-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

class StorageDialog::Private
{
public:
    explicit Private(StorageDialog *storageDialog)
        : storage(Storage::instance())
        , q_ptr(storageDialog)
        , app(nullptr)
        , scrollAreaSpacerTop(nullptr)
        , scrollAreaSpacerBottom(nullptr)
        , storageContentsLayout(nullptr)
        , btnNewStorageItem(nullptr)
        , storageInfoLabel(nullptr)
    {
        q_ptr->setMinimumSize(QSize(930, 646));
    }

    ~Private()
    {
        storage->close();
        storage->deleteLater();
    }

    void initialize(QMainWindow *window)
    {
        app = qobject_cast<App *>(window);

        auto verticalLayoutDataVaults = new QVBoxLayout(q_ptr);
        verticalLayoutDataVaults->setSpacing(0);
        verticalLayoutDataVaults->setObjectName("verticalLayoutDataVaults");
        verticalLayoutDataVaults->setContentsMargins(0, 0, 0, 0);

        auto widgetStorageHeader = new QWidget(q_ptr);
        widgetStorageHeader->setObjectName("widgetStorageHeader");
        widgetStorageHeader->setMinimumSize(QSize(0, 82));

        auto hlStorageWidgetInfo = new QHBoxLayout(widgetStorageHeader);
        hlStorageWidgetInfo->setSpacing(12);
        hlStorageWidgetInfo->setObjectName("hlStorageWidgetInfo");

        auto lblStorageInfoIcon = new QLabel(widgetStorageHeader);
        lblStorageInfoIcon->setObjectName("lblStorageInfoIcon");
        lblStorageInfoIcon->setMinimumSize(QSize(64, 64));
        lblStorageInfoIcon->setMaximumSize(QSize(64, 64));
        lblStorageInfoIcon->setPixmap(QPixmap(QString::fromUtf8(":/app/olbaflinx-logo-128")));
        lblStorageInfoIcon->setScaledContents(true);

        hlStorageWidgetInfo->addWidget(lblStorageInfoIcon);

        auto lblStorageInfoTitle = new QLabel(widgetStorageHeader);
        lblStorageInfoTitle->setObjectName("lblStorageInfoTitle");
        lblStorageInfoTitle->setAlignment(Qt::AlignCenter);
        lblStorageInfoTitle->setText(tr("OlbaFlinx - Online Banking For Linux"));

        hlStorageWidgetInfo->addWidget(lblStorageInfoTitle);

        verticalLayoutDataVaults->addWidget(widgetStorageHeader);

        auto scrollAreaStorage = new QScrollArea(q_ptr);
        scrollAreaStorage->setObjectName("scrollAreaStorage");
        scrollAreaStorage->setWidgetResizable(true);

        auto scrollAreaStorageContents = new QWidget();
        scrollAreaStorageContents->setObjectName("scrollAreaStorageContents");
        scrollAreaStorageContents->setGeometry(QRect(0, 0, 928, 489));

        scrollAreaStorage->setWidget(scrollAreaStorageContents);
        scrollAreaStorage->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollAreaStorage->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollAreaStorage->setFrameShape(QScrollArea::NoFrame);
        scrollAreaStorage->setFrameShadow(QScrollArea::Plain);

        verticalLayoutDataVaults->addWidget(scrollAreaStorage);

        auto hlStoragePage = new QHBoxLayout();
        hlStoragePage->setObjectName("hlStoragePage");
        hlStoragePage->setContentsMargins(-1, 5, 5, 5);
        auto hsStoragePage = new QSpacerItem(40,
                                             20,
                                             QSizePolicy::Policy::Expanding,
                                             QSizePolicy::Policy::Minimum);

        hlStoragePage->addItem(hsStoragePage);

        btnNewStorageItem = new QPushButton(q_ptr);
        btnNewStorageItem->setObjectName("btnNewStorageItem");

        QIcon icon;
        icon.addFile(QString::fromUtf8(":/datavault/add"), QSize(), QIcon::Normal, QIcon::Off);

        btnNewStorageItem->setIcon(icon);
        btnNewStorageItem->setFlat(true);
        btnNewStorageItem->setShortcut(QKeySequence("Ctrl+N"));
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
        auto items = storage->setting("Paths", "Items", QStringList()).toStringList();
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
        auto storageItem = new NewStorageItem();
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
                    connect(storage,
                            &Storage::itemsReceived,
                            q_ptr,
                            [&](const QList<BankingItem *> &items) {
                                app->setAccounts(items);
                            });

                    storage->receiveItems(Storage::StorageAccount);
                });

        connect(storageItem,
                &NewStorageItem::storageDeleted,
                q_ptr,
                [](bool success, NewStorageItem *item, const QString &errorMessage) {
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

    static QString dateFormat() { return "dd.MM.yyyy hh:mm"; }

    StorageDialog *q_ptr;
    App *app;

    QSpacerItem *scrollAreaSpacerTop;
    QSpacerItem *scrollAreaSpacerBottom;
    QVBoxLayout *storageContentsLayout;
    QPushButton *btnNewStorageItem;
    QLabel *storageInfoLabel;
};

StorageDialog::StorageDialog(QWidget *parent)
    : QWidget(parent)
    , d_ptr(new Private(this))
{}

StorageDialog::~StorageDialog()
{
    delete d_ptr;
}

void StorageDialog::initialize(QMainWindow *window)
{
    const auto pos = d_ptr->storage->setting("Position", "StorageDialog", QPoint()).toPoint();
    if (!pos.isNull()) {
        move(pos);
    }

    const auto size = d_ptr->storage->setting("Size", "StorageDialog", QSize()).toSize();
    if (!size.isNull() && size.isValid()) {
        resize(size);
    }

    d_ptr->initialize(window);
    d_ptr->loadStorageItems();
}

void StorageDialog::moveEvent(QMoveEvent *event)
{
    d_ptr->storage->storeSetting("Position", event->pos(), "StorageDialog");
    QWidget::moveEvent(event);
}

void StorageDialog::resizeEvent(QResizeEvent *event)
{
    d_ptr->storage->storeSetting("Size", event->size(), "StorageDialog");
    QWidget::resizeEvent(event);
}
