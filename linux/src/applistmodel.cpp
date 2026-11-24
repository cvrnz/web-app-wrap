#include "applistmodel.h"

#include <QDateTime>
#include <QProcess>

#include "themecolor.h"
#include "version.h"

AppListModel::AppListModel(const Paths &paths, const QString &executable, const std::optional<QString> &version, QObject *parent)
    : QAbstractListModel(parent), m_paths(paths), m_registry(paths.dataRoot), m_builder(paths, executable, version),
      m_version(version), m_executable(executable), m_draft(new AppDraft(this)), m_fetcher(new ThemeColorFetcher(this)) {
    reload();
}

void AppListModel::reload() {
    QString error;
    const auto records = m_registry.load(&error);
    beginResetModel();
    m_records = records.value_or(QList<AppRecord>());
    endResetModel();
    if (!records)
        m_message = tr("Couldn’t read the list: %1").arg(error);
    emit countChanged();
    emit statusChanged();
}

bool AppListModel::save() {
    QString error;
    if (!m_registry.save(m_records, &error)) {
        m_message = tr("Couldn’t save the list: %1").arg(error);
        emit statusChanged();
        return false;
    }
    return true;
}

QString AppListModel::version() const { return Version::describe(m_version); }

int AppListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_records.size());
}

int AppListModel::rowOf(const QString &id) const {
    for (int row = 0; row < m_records.size(); ++row)
        if (m_records[row].idString() == id)
            return row;
    return -1;
}

std::optional<AppRecord::BuildReason> AppListModel::reasonFor(const AppRecord &record) const {
    return record.buildReason(m_version, m_builder.isInstalled(record.settings.location().slug()));
}

QVariant AppListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_records.size())
        return {};
    const AppRecord &record = m_records[index.row()];
    const QString id = record.idString();
    switch (role) {
    case IdRole: return id;
    case NameRole: return record.settings.location().name;
    case UrlRole: return record.settings.url;
    case IconRole: return QString("image://icons/%1/%2").arg(id).arg(m_iconRevisions.value(id));
    case NeedsBuildRole: return reasonFor(record).has_value();
    case InstalledRole: return m_builder.isInstalled(record.settings.location().slug());
    case BuildingRole: return m_current == id || m_queue.contains(id);
    case FailedRole: return m_errors.contains(id);
    case StatusRole:
        if (m_current == id)
            return tr("Building…");
        if (m_queue.contains(id))
            return tr("Waiting to build");
        if (m_errors.contains(id))
            return m_errors.value(id);
        if (const auto reason = reasonFor(record))
            return AppRecord::describe(*reason);
        return tr("Up to date");
    }
    return {};
}

QHash<int, QByteArray> AppListModel::roleNames() const {
    return {{IdRole, "appId"},       {NameRole, "name"},         {UrlRole, "url"},
            {IconRole, "icon"},      {StatusRole, "status"},     {NeedsBuildRole, "needsBuild"},
            {InstalledRole, "installed"}, {BuildingRole, "building"}, {FailedRole, "failed"}};
}

int AppListModel::pendingCount() const {
    int count = 0;
    for (const AppRecord &record : m_records)
        if (reasonFor(record))
            ++count;
    return count;
}

void AppListModel::newDraft() { m_draft->reset(); }

void AppListModel::editDraft(int row) {
    if (row < 0 || row >= m_records.size())
        return;
    const AppRecord &record = m_records[row];
    m_draft->load(record, record.settings.icon ? m_registry.iconPath(*record.settings.icon) : QString());
}

QString AppListModel::saveDraft() {
    if (!m_draft->valid())
        return tr("Enter a name and a web address.");
    AppSettings settings = m_draft->settings();
    const QString slug = settings.location().slug();
    const int editingRow = rowOf(m_draft->editingId());
    for (int row = 0; row < m_records.size(); ++row)
        if (row != editingRow && m_records[row].settings.location().slug() == slug)
            return tr("“%1” is already on the list. Each app needs its own name.").arg(m_records[row].settings.location().name);

    AppRecord record = editingRow >= 0 ? m_records[editingRow] : AppRecord{QUuid::createUuid(), {}, std::nullopt};
    const std::optional<QString> oldIcon = record.settings.icon;
    if (m_draft->hasImage()) {
        QString error;
        const QString fileName = m_registry.importIcon(m_draft->imagePath(), record.id, oldIcon, &error);
        if (fileName.isEmpty())
            return error;
        settings.icon = fileName;
    }
    if (oldIcon && settings.icon != oldIcon)
        m_registry.retireIcon(*oldIcon, record.settings.name);
    // The background and color only matter for SVG icons, including one kept from before.
    if (!settings.icon || !IconSource::isSvg(*settings.icon)) {
        settings.iconBackground.reset();
        settings.iconColor.reset();
    }
    record.settings = settings;

    const QString id = record.idString();
    if (editingRow >= 0) {
        m_records[editingRow] = record;
    } else {
        beginInsertRows({}, int(m_records.size()), int(m_records.size()));
        m_records << record;
        endInsertRows();
        emit countChanged();
    }
    m_errors.remove(id);
    ++m_iconRevisions[id];
    if (!save())
        return m_message;
    refreshRow(id);
    emit statusChanged();
    return {};
}

