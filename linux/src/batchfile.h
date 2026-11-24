#pragma once

#include <QByteArray>
#include <QDir>
#include <QList>
#include <QString>
#include <QUrl>
#include <optional>

#include "hexcolor.h"
#include "inputnormalizer.h"

// One app in a batch file, in the same format as on macOS and Windows:
//
//     [
//       {
//         "name": "WebAppWrap/Photos",
//         "url": "https://kphotos.vercel.app",
//         "image": "icons/photos.svg",
//         "zoom": 0.8,
//         "iconBackground": "#ffffff",
//         "iconColor": "#e5e5e5",
//         "titleBarColor": "#1a1a1a"
//       }
//     ]
//
// Only `name` and `url` are required. `image` is relative to the batch file unless absolute
// (`~` works). An SVG is drawn on `iconBackground` (default #1C1C1C) and `zoom` (0.5–3) scales
// it, where 1 fits the whole SVG. `iconColor` fills the SVG with one color. Other images are
// cropped to fill the icon. `titleBarColor` defaults to the site's theme color.
struct BatchEntry {
    AppLocation location;
    QUrl url;
    QString image; // absolute path, or empty for the default icon
    double zoom = 1;
    std::optional<HexColor> iconBackground;
    std::optional<HexColor> iconColor;
    std::optional<HexColor> titleBarColor;
};

namespace BatchFile {
constexpr double minimumZoom = 0.5;
constexpr double maximumZoom = 3;

// Every entry, or nothing with `error` listing every problem found, one per line, so a batch
// never half-runs because of a typo in its last entry.
std::optional<QList<BatchEntry>> parse(const QByteArray &data, const QDir &baseDirectory, QString *error);

// "~/x" and "~" become paths in the home folder; relative paths resolve against `base`.
QString resolvePath(const QString &path, const QDir &base);
}
