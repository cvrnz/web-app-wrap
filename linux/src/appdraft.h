#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QTimer>
#include <optional>

#include "appregistry.h"
#include "iconrenderer.h"

class PortalFilePicker;
class ThemeColorFetcher;

// The inputs for one app while it's being added to or edited in the list, handed to the
// editor as `draft`. It looks up the site's theme color as the address is typed.
class AppDraft : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString editingId READ editingId NOTIFY changed)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY changed)
    Q_PROPERTY(bool valid READ valid NOTIFY changed)
    Q_PROPERTY(QString imageName READ imageName NOTIFY changed)
    Q_PROPERTY(QString imageError READ imageError NOTIFY changed)
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY changed)
    Q_PROPERTY(bool imageIsSvg READ imageIsSvg NOTIFY changed)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY changed)
    Q_PROPERTY(double zoomMinimum READ zoomMinimum NOTIFY changed)
    Q_PROPERTY(QColor iconBackground READ iconBackground WRITE setIconBackground NOTIFY changed)
    Q_PROPERTY(bool usesSvgColor READ usesSvgColor WRITE setUsesSvgColor NOTIFY changed)
    Q_PROPERTY(QColor svgColor READ svgColor WRITE setSvgColor NOTIFY changed)
    Q_PROPERTY(QColor titleBarColor READ titleBarColor NOTIFY changed)
    Q_PROPERTY(QString titleBarSource READ titleBarSource NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit AppDraft(QObject *parent = nullptr);

    // Starts a new app, or from one in the list (`iconPath` is where its stored icon is).
    void reset();
    void load(const AppRecord &record, const QString &iconPath);

    QString editingId() const { return m_editingId; }
    QString name() const { return m_name; }
    void setName(const QString &name);
    QString url() const { return m_url; }
    void setUrl(const QString &url);
    bool valid() const;
    QString imageName() const { return m_imageName; }
    QString imageError() const { return m_imageError; }
    QString imagePath() const { return m_imagePath; }
    bool hasImage() const { return !m_imagePath.isEmpty(); }
    bool imageIsSvg() const { return hasImage() && IconSource::isSvg(m_imagePath); }
    double zoom() const { return m_zoom; }
    void setZoom(double zoom);
    double zoomMinimum() const { return hasImage() && !imageIsSvg() ? 1 : 0.5; }
    QColor iconBackground() const { return m_iconBackground; }
    void setIconBackground(const QColor &color);
    bool usesSvgColor() const { return m_usesSvgColor; }
    void setUsesSvgColor(bool uses);
    QColor svgColor() const { return m_svgColor; }
    void setSvgColor(const QColor &color);
    QColor titleBarColor() const { return m_titleBarColor; }
    // "standard" (no site color known), "lookingUp", "site" or "custom".
    QString titleBarSource() const { return m_titleBarSource; }
    int revision() const { return m_revision; }

    Q_INVOKABLE bool setImage(const QUrl &file);
    Q_INVOKABLE void chooseImage();
    Q_INVOKABLE void clearImage();
    Q_INVOKABLE void setTitleBarColor(const QColor &color);
    Q_INVOKABLE void useSiteColor();

    // The icon as the builder would draw it.
    IconSource iconSource() const;
    // The settings to save; `icon` is filled in by the list when it stores the image.
    AppSettings settings() const;

signals:
    void changed();

private:
    void touch();
    void lookUpThemeColor();

    QString m_editingId;
    QString m_name;
    QString m_url;
    QString m_imagePath;
    QString m_imageName;
    QString m_imageError;
    double m_zoom = 1;
    QColor m_iconBackground;
    bool m_usesSvgColor = false;
    QColor m_svgColor;
    QColor m_titleBarColor;
    QString m_titleBarSource = "standard";
    int m_revision = 0;
    QTimer m_lookupDelay;
    ThemeColorFetcher *m_fetcher;
    PortalFilePicker *m_picker;
};
