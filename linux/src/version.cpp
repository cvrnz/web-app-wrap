#include "version.h"

#include <QCoreApplication>

// scripts/build writes this header into the build folder.
#if __has_include("waw_version.h")
#include "waw_version.h"
#endif
#ifndef WAW_COMMIT
#define WAW_COMMIT ""
#endif

namespace Version {

std::optional<QString> commit() {
    const QString value = QStringLiteral(WAW_COMMIT);
    return value.isEmpty() ? std::nullopt : std::optional<QString>(value);
}

QString describe(const std::optional<QString> &commit) {
    if (!commit)
        return QCoreApplication::translate("Version", "development build");
    const bool dirty = commit->endsWith("-dirty");
    const QString sha = dirty ? commit->chopped(6) : *commit;
    return sha.left(7) + (dirty ? "-dirty" : "");
}

QString display() { return describe(commit()); }

}
