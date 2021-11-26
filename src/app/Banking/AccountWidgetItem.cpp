/**
 * Copyright (c) 2021, Alexander Saal <alexander.saal@chm-projects.de>
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

#include "AccountWidgetItem.h"

using namespace olbaflinx::app::banking;

AccountWidgetItem::AccountWidgetItem(QWidget *parent)
    : QWidget(parent),
      mAccountId(0)
{
    setupUi(this);
}

AccountWidgetItem::~AccountWidgetItem() = default;

void AccountWidgetItem::setId(quint32 accountId)
{
    mAccountId = accountId;
}

quint32 AccountWidgetItem::id() const
{
    return mAccountId;
}

void AccountWidgetItem::setName(const QString &accountName)
{
    labelName->setText(accountName);
}

void AccountWidgetItem::setType(const QString &accountType)
{
    labelType->setText(accountType);
}

void AccountWidgetItem::setImage(const QPixmap &accountImage)
{
    labelLogo->setPixmap(accountImage);
}
