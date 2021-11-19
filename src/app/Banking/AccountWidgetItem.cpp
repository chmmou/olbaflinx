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
      mAccountId(0),
      mAccountName(""),
      mAccountType(""),
      mAccountImage(QPixmap())
{
    setupUi(this);
}

AccountWidgetItem::~AccountWidgetItem() = default;

void AccountWidgetItem::setAccountId(quint32 accountId)
{
    mAccountId = accountId;
}

quint32 AccountWidgetItem::accountId() const
{
    return mAccountId;
}

void AccountWidgetItem::setAccountName(const QString &accountName)
{
    mAccountName = accountName;
}

void AccountWidgetItem::setAccountType(const QString &accountType)
{
    mAccountType = accountType;
}

void AccountWidgetItem::setAccountImage(const QPixmap &accountImage)
{
    mAccountImage = accountImage;
}
