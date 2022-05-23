/**
 * Original code licensed under MIT - https://github.com/sthlm58/QtMaterialDesignIcons
 *
 * Copyright (C) 2021-2022, Alexander Saal <developer@olbaflinx.chm-projects.de>
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

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtGui/QPainter>
#include <QtSvg/QSvgRenderer>
#include <QtXml/QXmlDefaultHandler>

#include <mutex>

#include "MaterialDesign.h"

using namespace olbaflinx::core::material::design;

QPixmap MaterialDesign::icon(const QString &name, const QSize &size, const QColor &color)
{
    static QHash<QString, QString> svgPaths;
    static std::once_flag runOnce;

    std::call_once(runOnce, [&] {
        class SvgGlyphReader : public QXmlDefaultHandler
        {
        public:
            bool startElement(const QString &namespaceURI,
                              const QString &localName,
                              const QString &qName,
                              const QXmlAttributes &atts) override
            {
                if (localName != "glyph") {
                    return true;
                }

                const auto glyphName = atts.value("glyph-name");
                const auto pathData = atts.value("d");

                if (glyphName == ".notdef") {
                    return true;
                }

                if (glyphName.isEmpty() || pathData.isEmpty()) {
                    qDebug() << QObject::tr("glyphName or SVG path data not found in the SVG Font");
                    return false;
                }

                if (!svgPaths.contains(glyphName)) {
                    svgPaths.insert(glyphName, pathData);
                }

                return true;
            }
        };

        SvgGlyphReader glyphReader;
        QXmlSimpleReader xmlParser;
        xmlParser.setContentHandler(&glyphReader);

        QFile svgFont(":/fonts/materialdesignicons-webfont-svg");
        svgFont.open(QFile::ReadOnly);
        QXmlInputSource xmlSource(&svgFont);

        bool parse_ok = xmlParser.parse(&xmlSource);
        if (!parse_ok) {
            qDebug() << QObject::tr("SVG parsing failed! Something changed in the SVG Font?");
        }
    });

    QPixmap pix(size);
    pix.fill(Qt::transparent);

    const auto svg = [](const QString &path, QColor color) {
        // The magic part is to do the proper adjustments according to how the original SVG font is
        // created (i.e. do some offsets, view-box correction, translate & scale, and color fill on
        // top of that)
        return QString(
                   R"(<?xml version="1.0" encoding="utf-8"?>)"
                   R"(<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">)"
                   R"(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink")"
                   R"( version="1.1" baseProfile="full" viewBox="0 -64 512 512" xml:space="preserve">)"
                   R"-(<path transform="translate(0, 384), scale(1, -1)" fill="%1" d="%2"/>)-"
                   R"(</svg>)")
            .arg(color.name())
            .arg(path)
            .toLocal8Bit();
    };

    QPainter painter(&pix);
    QSvgRenderer svgRenderer;
    svgRenderer.load(svg(svgPaths[name], color));
    svgRenderer.render(&painter);

    return pix;
}
