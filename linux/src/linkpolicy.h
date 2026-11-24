#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

// Decides whether a link belongs to the wrapped web app or goes to the browser, the same way
// as on macOS and Windows. The app's own site is the base URL's host; `allowedDomains` adds
// other sites the user allowed, each with its subdomains. "www." is ignored on both sides.
class LinkPolicy {
public:
    enum class Destination {
        App,     // load inside the app
        Browser, // open in the default web browser
        System,  // hand to whatever handles the scheme (mailto:, tel:, zoommtg:, ...)
    };

    explicit LinkPolicy(const QUrl &baseUrl = {}, const QStringList &allowedDomains = {});

    Destination destination(const QUrl &url) const;
    // Whether the URL is on the app's own site or an allowed one.
    bool isListed(const QUrl &url) const { return isInternalHost(url.host()); }

    // The domain in a typed or pasted entry: "Example.com", "www.example.com" and
    // "https://example.com/page" all give "example.com". Empty if it isn't a web address.
    static QString normalizeDomain(const QString &input);
    static QString canonicalHost(const QString &host);

private:
    bool isInternalHost(const QString &host) const;

    QString m_baseHost;
    QStringList m_allowedHosts;
};
