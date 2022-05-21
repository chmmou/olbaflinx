/**
 * Original code adapted from
 * https://codereview.qt-project.org/c/pyside/pyside-setup/+/335828/2/examples/widgets/icons/fonticons/fonticons.py
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
#include <QtGui/QPainter>
#include <QtSvg/QSvgRenderer>
#include <QtXml/QXmlDefaultHandler>

#include <mutex>

#include "MaterialDesign.h"

using namespace olbaflinx::core::material::design;

QPixmap MaterialDesign::icon(const QString &name, const QSize &size, const QColor &color)
{
    static QHash<QString, QString> svg_paths;
    static std::once_flag runOnce;

    std::call_once(runOnce, [&] {
        class SvgGlyphReader : public QXmlDefaultHandler
        {
        public:
            bool startElement(const QString &namespace_uri,
                              const QString &local_name,
                              const QString &qualified_name,
                              const QXmlAttributes &atts) override
            {
                if (local_name != "glyph") {
                    return true;
                }

                auto glyph_name = atts.value("glyph-name");
                auto path_data = atts.value("d");

                if (glyph_name == ".notdef") {
                    return true;
                }

                if (glyph_name.isEmpty() || path_data.isEmpty()) {
                    qDebug() << "glyph_name or SVG path data not found in the SVG Font";
                    return false;
                }

                if (!svg_paths.contains(glyph_name)) {
                    svg_paths.insert(glyph_name, path_data);
                }

                return true;
            }
        };

        SvgGlyphReader glyph_reader;
        QXmlSimpleReader xml_parser;
        xml_parser.setContentHandler(&glyph_reader);

        QFile svg_font(":/fonts/materialdesignicons-webfont-svg");
        svg_font.open(QFile::ReadOnly);
        QXmlInputSource xml_source(&svg_font);

        bool parse_ok = xml_parser.parse(&xml_source);
        if (!parse_ok) {
            qDebug() << "SVG parsing failed! Something changed in the SVG Font?";
        }
    });

    QPixmap pix(size);
    pix.fill(Qt::transparent);

    const auto svg = [](const QString &path, QColor color) {
        // The magic part is to do the proper adjustments according to how the original SVG font is created
        // (i.e. do some offsets, view-box correction, translate & scale, and color fill on top of that)
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
    QSvgRenderer svg_renderer;
    svg_renderer.load(svg(svg_paths[name], color));
    svg_renderer.render(&painter);

    return pix;
}
