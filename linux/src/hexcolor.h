#pragma once

#include <QColor>
#include <QString>
#include <optional>

// An sRGB color stored as "#RRGGBB", as in the other platforms' app lists and batch files.
struct HexColor {
    double red = 0, green = 0, blue = 0;

    HexColor() = default;
    HexColor(double r, double g, double b);

    // The title bar color used when none is chosen and the site has no theme color.
    static HexColor defaultTitleBar() { return fromRgb(0x1C, 0x1C, 0x1C); }
    // The color behind an SVG icon when none is chosen.
    static HexColor defaultIconBackground() { return fromRgb(0x1C, 0x1C, 0x1C); }
    // The color offered when an SVG switches from its own colors to one color.
    static HexColor defaultSvgColor() { return fromRgb(0xE5, 0xE5, 0xE5); }

    static HexColor fromRgb(int r, int g, int b) { return HexColor(r / 255.0, g / 255.0, b / 255.0); }
    static HexColor fromQColor(const QColor &color);

    // "#1a1a1a", "1A1A1A" or the short form "#fff".
    static std::optional<HexColor> fromHex(const QString &text);
    // The CSS forms sites use for theme colors: #rgb, #rgba, #rrggbb, #rrggbbaa, rgb()/rgba()
    // with numbers or percentages, and white and black. Alpha is ignored.
    static std::optional<HexColor> fromCss(const QString &text);

    QString hexString() const;
    QColor toQColor() const { return QColor::fromRgbF(red, green, blue); }
    // Relative luminance; below about 0.5 a color needs light text on it.
    double luminance() const { return 0.2126 * red + 0.7152 * green + 0.0722 * blue; }
    bool isDark() const { return luminance() < 0.5; }

    bool operator==(const HexColor &other) const { return hexString() == other.hexString(); }
};
