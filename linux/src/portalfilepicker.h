#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <functional>

// Opens the desktop's file chooser through xdg-desktop-portal
// (org.freedesktop.portal.FileChooser), as Omarchy's apps do, rather than Qt's own dialog.
class PortalFilePicker : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(const QString &path, const QString &error)>;

    explicit PortalFilePicker(QObject *parent = nullptr);

    // Calls `done` once with the chosen file, an empty path if the user cancelled, or an error
    // when there's no portal to ask. `patterns` are globs such as "*.svg".
    void openFile(const QString &title, const QString &filterName, const QStringList &patterns, Callback done);

private slots:
    void onResponse(uint response, const QVariantMap &results);

private:
    Callback m_done;
    QString m_requestPath;
};
