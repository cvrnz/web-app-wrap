#include "iconrenderer.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QImageReader>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <algorithm>
#include <cmath>

namespace {

constexpr double canvas = 1024;
// Omarchy's icons are rounded squares with corners about 22% of the side.
constexpr double cornerRatio = 0.22;
constexpr double maximumAutomaticZoom = 3;

QPainterPath tileShape() {
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, canvas, canvas), canvas * cornerRatio, canvas * cornerRatio);
    return path;
}

QRectF scaledAround(const QRectF &rect, double zoom) {
    const QSizeF size = rect.size() * zoom;
    return QRectF(rect.center().x() - size.width() / 2, rect.center().y() - size.height() / 2, size.width(), size.height());
}

QRectF fitted(const QSizeF &content, const QRectF &into, bool fill) {
    if (content.isEmpty())
        return into;
    const double ratio = fill ? std::max(into.width() / content.width(), into.height() / content.height())
                              : std::min(into.width() / content.width(), into.height() / content.height());
    const QSizeF size = content * ratio;
    return QRectF(into.center().x() - size.width() / 2, into.center().y() - size.height() / 2, size.width(), size.height());
}

// djb2, so the same name always gets the same color, as on macOS.
quint64 djb2(const QString &text) {
    quint64 hash = 5381;
    for (const char byte : text.toLower().toUtf8())
        hash = hash * 33 + quint8(byte);
    return hash;
}

void drawGenerated(QPainter &painter, const QString &name) {
    const double hue = double(djb2(name) % 360) / 360;
    QLinearGradient gradient(0, 0, 0, canvas);
    gradient.setColorAt(0, QColor::fromHsvF(hue, 0.55, 0.98));
    gradient.setColorAt(1, QColor::fromHsvF(std::fmod(hue + 0.06, 1.0), 0.8, 0.74));
    painter.fillRect(QRectF(0, 0, canvas, canvas), gradient);

    const QString trimmed = name.trimmed();
    const QString letter = trimmed.isEmpty() ? QStringLiteral("?") : trimmed.left(1).toUpper();
    QFont font;
    font.setPixelSize(560);
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.setPen(Qt::white);
    // Center the capital height, not the line box, so the letter sits in the optical middle.
    const QFontMetricsF metrics(font);
    const double width = metrics.horizontalAdvance(letter);
    const double baseline = canvas / 2 + metrics.capHeight() / 2;
    painter.drawText(QPointF(canvas / 2 - width / 2, baseline), letter);
}

void drawSvg(QPainter &painter, const IconSource &source, int pixels) {
    painter.fillRect(QRectF(0, 0, canvas, canvas), source.background.toQColor());
    QSvgRenderer renderer(source.svg);
    if (!renderer.isValid())
        return;
    const QRectF tile(0, 0, canvas, canvas);
    QSizeF size = renderer.viewBoxF().size();
    if (size.isEmpty())
        size = renderer.defaultSize();
    const QRectF target = fitted(size, scaledAround(tile, std::max(source.zoom, 0.01)), false);
    if (!source.color) {
        renderer.render(&painter, target);
        return;
    }
    // Draw the SVG on its own layer, then paint the color over just its pixels, keeping the
    // transparency of its edges.
    QImage layer(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);
    QPainter layerPainter(&layer);
    layerPainter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    layerPainter.scale(pixels / canvas, pixels / canvas);
    renderer.render(&layerPainter, target);
    layerPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    layerPainter.fillRect(tile, source.color->toQColor());
    layerPainter.end();
    painter.drawImage(tile, layer);
}

}

IconSource IconSource::generated(const QString &name) {
    IconSource source;
    source.kind = Kind::Generated;
    source.name = name;
    return source;
}

bool IconSource::isSvg(const QString &path) {
    return QFileInfo(path).suffix().compare("svg", Qt::CaseInsensitive) == 0;
}

