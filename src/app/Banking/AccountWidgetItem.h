/**
 * Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
#ifndef OLBAFLINX_ACCOUNTWIDGETITEM_H
#define OLBAFLINX_ACCOUNTWIDGETITEM_H

#include <QtGui/QPixmap>
#include <QtWidgets/QWidget>

#include "ui_AccountWidgetItem.h"

namespace olbaflinx::app::banking
{

class AccountWidgetItem: public QWidget, private Ui::UiAccountWidgetItem
{

Q_OBJECT

public:
    explicit AccountWidgetItem(QWidget *parent = nullptr);
    ~AccountWidgetItem() override;

    void setId(quint32 accountId);
    [[nodiscard]] quint32 id() const;

    void setName(const QString &accountName);
    void setType(const QString &accountType);
    void setBalance(const QString &accountType);
    void setImage(const QPixmap &accountImage);


private:
    quint32 mAccountId;
};

} // olbflinx::app::banking

Q_DECLARE_METATYPE(olbaflinx::app::banking::AccountWidgetItem *)

#endif // OLBAFLINX_ACCOUNTWIDGETITEM_H
