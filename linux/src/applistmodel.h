#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QQuickImageProvider>
#include <QStringList>

#include "appbuilder.h"
#include "appdraft.h"
#include "appregistry.h"
#include "paths.h"

class ThemeColorFetcher;

// The My Apps list, handed to the window as `apps`: every app made in the window, whether it
// needs building and why, and the editor's draft.
class AppListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY statusChanged)
    Q_PROPERTY(bool building READ building NOTIFY statusChanged)
    Q_PROPERTY(QString message READ message NOTIFY statusChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString dataFolder READ dataFolder CONSTANT)
    Q_PROPERTY(AppDraft *draft READ draft CONSTANT)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        UrlRole,
        IconRole,
        StatusRole,
        NeedsBuildRole,
        InstalledRole,
        BuildingRole,
        FailedRole,
    };

    AppListModel(const Paths &paths, const QString &executable, const std::optional<QString> &version, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int pendingCount() const;
    bool building() const { return !m_queue.isEmpty() || !m_current.isEmpty(); }
    QString message() const { return m_message; }
    QString version() const;
    QString dataFolder() const { return m_registry.folder(); }
    AppDraft *draft() const { return m_draft; }

    // Starts the editor on a new app, or on the app in `row`.
    Q_INVOKABLE void newDraft();
    Q_INVOKABLE void editDraft(int row);
    // Saves the draft to the list. Returns an error to show, or an empty string.
    Q_INVOKABLE QString saveDraft();

    // Takes the app off the list; the app itself stays installed.
    Q_INVOKABLE void removeFromList(int row);
    // Removes the installed app (its launcher entry and icon), keeping its sign-ins.
    Q_INVOKABLE void uninstall(int row);
    Q_INVOKABLE void openApp(int row) const;
    // Builds every app that needs it, or just the one in `row`.
    Q_INVOKABLE void buildPending();
    Q_INVOKABLE void build(int row);

    // Renders icons for the image provider. `id` is a record ID or "draft".
    QImage icon(const QString &id, int pixels) const;

signals:
    void countChanged();
    void statusChanged();

private:
    void reload();
    bool save();
    int rowOf(const QString &id) const;
    std::optional<AppRecord::BuildReason> reasonFor(const AppRecord &record) const;
    void buildNext();
    void finishCurrent(const QString &error);
    void refreshRow(const QString &id);

    Paths m_paths;
    AppRegistry m_registry;
    AppBuilder m_builder;
    std::optional<QString> m_version;
    QString m_executable;
    QList<AppRecord> m_records;
    QHash<QString, QString> m_errors; // by record ID, from the last build
    QStringList m_queue;
    QString m_current;
    QString m_message;
    QHash<QString, int> m_iconRevisions;
    AppDraft *m_draft;
    ThemeColorFetcher *m_fetcher;
};

// Serves image://icons/<id>/<revision>, where <id> is a record ID or "draft".
class IconImageProvider : public QQuickImageProvider {
public:
    explicit IconImageProvider(AppListModel *model)
        : QQuickImageProvider(QQuickImageProvider::Image), m_model(model) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    AppListModel *m_model;
};