std::optional<IconSource> IconSource::load(const QString &path, double zoom, std::optional<HexColor> background,
                                           std::optional<HexColor> color, QString *error) {
    auto failWith = [&](const QString &message) {
        if (error)
            *error = message;
        return std::nullopt;
    };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return failWith(QCoreApplication::translate("IconSource", "Couldn’t open %1.").arg(path));
    IconSource source;
    if (isSvg(path)) {
        source.kind = Kind::Svg;
        source.svg = file.readAll();
        if (!QSvgRenderer(source.svg).isValid())
            return failWith(QCoreApplication::translate("IconSource", "%1 isn’t an SVG WebAppWrap can read.").arg(path));
        source.zoom = std::clamp(zoom, 0.5, 3.0);
        source.background = background.value_or(HexColor::defaultIconBackground());
        source.color = color;
        return source;
    }
    QImageReader reader(&file);
    reader.setAutoTransform(true);
    source.image = reader.read();
    if (source.image.isNull())
        return failWith(QCoreApplication::translate("IconSource", "%1 isn’t an image WebAppWrap can read.").arg(path));
    source.kind = Kind::Image;
    source.zoom = IconRenderer::automaticZoom(source.image) * std::clamp(zoom, 1.0, 3.0);
    return source;
}

namespace IconRenderer {

QImage render(const IconSource &source, int pixels) {
    QImage image(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    painter.scale(pixels / canvas, pixels / canvas);
    painter.setClipPath(tileShape());

    switch (source.kind) {
    case IconSource::Kind::Generated:
        drawGenerated(painter, source.name);
        break;
    case IconSource::Kind::Svg:
        drawSvg(painter, source, pixels);
        break;
    case IconSource::Kind::Image: {
        const QRectF tile(0, 0, canvas, canvas);
        painter.fillRect(tile, Qt::white);
        painter.drawImage(fitted(source.image.size(), scaledAround(tile, std::max(source.zoom, 1.0)), true), source.image);
        break;
    }
    }
    painter.end();
    return image;
}

double automaticZoom(const QImage &original) {
    if (original.isNull())
        return 1;
    // Analyze at up to 1024 px on the long side.
    QImage image = original.convertToFormat(QImage::Format_ARGB32);
    if (std::max(image.width(), image.height()) > 1024)
        image = image.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const int width = image.width(), height = image.height();

    auto distance = [](QRgb p, QRgb q) {
        return std::max({std::abs(qRed(p) - qRed(q)), std::abs(qGreen(p) - qGreen(q)),
                         std::abs(qBlue(p) - qBlue(q)), std::abs(qAlpha(p) - qAlpha(q))});
    };
    const QRgb corners[] = {image.pixel(0, 0), image.pixel(width - 1, 0), image.pixel(0, height - 1),
                            image.pixel(width - 1, height - 1)};
    const QRgb corner = corners[0];
    const bool cornerIsTransparent = std::all_of(std::begin(corners), std::end(corners), [](QRgb c) { return qAlpha(c) < 128; });
    if (!cornerIsTransparent && !std::all_of(std::begin(corners), std::end(corners), [&](QRgb c) { return distance(c, corner) <= 24; }))
        return 1;

    // Anything within about 40% of the corner color counts as border, which also catches the
    // anti-aliased ring and any soft shadow where the border meets the artwork.
    auto isBackground = [&](int x, int y) {
        const QRgb pixel = image.pixel(x, y);
        return qAlpha(pixel) < 128 || (!cornerIsTransparent && distance(pixel, corner) < 100);
    };

    // Points on and just outside the tile's outline, as offsets from the center where the
    // tile's half-width is 1. The tile is aspect-filled, so at zoom 1 it spans half the shorter side.
    QList<QPointF> outline;
    const QPainterPath shape = tileShape();
    for (const double grow : {1.0, 1.02})
        for (int step = 0; step < 720; ++step) {
            const QPointF p = shape.pointAtPercent(step / 720.0);
            outline << QPointF((p.x() / canvas * 2 - 1) * grow, (p.y() / canvas * 2 - 1) * grow);
        }
    const double centerX = width / 2.0, centerY = height / 2.0, halfExtent = std::min(width, height) / 2.0;
    auto isClear = [&](double zoom) {
        const double reach = halfExtent / zoom;
        for (const QPointF &point : outline) {
            const int x = std::clamp(int(centerX + point.x() * reach), 0, width - 1);
            const int y = std::clamp(int(centerY + point.y() * reach), 0, height - 1);
            if (isBackground(x, y))
                return false;
        }
        return true;
    };

    if (isClear(1))
        return 1;
    if (!isClear(maximumAutomaticZoom))
        return 1;
    double low = 1, high = maximumAutomaticZoom;
    for (int i = 0; i < 20; ++i) {
        const double middle = (low + high) / 2;
        (isClear(middle) ? high : low) = middle;
    }
    // A little extra so no faint line survives at small sizes.
    return std::min(maximumAutomaticZoom, high * 1.02);
}

}