void AppListModel::removeFromList(int row) {
    if (row < 0 || row >= m_records.size() || building())
        return;
    const AppRecord record = m_records[row];
    if (record.settings.icon)
        m_registry.retireIcon(*record.settings.icon, record.settings.name);
    beginRemoveRows({}, row, row);
    m_records.removeAt(row);
    endRemoveRows();
    save();
    emit countChanged();
    emit statusChanged();
}

void AppListModel::uninstall(int row) {
    if (row < 0 || row >= m_records.size() || building())
        return;
    QString error;
    if (!m_builder.remove(m_records[row].settings.location().slug(), false, &error))
        m_message = error;
    else
        m_message = tr("Uninstalled %1. Its sign-ins are kept, in case you build it again.").arg(m_records[row].settings.location().name);
    refreshRow(m_records[row].idString());
    emit statusChanged();
}

void AppListModel::openApp(int row) const {
    if (row < 0 || row >= m_records.size())
        return;
    QProcess::startDetached(m_executable == "webappwrap" ? QStringLiteral("webappwrap") : m_executable,
                            {"run", m_records[row].settings.location().slug()});
}

void AppListModel::buildPending() {
    for (const AppRecord &record : std::as_const(m_records))
        if (reasonFor(record) && !m_queue.contains(record.idString()) && m_current != record.idString())
            m_queue << record.idString();
    m_message.clear();
    emit statusChanged();
    buildNext();
}

void AppListModel::build(int row) {
    if (row < 0 || row >= m_records.size())
        return;
    const QString id = m_records[row].idString();
    if (!m_queue.contains(id) && m_current != id)
        m_queue << id;
    m_message.clear();
    refreshRow(id);
    emit statusChanged();
    buildNext();
}

void AppListModel::buildNext() {
    if (!m_current.isEmpty())
        return;
    if (m_queue.isEmpty()) {
        emit statusChanged();
        return;
    }
    m_current = m_queue.takeFirst();
    m_errors.remove(m_current);
    refreshRow(m_current);
    emit statusChanged();

    const int row = rowOf(m_current);
    if (row < 0) {
        finishCurrent({});
        return;
    }
    const auto entry = m_records[row].batchEntry(m_registry.iconsFolder());
    if (!entry) {
        finishCurrent(tr("The name or web address isn’t valid."));
        return;
    }
    auto finish = [this, entry](const HexColor &titleBarColor) {
        const BuildOutcome outcome = m_builder.build(*entry, titleBarColor, m_current);
        if (outcome.ok) {
            const int row = rowOf(m_current);
            if (row >= 0) {
                m_records[row].lastBuild = BuildStamp{m_version, QDateTime::currentDateTimeUtc(), m_records[row].settings};
                save();
            }
        }
        finishCurrent(outcome.ok ? QString() : outcome.error);
    };
    if (entry->titleBarColor) {
        finish(*entry->titleBarColor);
        return;
    }
    // Without a chosen color, the app gets the site's theme color as it is now.
    m_fetcher->fetch(entry->url, [finish](std::optional<HexColor> color) {
        finish(color.value_or(HexColor::defaultTitleBar()));
    });
}

void AppListModel::finishCurrent(const QString &error) {
    const QString id = m_current;
    if (!error.isEmpty())
        m_errors.insert(id, error);
    m_current.clear();
    ++m_iconRevisions[id];
    refreshRow(id);
    if (m_queue.isEmpty()) {
        m_message = m_errors.isEmpty() ? tr("Your apps are built. Find them in the app launcher.")
                                       : tr("Some apps didn’t build. Their errors are in the list.");
    }
    QMetaObject::invokeMethod(this, &AppListModel::buildNext, Qt::QueuedConnection);
}

void AppListModel::refreshRow(const QString &id) {
    const int row = rowOf(id);
    if (row >= 0)
        emit dataChanged(index(row), index(row));
}

QImage AppListModel::icon(const QString &id, int pixels) const {
    if (id == "draft")
        return IconRenderer::render(m_draft->iconSource(), pixels);
    const int row = rowOf(id);
    if (row < 0)
        return {};
    const auto entry = m_records[row].batchEntry(m_registry.iconsFolder());
    const auto source = entry ? AppBuilder::iconSource(*entry) : std::nullopt;
    return IconRenderer::render(source.value_or(IconSource::generated(m_records[row].settings.location().name)), pixels);
}

QImage IconImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    const int pixels = requestedSize.isValid() ? std::max(requestedSize.width(), requestedSize.height()) : 256;
    const QImage image = m_model->icon(id.section('/', 0, 0), pixels);
    if (size)
        *size = image.size();
    return image;
}
