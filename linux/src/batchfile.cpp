#include "batchfile.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

#include "json5.h"

namespace BatchFile {

QString resolvePath(const QString &path, const QDir &base) {
    QString expanded = path.trimmed();
    if (expanded == "~")
        expanded = QDir::homePath();
    else if (expanded.startsWith("~/"))
        expanded = QDir::homePath() + expanded.mid(1);
    return QDir::cleanPath(QDir::isAbsolutePath(expanded) ? expanded : base.absoluteFilePath(expanded));
}

static QString tr(const char *text) { return QCoreApplication::translate("BatchFile", text); }

std::optional<QList<BatchEntry>> parse(const QByteArray &data, const QDir &baseDirectory, QString *error) {
    auto failWith = [&](const QString &message) {
        if (error)
            *error = message;
        return std::nullopt;
    };

    QString parseError;
    const QJsonValue json = Json5::parse(data, &parseError);
    if (json.isUndefined())
        return failWith(tr("The file isn’t valid JSON: %1").arg(parseError));
    if (!json.isArray())
        return failWith(tr("The file must contain a JSON array of app objects."));
    const QJsonArray objects = json.toArray();
    if (objects.isEmpty())
        return failWith(tr("The array is empty."));

    static const QSet<QString> knownKeys{"name", "url", "image", "zoom", "iconBackground", "iconColor", "titleBarColor"};
    QList<BatchEntry> entries;
    QStringList problems;

    for (qsizetype index = 0; index < objects.size(); ++index) {
        const QString label = tr("Entry %1").arg(index + 1);
        if (!objects[index].isObject()) {
            problems << tr("%1 isn’t an object.").arg(label);
            continue;
        }
        const QJsonObject object = objects[index].toObject();
        const QString entryLabel = object.value("name").isString()
            ? QString("%1 (%2)").arg(label, object.value("name").toString()) : label;
        QStringList entryProblems;

        QStringList unknown;
        for (const QString &key : object.keys())
            if (!knownKeys.contains(key))
                unknown << QString("“%1”").arg(key);
        if (!unknown.isEmpty())
            entryProblems << (unknown.size() == 1 ? tr("unknown field %1") : tr("unknown fields %1")).arg(unknown.join(", "));

        BatchEntry entry;
        entry.location = AppLocation(object.value("name").toString());
        if (entry.location.name.isEmpty())
            entryProblems << tr("“name” is missing");

        entry.url = InputNormalizer::normalizeUrl(object.value("url").toString());
        if (!entry.url.isValid())
            entryProblems << tr("“url” is missing or isn’t a web address");

        const QJsonValue image = object.value("image");
        if (image.isString() && !image.toString().trimmed().isEmpty())
            entry.image = resolvePath(image.toString(), baseDirectory);
        else if (!image.isUndefined() && !image.isNull())
            entryProblems << tr("“image” must be a file path");

        const QJsonValue zoom = object.value("zoom");
        if (!zoom.isUndefined() && !zoom.isNull()) {
            if (zoom.isDouble() && zoom.toDouble() >= minimumZoom && zoom.toDouble() <= maximumZoom)
                entry.zoom = zoom.toDouble();
            else
                entryProblems << tr("“zoom” must be a number from 0.5 to 3");
        }

        auto color = [&](const char *key, const char *example, std::optional<HexColor> &target) {
            const QJsonValue value = object.value(key);
            if (value.isUndefined() || value.isNull())
                return;
            target = value.isString() ? HexColor::fromCss(value.toString()) : std::nullopt;
            if (!target)
                entryProblems << tr("“%1” must be a color like \"%2\"").arg(key, example);
        };
        color("iconBackground", "#1c1c1c", entry.iconBackground);
        color("iconColor", "#e5e5e5", entry.iconColor);
        color("titleBarColor", "#1a1a1a", entry.titleBarColor);

        if (entryProblems.isEmpty())
            entries << entry;
        else
            problems << QString("%1: %2.").arg(entryLabel, entryProblems.join("; "));
    }

    // Two entries with the same name would build the same app, with the same sign-ins.
    QHash<QString, int> counts;
    QHash<QString, QString> names;
    for (const BatchEntry &entry : entries) {
        ++counts[entry.location.slug()];
        names.insert(entry.location.slug(), entry.location.name);
    }
    QStringList duplicates;
    for (auto it = counts.begin(); it != counts.end(); ++it)
        if (it.value() > 1)
            duplicates << it.key();
    duplicates.sort();
    for (const QString &slug : duplicates)
        problems << tr("More than one entry would create the app “%1”.").arg(names.value(slug));

    if (!problems.isEmpty())
        return failWith(problems.join('\n'));
    return entries;
}

}
