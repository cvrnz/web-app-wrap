#include "paths.h"

#include <QDir>

static QString xdg(const char *variable, const QString &fallback) {
    const QString value = qEnvironmentVariable(variable);
    // The XDG spec says relative values are invalid and should be ignored.
    return !value.isEmpty() && QDir::isAbsolutePath(value) ? value : QDir::homePath() + fallback;
}

Paths Paths::standard() {
    const QString data = xdg("XDG_DATA_HOME", "/.local/share");
    return {data + "/webappwrap", data + "/applications", xdg("XDG_CACHE_HOME", "/.cache") + "/webappwrap"};
}

Paths Paths::under(const QString &root) {
    return {root + "/data", root + "/applications", root + "/cache"};
}
