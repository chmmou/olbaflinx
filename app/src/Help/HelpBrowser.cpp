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
#include "HelpBrowser.h"

#include <QtCore/QFile>

#include <QtGui/QTextDocument>

#include <QtHelp/QHelpEngineCore>
#include <QtHelp/QHelpLink>

#include <QtWidgets/QApplication>

using namespace olbaflinx::app::help;

class HelpBrowser::Private
{
public:
    explicit Private(HelpBrowser *browser)
        : helpEngine(nullptr)
        , q_ptr(browser)
    {
        q_ptr->setMinimumSize(QSize(800, 600));
        q_ptr->setAttribute(Qt::WA_DeleteOnClose, true);

        const auto appPath = QApplication::applicationDirPath();

        QFile qhcFile(":/help/qhc");
        qhcFile.copy(appPath + "/OlbaFlinxApp.qhc");

        QFile qchFile(":/help/qch");
        qchFile.copy(appPath + "/OlbaFlinxApp.qch");

        helpEngine = new QHelpEngineCore(appPath + "/OlbaFlinxApp.qhc", browser);
        if (!helpEngine->setupData()) {
            delete helpEngine;
            helpEngine = nullptr;
        }
    }

    ~Private()
    {
        delete helpEngine;
        helpEngine = nullptr;
    }

    void showHelpForKeyword(const QString &id)
    {
        if (helpEngine != nullptr) {
            auto documents = helpEngine->documentsForIdentifier(id);
            if (documents.count()) {
                q_ptr->setSource(documents.first().url);
            }
            documents.clear();
        }
    }

    QVariant loadResource(int type, const QUrl &name)
    {
        QByteArray ba = {};

        if (helpEngine != nullptr) {
            QUrl url(name);

            switch (type) {
            case QTextDocument::UnknownResource:
            case QTextDocument::HtmlResource:
            case QTextDocument::ImageResource:
            case QTextDocument::StyleSheetResource:
                if (name.isRelative()) {
                    url = q_ptr->source().resolved(url);
                }
                break;
            default:
                return helpEngine->fileData(url);
            }

            ba = helpEngine->fileData(url);
        }

        return ba;
    }

private:
    QHelpEngineCore *helpEngine;
    HelpBrowser *q_ptr;
};

HelpBrowser::HelpBrowser(QWidget *parent)
    : QTextBrowser(parent)
    , d_ptr(new Private(this))
{}

HelpBrowser::~HelpBrowser()
{
    delete d_ptr;
}

void HelpBrowser::showHelpForKeyword(const QString &id)
{
    d_ptr->showHelpForKeyword(id);
    show();
}

QVariant HelpBrowser::loadResource(int type, const QUrl &name)
{
    return d_ptr->loadResource(type, name);
}

void HelpBrowser::closeEvent(QCloseEvent *event)
{
    delete d_ptr;
    d_ptr = nullptr;

    QTextBrowser::closeEvent(event);
}
