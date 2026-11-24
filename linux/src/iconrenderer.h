#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <optional>

#include "hexcolor.h"

// What an app's icon is drawn from.
struct IconSource {
    enum class Kind {
        Generated, // a colored gradient tile with the name's first letter
        Svg,       // an SVG on a solid background
        Image,     // any other image, cropped to fill the tile
    };

    Kind kind = Kind::Generated;
    QString name;        // Generated: the letter and the hue come from it
    QByteArray svg;      // Svg
    QImage image;        // Image
    // Svg: 0.5–3, where 1 fits the whole SVG in the tile. Image: 1–3, on top of the automatic
    // zoom that crops off a border.
    double zoom = 1;
    HexColor background = HexColor::defaultIconBackground();
    std::optional<HexColor> color; // Svg: fills every visible part with one color

    static IconSource generated(const QString &name);
    // Reads an image file the way the window draws it: an SVG on `background`, in `color` if
    // given, and anything else with the automatic border crop. None with `error` set if the
    // file can't be read.
    static std::optional<IconSource> load(const QString &path, double zoom = 1,
                                          std::optional<HexColor> background = std::nullopt,
                                          std::optional<HexColor> color = std::nullopt,
                                          QString *error = nullptr);
    static bool isSvg(const QString &path);
};

// Draws icons as a rounded square that fills the canvas, like Omarchy's own app icons.
namespace IconRenderer {
QImage render(const IconSource &source, int pixels);
// The smallest zoom at which the tile shows only the image's artwork, not the plain or
// transparent border around it. 1 when there's no such border or cropping wouldn't help.
double automaticZoom(const QImage &image);
}
