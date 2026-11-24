#include "linkpolicy.h"

#include "inputnormalizer.h"

LinkPolicy::LinkPolicy(const QUrl &baseUrl, const QStringList &allowedDomains)
    : m_baseHost(canonicalHost(baseUrl.host())) {
    for (const QString &domain : allowedDomains) {
        const QString normalized = normalizeDomain(domain);
        if (!normalized.isEmpty())
            m_allowedHosts << normalized;
    }
}

QString LinkPolicy::canonicalHost(const QString &host) {
    const QString lower = host.toLower();
    return lower.startsWith("www.") ? lower.mid(4) : lower;
}

QString LinkPolicy::normalizeDomain(const QString &input) {
    const QUrl url = InputNormalizer::normalizeUrl(input);
    return url.isValid() ? canonicalHost(url.host()) : QString();
}

LinkPolicy::Destination LinkPolicy::destination(const QUrl &url) const {
    const QString scheme = url.scheme().toLower();
    if (scheme.isEmpty())
        return Destination::App;
    // Pages the web view makes itself: blank pages, blobs and data, its error pages, and pages
    // that belong to browser extensions.
    static const QStringList internalSchemes{"about", "blob", "data", "javascript", "chrome-error",
                                             "chrome-extension", "devtools", "qrc"};
    if (internalSchemes.contains(scheme))
        return Destination::App;
    if (scheme == "http" || scheme == "https")
        return isInternalHost(url.host()) ? Destination::App : Destination::Browser;
    return Destination::System;
}

bool LinkPolicy::isInternalHost(const QString &host) const {
    if (host.isEmpty())
        return false;
    const QString candidate = canonicalHost(host);
    QStringList hosts{m_baseHost};
    hosts += m_allowedHosts;
    for (const QString &allowed : hosts)
        if (!allowed.isEmpty() && (candidate == allowed || candidate.endsWith("." + allowed)))
            return true;
    return false;
}
