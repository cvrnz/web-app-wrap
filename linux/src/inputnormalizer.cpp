#include "inputnormalizer.h"

namespace InputNormalizer {

QUrl normalizeUrl(const QString &raw) {
    QString text = raw.trimmed();
    if (text.isEmpty())
        return {};

    if (!text.contains("://")) {
        const QString lower = text.toLower();
        const bool isLocal = lower.startsWith("localhost") || lower.startsWith("127.0.0.1") || lower.startsWith("0.0.0.0");
        text = (isLocal ? "http://" : "https://") + text;
    }

    const QUrl url(text, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != "http" && scheme != "https"))
        return {};
    const QString host = url.host();
    if (host.isEmpty())
        return {};
    // Letters (including international ones), digits, and the punctuation hostnames and IPv6
    // literals use. Anything else, such as a space, means the input wasn't an address.
    for (const QChar c : host)
        if (!c.isLetterOrNumber() && !QStringLiteral("-._:[]").contains(c))
            return {};
    return url;
}

QString slug(const QString &name) {
    // Decompose accented letters so "Café" becomes "cafe", then keep ASCII letters and digits.
    const QString folded = name.normalized(QString::NormalizationForm_KD).toLower();
    QString result;
    bool pendingHyphen = false;
    for (const QChar c : folded) {
        if (c.category() == QChar::Mark_NonSpacing)
            continue;
        const bool isAlnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (isAlnum) {
            if (pendingHyphen && !result.isEmpty())
                result += '-';
            result += c;
            pendingHyphen = false;
        } else {
            pendingHyphen = true;
        }
    }
    return result.isEmpty() ? QStringLiteral("app") : result;
}

QString fileSafeName(const QString &name) {
    QString cleaned = name;
    cleaned.replace('/', '-').replace(':', '-');
    cleaned = cleaned.trimmed();
    if (cleaned.startsWith('.'))
        cleaned.remove(0, 1);
    return cleaned.isEmpty() ? QStringLiteral("App") : cleaned;
}

}

AppLocation::AppLocation(const QString &input) {
    QStringList parts;
    for (const QString &part : input.split('/')) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty() && trimmed != "." && trimmed != "..")
            parts << trimmed;
    }
    if (parts.isEmpty())
        return;
    name = parts.takeLast();
    for (const QString &folder : parts)
        folders << InputNormalizer::fileSafeName(folder);
}
