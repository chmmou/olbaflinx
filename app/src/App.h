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
#ifndef OLBAFLINX_APP_APP_H
#define OLBAFLINX_APP_APP_H

#include <Banking/BankingItem.h>

#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>

using namespace olbaflinx::core::banking;

namespace olbaflinx::app {

class App : public QMainWindow
{
    Q_OBJECT

public:
    explicit App(QWidget *parent = Q_NULLPTR, const Qt::WindowFlags &flags = Qt::WindowFlags());
    ~App() override;

    void initialize(const QApplication *app);

    void setAccounts(const QList<BankingItem *> &items);

protected:
    bool event(QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    class Private;
    Private *d_ptr; ///< private data (pimpl)
};

} // namespace olbaflinx::app

#endif //OLBAFLINX_APP_APP_H
