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

#include "ui/Themes/ThemeManager.h"

#include "ui/Logging.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMap>
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

        application = nullptr;
        q_ptr = nullptr;
    }

    void registerTheme(const QString &name)
    {
        if (const auto themKey = themeName(name); !themes.contains(themKey)) {
            themes[themKey] = name;
        }
    }

    void unregisterTheme(const QString &name)
    {
        // The condition used to ask for the absence of the key before removing
        // it, so a registered theme could never be taken out again.
        if (const auto themKey = themeName(name); themes.contains(themKey)) {
            themes.remove(themKey);
        }
    }

    /**
     * Reads every registered theme and hands the lot to the application at once.
     *
     * The loop used to look the value of the iterator up as if it were a key,
     * which answers with an empty path, so its body never opened a file. Each
     * round also replaced the whole style sheet rather than adding to it, which
     * left only the last theme in effect.
     */
    void reloadTheme()
    {
        if (!application) {
            return;
        }

        auto styleSheet = QString();

        for (auto i = themes.cbegin(), end = themes.cend(); i != end; ++i) {
            styleSheet += readTheme(i.value());
        }

        application->setStyleSheet(styleSheet);
    }

    void applyTheme(QApplication *app, const QString &name)
    {
        registerTheme(name);

        if (!application) {
            application = app;
        }

        const auto themKey = themeName(name);
        if (!themes.contains(themKey)) {
            return;
        }

        // A theme that cannot be read leaves the one in force alone. Setting the
        // empty result would strip the application of the style it already had.
        if (const auto styleSheet = readTheme(themes.value(themKey)); !styleSheet.isEmpty()) {
            application->setStyleSheet(styleSheet);
        }
    }

private:
    /**
     * A theme that cannot be read costs its own rules, not those of the others.
     * The failure used to pass without a word.
     */
    QString readTheme(const QString &path) const
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(lcUiThemes) << "could not read the theme" << path << file.errorString();
            return {};
        }

        return QString::fromUtf8(file.readAll());
    }

    [[nodiscard]] QString themeName(const QString &filename)
    {
        const QFileInfo fileInfo(filename);
        return fileInfo.baseName();
    }

    // A map, not a hash. The themes are concatenated, so the order they are read
    // in decides which rule wins, and the order of a hash is not defined.
    QMap<QString, QString> themes = {};
    QApplication *application = nullptr;

    friend class ThemeManager;
    ThemeManager *q_ptr = nullptr;
};

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
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

void ThemeManager::apply(QApplication *application, const QString &filename) const
{
    d_ptr->applyTheme(application, filename);
}

QPixmap ThemeManager::pixmap(const QString &name, const ThemeManager::Mode mode)
{
    const auto filenameFormat = QStringLiteral(":/icons/%1/%2");
    auto filename = QString();

    // No default branch. It turned off the warning about an unhandled value, was
    // word for word the same as the branch for Light, and had no break.
    switch (mode) {
    case Mode::Dark:
        filename = filenameFormat.arg(QStringLiteral("dark"), name);
        break;
    case Mode::Light:
        filename = filenameFormat.arg(QStringLiteral("light"), name);
        break;
    }

    // The prologue is joined on at byte level. Going through QString meant a
    // decode and an encode, the second of them in the locale of the user, which
    // contradicted the encoding the declaration itself names.
    const auto svg = [](const QByteArray &contents) -> QByteArray {
        return QByteArrayLiteral(
                   R"(<?xml version="1.0" encoding="utf-8"?>)"
                   R"(<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">)")
               + contents;
    };

    if (QFile svgFile(filename); svgFile.open(QIODevice::ReadOnly)) {
        QSvgRenderer svgRenderer;
        if (svgRenderer.load(svg(svgFile.readAll()))) {
            qCDebug(lcUiThemes) << "rendering" << name << "at" << svgRenderer.defaultSize();

            QImage pix(svgRenderer.defaultSize(), QImage::Format_ARGB32_Premultiplied);
            // Without this the pixels the renderer does not touch keep whatever
            // the allocation left behind.
            pix.fill(Qt::transparent);

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
