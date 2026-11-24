#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <functional>
#include <optional>

#include "hexcolor.h"

class QNetworkAccessManager;
class QNetworkReply;

// Finds a site's theme color: `<meta name="theme-color">` in its HTML, or `theme_color` in the
// web app manifest the HTML links to. The same rules as on macOS and Windows.
namespace ThemeColor {
// The first theme-color meta tag's color. Tags without a `media` query win over light and dark
// variants, and a light variant wins over a dark one.
std::optional<HexColor> fromHtml(const QString &html);
// The manifest linked from the HTML, resolved against the page's address.
QUrl manifestUrl(const QString &html, const QUrl &pageUrl);
std::optional<HexColor> fromManifest(const QByteArray &data);
}

// Fetches a site and finds its theme color. One lookup at a time: starting another cancels
// the one in progress.
class ThemeColorFetcher : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(std::optional<HexColor>)>;

    explicit ThemeColorFetcher(QObject *parent = nullptr);

    // Calls `done` once, with none if the site couldn't be reached or has no theme color.
    void fetch(const QUrl &url, Callback done, int timeoutMs = 8000);
    void cancel();

    // Waits for the answer, for the command line.
    static std::optional<HexColor> fetchBlocking(const QUrl &url, int timeoutMs = 8000);

private:
    QNetworkReply *get(const QUrl &url, int timeoutMs);
    void finish(std::optional<HexColor> color);

    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    Callback m_done;
    int m_generation = 0;
};
