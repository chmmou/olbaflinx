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

#include "ui/Themes/ThemeManager.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QObject>

#include <QtGui/QPainter>

#include <QtSvg/QSvgRenderer>

using namespace olbaflinx::ui::themes;

class ThemeManager::Private
{
public:
    explicit Private(ThemeManager *manager)
        : q_ptr(manager) {};

    ~Private()
    {
        themes.clear();

        application = Q_NULLPTR;
        q_ptr = Q_NULLPTR;
    }

    void registerTheme(const QString &name)
    {
        if (const auto themKey = themeName(name); !themes.contains(themKey)) {
            themes[themKey] = name;
        }
    }

    void unregisterTheme(const QString &name)
    {
        if (const auto themKey = themeName(name); !themes.contains(themKey)) {
            themes.remove(themKey);
        }
    }

    void reloadTheme()
    {
        if (!application) {
            return;
        }

        application->setStyleSheet({});

        for (auto i = themes.cbegin(), end = themes.cend(); i != end; ++i) {
            if (QFile file(themes[i.value()]); file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                application->setStyleSheet(file.readAll());
                file.close();
            }
        }
    }

    void applyTheme(const QApplication *app, const QString &name)
    {
        registerTheme(name);

        if (!application) {
            application = const_cast<QApplication *>(app);
        }

        if (const auto themKey = themeName(name); themes.contains(themKey)) {
            if (QFile file(themes[themKey]); file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                application->setStyleSheet(file.readAll());
                file.close();
            }
        }
    }

private:
    [[nodiscard]] QString themeName(const QString &filename)
    {
        const QFileInfo fileInfo(filename);
        return fileInfo.baseName();
    }

    QHash<QString, QString> themes = {};
    QApplication *application = Q_NULLPTR;

    friend class ThemeManager;
    ThemeManager *q_ptr = Q_NULLPTR;
};

ThemeManager::ThemeManager()
    : QObject(Q_NULLPTR)
    , d_ptr(new Private(this))
{}

ThemeManager::~ThemeManager()
{
    delete d_ptr;
}

void ThemeManager::reload() const
{
    d_ptr->reloadTheme();
}

void ThemeManager::apply(const QApplication *application, const QString &filename) const
{
    d_ptr->applyTheme(application, filename);
}

QPixmap ThemeManager::pixmap(const QString &name, const ThemeManager::Mode mode)
{
    const auto filenameFormat = ":/icons/%1/%2";
    auto filename = QString();

    switch (mode) {
    case Mode::Dark:
        filename = QString(filenameFormat).arg("dark", name);
        break;
    case Mode::Light:
        filename = QString(filenameFormat).arg("light", name);
        break;
    default:
        filename = QString(filenameFormat).arg("light", name);
    }

    const auto svg = [](const QByteArray &contents) -> QByteArray {
        return QString(
                   R"(<?xml version="1.0" encoding="utf-8"?>)"
                   R"(<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">)"
                   R"(%1)")
            .arg(contents)
            .toLocal8Bit();
    };

    if (QFile svgFile(filename); svgFile.open(QIODevice::ReadOnly)) {
        QSvgRenderer svgRenderer;
        if (svgRenderer.load(svg(svgFile.readAll()))) {
            qDebug() << svgRenderer.isValid();
            qDebug() << svgRenderer.defaultSize();

            QImage pix(svgRenderer.defaultSize(), QImage::Format_ARGB6666_Premultiplied);
            //pix.fill(Qt::transparent);

            auto painter = QPainter(&pix);
            painter.save();
            painter.setRenderHints(QPainter::Antialiasing | QPainter::LosslessImageRendering, true);
            svgRenderer.render(&painter, pix.rect());
            painter.restore();

            svgFile.close();

            return QPixmap::fromImage(pix);
        }
    }

    return {};
}
