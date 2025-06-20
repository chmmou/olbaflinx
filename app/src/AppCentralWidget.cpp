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
#include "App.h"

#include "AppCentralWidget.h"

#include "ui_AppCentralWidget.h"

#include <Themes/ThemeManager.h>
#include <Themes/ThemeManagerIconNames.h>

#include <QtWidgets/QLabel>

using namespace olbaflinx::app;
using namespace olbaflinx::core::themes;

class AppCentralWidget::Private
{
public:
    explicit Private(AppCentralWidget *widget)
        : app(nullptr)
        , q_ptr(widget)
        , ui(new Ui::UiAppCentralWidget)
    {
        ui->setupUi(q_ptr);
    }

    ~Private() = default;

    void initialize(QMainWindow *window)
    {
        app = qobject_cast<App *>(window);

        ui->label->setPixmap(ThemeManager::pixmap(ThemeManagerIconNames::AlertTriangle));
    }

    Ui::UiAppCentralWidget *ui;

private:
    App *app;
    AppCentralWidget *q_ptr;
};

AppCentralWidget::AppCentralWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
    , d_ptr(new Private(this))
{}

AppCentralWidget::~AppCentralWidget()
{
    delete d_ptr;
}

void AppCentralWidget::initialize(QMainWindow *window)
{
    d_ptr->initialize(window);
}

QTreeWidget *AppCentralWidget::accountWidget() const
{
    return Q_NULLPTR;
}
