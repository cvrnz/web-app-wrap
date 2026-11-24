#include "hexcolor.h"

#include <QRegularExpression>
#include <algorithm>
#include <cmath>

HexColor::HexColor(double r, double g, double b)
    : red(std::clamp(r, 0.0, 1.0)), green(std::clamp(g, 0.0, 1.0)), blue(std::clamp(b, 0.0, 1.0)) {}

HexColor HexColor::fromQColor(const QColor &color) {
    const QColor rgb = color.toRgb();
    return HexColor(rgb.redF(), rgb.greenF(), rgb.blueF());
}

std::optional<HexColor> HexColor::fromHex(const QString &input) {
    QString text = input.trimmed();
    if (text.startsWith('#'))
        text.remove(0, 1);
    if (text.size() == 3) {
        QString doubled;
        for (const QChar c : text)
            doubled += QString(2, c);
        text = doubled;
    }
    static const QRegularExpression hex("^[0-9A-Fa-f]{6}$");
    if (!hex.match(text).hasMatch())
        return std::nullopt;
    const uint value = text.toUInt(nullptr, 16);
    return fromRgb((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
}

std::optional<HexColor> HexColor::fromCss(const QString &input) {
    const QString text = input.trimmed().toLower();
    if (text == "white")
        return HexColor(1, 1, 1);
    if (text == "black")
        return HexColor(0, 0, 0);

    if (text.startsWith('#')) {
        QString digits = text.mid(1);
        if (digits.size() == 4)
            digits.truncate(3);
        if (digits.size() == 8)
            digits.truncate(6);
        return fromHex(digits);
    }

    const qsizetype open = text.indexOf('('), close = text.lastIndexOf(')');
    if (!text.startsWith("rgb") || open < 0 || close < open)
        return std::nullopt;
    QString inside = text.mid(open + 1, close - open - 1);
    inside.replace('/', ' ');
    static const QRegularExpression separators("[,\\s]+");
    const QStringList parts = inside.split(separators, Qt::SkipEmptyParts);
    if (parts.size() < 3)
        return std::nullopt;
    auto channel = [](QString part, bool *ok) {
        if (part.endsWith('%')) {
            part.chop(1);
            return part.toDouble(ok) / 100;
        }
        return part.toDouble(ok) / 255;
    };
    bool okR = false, okG = false, okB = false;
    const double r = channel(parts[0], &okR), g = channel(parts[1], &okG), b = channel(parts[2], &okB);
    if (!okR || !okG || !okB)
        return std::nullopt;
    return HexColor(r, g, b);
}

QString HexColor::hexString() const {
    auto channel = [](double v) { return QString("%1").arg(int(std::lround(v * 255)), 2, 16, QChar('0')).toUpper(); };
    return "#" + channel(red) + channel(green) + channel(blue);
}
