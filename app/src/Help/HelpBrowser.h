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
#ifndef OLBAFLINX_PROJECT_HELP_BROWSER_H
#define OLBAFLINX_PROJECT_HELP_BROWSER_H

#include <QtWidgets/QTextBrowser>

namespace olbaflinx::app::help {

class HelpBrowser : public QTextBrowser
{
    Q_OBJECT

public:
    explicit HelpBrowser(QWidget *parent = nullptr);
    ~HelpBrowser() override;

    void showHelpForKeyword(const QString &id);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    class Private;
    Private *d_ptr;

    QVariant loadResource(int type, const QUrl &name) override;
};

} // namespace olbaflinx::app::help

#endif //OLBAFLINX_PROJECT_HELP_BROWSER_H
