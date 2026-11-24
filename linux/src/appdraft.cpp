#include "appdraft.h"

#include <QFileInfo>
#include <algorithm>

#include "portalfilepicker.h"
#include "themecolor.h"

AppDraft::AppDraft(QObject *parent)
    : QObject(parent), m_fetcher(new ThemeColorFetcher(this)), m_picker(new PortalFilePicker(this)) {
    // Look the color up once the address stops changing, not on every key.
    m_lookupDelay.setSingleShot(true);
    m_lookupDelay.setInterval(600);
    connect(&m_lookupDelay, &QTimer::timeout, this, [this] {
        const QUrl url = InputNormalizer::normalizeUrl(m_url);
        if (!url.isValid() || m_titleBarSource == "custom")
            return;
        m_fetcher->fetch(url, [this](std::optional<HexColor> color) {
            if (m_titleBarSource == "custom")
                return;
            m_titleBarColor = color.value_or(HexColor::defaultTitleBar()).toQColor();
            m_titleBarSource = color ? "site" : "standard";
            touch();
        });
    });
    reset();
}

void AppDraft::reset() {
    m_fetcher->cancel();
    m_lookupDelay.stop();
    m_editingId.clear();
    m_name.clear();
    m_url.clear();
    m_imagePath.clear();
    m_imageName.clear();
    m_imageError.clear();
    m_zoom = 1;
    m_iconBackground = HexColor::defaultIconBackground().toQColor();
    m_usesSvgColor = false;
    m_svgColor = HexColor::defaultSvgColor().toQColor();
    m_titleBarColor = HexColor::defaultTitleBar().toQColor();
    m_titleBarSource = "standard";
    touch();
}

void AppDraft::load(const AppRecord &record, const QString &iconPath) {
    reset();
    const AppSettings &settings = record.settings;
    m_editingId = record.idString();
    m_name = settings.name;
    m_url = settings.url;
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath)) {
        m_imagePath = iconPath;
        m_imageName = tr("Current icon");
    }
    m_zoom = settings.zoom;
    if (const auto hex = settings.iconBackground ? HexColor::fromHex(*settings.iconBackground) : std::nullopt)
        m_iconBackground = hex->toQColor();
    if (const auto hex = settings.iconColor ? HexColor::fromHex(*settings.iconColor) : std::nullopt) {
        m_usesSvgColor = true;
        m_svgColor = hex->toQColor();
    }
    if (const auto hex = settings.titleBarColor ? HexColor::fromHex(*settings.titleBarColor) : std::nullopt) {
        m_titleBarColor = hex->toQColor();
        m_titleBarSource = "custom";
    } else {
        lookUpThemeColor();
    }
    touch();
}

void AppDraft::touch() {
    ++m_revision;
    emit changed();
}

void AppDraft::setName(const QString &name) {
    if (name == m_name)
        return;
    m_name = name;
    touch();
}

void AppDraft::setUrl(const QString &url) {
    if (url == m_url)
        return;
    m_url = url;
    lookUpThemeColor();
    touch();
}

void AppDraft::lookUpThemeColor() {
    m_fetcher->cancel();
    if (m_titleBarSource == "custom" || !InputNormalizer::normalizeUrl(m_url).isValid()) {
        m_lookupDelay.stop();
        return;
    }
    m_titleBarSource = "lookingUp";
    m_lookupDelay.start();
}

bool AppDraft::valid() const {
    return !AppLocation(m_name).name.isEmpty() && InputNormalizer::normalizeUrl(m_url).isValid();
}

void AppDraft::setZoom(double zoom) {
    zoom = std::clamp(zoom, zoomMinimum(), 3.0);
    if (qFuzzyCompare(zoom, m_zoom))
        return;
    m_zoom = zoom;
    touch();
}

void AppDraft::setIconBackground(const QColor &color) {
    m_iconBackground = color;
    touch();
}

void AppDraft::setUsesSvgColor(bool uses) {
    m_usesSvgColor = uses;
    touch();
}

void AppDraft::setSvgColor(const QColor &color) {
    m_svgColor = color;
    touch();
}

bool AppDraft::setImage(const QUrl &file) {
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    // New icons are SVGs, drawn on the background color, as on macOS. Apps keep older images.
    if (!IconSource::isSvg(path)) {
        m_imageError = tr("Choose an SVG file.");
        touch();
        return false;
    }
    QString error;
    if (!IconSource::load(path, 1, std::nullopt, std::nullopt, &error)) {
        m_imageError = error;
        touch();
        return false;
    }
    m_imagePath = path;
    m_imageName = QFileInfo(path).fileName();
    m_imageError.clear();
    m_zoom = 1;
    touch();
    return true;
}

void AppDraft::chooseImage() {
    m_picker->openFile(tr("Choose an SVG for the app icon"), tr("SVG images"), {"*.svg", "*.SVG"},
                       [this](const QString &path, const QString &error) {
                           if (!error.isEmpty()) {
                               m_imageError = error;
                               touch();
                           } else if (!path.isEmpty()) {
                               setImage(QUrl::fromLocalFile(path));
                           }
                       });
}

void AppDraft::clearImage() {
    m_imagePath.clear();
    m_imageName.clear();
    m_imageError.clear();
    m_zoom = 1;
    touch();
}

void AppDraft::setTitleBarColor(const QColor &color) {
    m_fetcher->cancel();
    m_lookupDelay.stop();
    m_titleBarColor = color;
    m_titleBarSource = "custom";
    touch();
}

void AppDraft::useSiteColor() {
    m_titleBarSource = "standard";
    m_titleBarColor = HexColor::defaultTitleBar().toQColor();
    lookUpThemeColor();
    touch();
}

IconSource AppDraft::iconSource() const {
    if (hasImage()) {
        const std::optional<HexColor> color = m_usesSvgColor ? std::optional(HexColor::fromQColor(m_svgColor)) : std::nullopt;
        if (const auto source = IconSource::load(m_imagePath, m_zoom, HexColor::fromQColor(m_iconBackground), color))
            return *source;
    }
    return IconSource::generated(AppLocation(m_name).name);
}

AppSettings AppDraft::settings() const {
    AppSettings settings;
    settings.name = m_name.trimmed();
    settings.url = m_url.trimmed();
    settings.zoom = m_zoom;
    if (imageIsSvg()) {
        settings.iconBackground = HexColor::fromQColor(m_iconBackground).hexString();
        if (m_usesSvgColor)
            settings.iconColor = HexColor::fromQColor(m_svgColor).hexString();
    }
    if (m_titleBarSource == "custom")
        settings.titleBarColor = HexColor::fromQColor(m_titleBarColor).hexString();
    return settings;
}
