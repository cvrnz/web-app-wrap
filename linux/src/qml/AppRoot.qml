import QtQuick
import QtWebEngine

// One running app: its profile (cookies, storage and cache, kept apart from every other app),
// its windows, and the things that span windows: notifications, downloads, and being
// launched again.
QtObject {
    id: root

    property var windows: []
    property var lastActiveWindow: null

    property WebEngineProfilePrototype profilePrototype: WebEngineProfilePrototype {
        storageName: app.slug
        persistentStoragePath: app.storagePath
        cachePath: app.cachePath
        persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
        persistentPermissionsPolicy: WebEngineProfile.StoreOnDisk
    }
    // Made once the prototype has all its settings, which a binding can't wait for.
    property WebEngineProfile profile: null

    property Component windowComponent: Component { BrowserWindow {} }

    // A window with one tab at `url`, or with an empty tab when `url` is null, for a page to
    // open into.
    function newWindow(url) {
        const window = windowComponent.createObject(null, { appRoot: root, profile: root.profile })
        windows = windows.concat([window])
        window.addTab(url === undefined ? app.url : url, true)
        window.closing.connect(() => {
            windows = windows.filter(w => w !== window)
            if (lastActiveWindow === window)
                lastActiveWindow = windows.length ? windows[windows.length - 1] : null
            Qt.callLater(() => window.destroy())
        })
        window.activeChanged.connect(() => { if (window.active) lastActiveWindow = window })
        lastActiveWindow = window
        return window
    }

    function bringForward(window) {
        if (window)
            app.activate(window)
    }

    // A tab showing `origin`'s site: the active window's current tab if it does, otherwise the
    // first one found, searching the most recently used window first.
    function findTab(origin) {
        const host = origin.toString().replace(/^[a-z]+:\/\//, "").replace(/[:\/].*$/, "")
        const ordered = lastActiveWindow ? [lastActiveWindow].concat(windows.filter(w => w !== lastActiveWindow)) : windows
        for (const window of ordered) {
            const current = window.currentView
            if (current && current.url.toString().indexOf(host) >= 0)
                return { window: window, index: window.currentIndex }
            for (let i = 0; i < window.tabs.length; ++i)
                if (window.tabs[i].url.toString().indexOf(host) >= 0)
                    return { window: window, index: i }
        }
        return null
    }

    Component.onCompleted: {
        profile = profilePrototype.instance()
        // Qt WebEngine adds "QtWebEngine/<version>" to the browser's user agent, and some sites
        // turn away browsers they don't recognize. Without it, the agent is Chrome's.
        profile.httpUserAgent = profile.httpUserAgent.replace(/\s*QtWebEngine\/\S+/, "")
        // Start out signed in wherever Helium is, since some sites (Google's sign-in) won't let
        // you sign in here. Done before the first page loads, so it already sees the cookies.
        console.info(app.importHeliumCookies(profile))
        newWindow(app.url)
    }

    property Connections appConnections: Connections {
        target: app
        function onOpenRequested() {
            if (root.windows.length === 0)
                root.bringForward(root.newWindow(app.url))
            else
                root.bringForward(root.lastActiveWindow || root.windows[0])
        }
        function onNotificationClicked(origin) {
            const found = root.findTab(origin)
            if (found) {
                found.window.select(found.index)
                root.bringForward(found.window)
            } else {
                root.bringForward(root.lastActiveWindow || root.newWindow(app.url))
            }
        }
    }

    property Connections profileConnections: Connections {
        target: root.profile
        function onPresentNotification(notification) {
            app.presentNotification(notification)
        }
        function onDownloadRequested(download) {
            download.downloadDirectory = app.downloadsPath
            download.accept()
        }
        function onDownloadFinished(download) {
            if (download.state === WebEngineDownloadRequest.DownloadCompleted)
                app.notifyDownloadFinished(download.downloadFileName,
                                           download.downloadDirectory + "/" + download.downloadFileName)
        }
    }
}
