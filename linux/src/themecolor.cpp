#include "themecolor.h"

#include <QEventLoop>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace {

// Attributes of every `<name ...>` tag, with lowercased attribute names. Enough for <head>
// metadata; not a full HTML parser.
QList<QHash<QString, QString>> tags(const QString &name, const QString &html) {
    const QRegularExpression tagPattern("<" + name + "\\b[^>]*>", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression attributePattern(
        R"re(([A-Za-z_:][-A-Za-z0-9_:.]*)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'>]+)))re");
    QList<QHash<QString, QString>> result;
    for (auto tag = tagPattern.globalMatch(html); tag.hasNext();) {
        QHash<QString, QString> attributes;
        const QString text = tag.next().captured(0);
        for (auto attribute = attributePattern.globalMatch(text); attribute.hasNext();) {
            const auto match = attribute.next();
            const QString key = match.captured(1).toLower();
            QString value;
            for (int group = 2; group <= 4; ++group)
                if (match.capturedStart(group) >= 0) {
                    value = match.captured(group);
                    break;
                }
            if (!attributes.contains(key))
                attributes.insert(key, value);
        }
        result << attributes;
    }
    return result;
}

}

namespace ThemeColor {

std::optional<HexColor> fromHtml(const QString &html) {
    std::optional<HexColor> best;
    int bestRank = 3;
    for (const auto &tag : tags("meta", html)) {
        if (tag.value("name").toLower() != "theme-color")
            continue;
        const auto color = HexColor::fromCss(tag.value("content"));
        if (!color)
            continue;
        const QString media = tag.value("media").toLower();
        const int rank = media.isEmpty() ? 0 : media.contains("dark") ? 2 : 1;
        if (rank < bestRank) {
            best = color;
            bestRank = rank;
        }
    }
    return best;
}

QUrl manifestUrl(const QString &html, const QUrl &pageUrl) {
    for (const auto &tag : tags("link", html)) {
        const QStringList rel = tag.value("rel").toLower().split(' ', Qt::SkipEmptyParts);
        if (rel.contains("manifest") && tag.contains("href"))
            return pageUrl.resolved(QUrl(tag.value("href")));
    }
    return {};
}

std::optional<HexColor> fromManifest(const QByteArray &data) {
    const QJsonValue value = QJsonDocument::fromJson(data).object().value("theme_color");
    return value.isString() ? HexColor::fromCss(value.toString()) : std::nullopt;
}

}

ThemeColorFetcher::ThemeColorFetcher(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)) {}

QNetworkReply *ThemeColorFetcher::get(const QUrl &url, int timeoutMs) {
    QNetworkRequest request(url);
    // Some sites only send their real page, with its theme color, to browsers.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Safari/537.36");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(timeoutMs);
    return m_network->get(request);
}

void ThemeColorFetcher::cancel() {
    ++m_generation;
    m_done = nullptr;
    if (m_reply)
        m_reply->abort();
}

void ThemeColorFetcher::finish(std::optional<HexColor> color) {
    Callback done = std::move(m_done);
    m_done = nullptr;
    if (done)
        done(color);
}

void ThemeColorFetcher::fetch(const QUrl &url, Callback done, int timeoutMs) {
    cancel();
    m_done = std::move(done);
    const int generation = m_generation;
    QNetworkReply *page = get(url, timeoutMs);
    m_reply = page;
    connect(page, &QNetworkReply::finished, this, [this, page, generation, timeoutMs] {
        page->deleteLater();
        if (generation != m_generation)
            return;
        if (page->error() != QNetworkReply::NoError) {
            finish(std::nullopt);
            return;
        }
        // The <head> is near the top; 512 KB is plenty and keeps huge pages cheap.
        const QString html = QString::fromUtf8(page->read(512 * 1024));
        if (const auto color = ThemeColor::fromHtml(html)) {
            finish(color);
            return;
        }
        const QUrl manifest = ThemeColor::manifestUrl(html, page->url());
        if (!manifest.isValid()) {
            finish(std::nullopt);
            return;
        }
        QNetworkReply *reply = get(manifest, timeoutMs);
        m_reply = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
            reply->deleteLater();
            if (generation != m_generation)
                return;
            finish(reply->error() == QNetworkReply::NoError ? ThemeColor::fromManifest(reply->readAll()) : std::nullopt);
        });
    });
}

std::optional<HexColor> ThemeColorFetcher::fetchBlocking(const QUrl &url, int timeoutMs) {
    ThemeColorFetcher fetcher;
    QEventLoop loop;
    std::optional<HexColor> result;
    fetcher.fetch(url, [&](std::optional<HexColor> color) {
        result = color;
        loop.quit();
    }, timeoutMs);
    loop.exec();
    return result;
}
