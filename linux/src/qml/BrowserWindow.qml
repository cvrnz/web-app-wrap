import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQml.Models
import QtWebEngine

// One window of an app: a bar in the app's title bar color holding the tabs and the ⋮ menu,
// and the tabs' web views below it.
ApplicationWindow {
    id: win

    property var appRoot
    property WebEngineProfile profile
    property var tabs: []
    property var currentView: null
    readonly property int currentIndex: tabs.indexOf(currentView)
    readonly property color barColor: app.titleBarColor
    readonly property color barText: app.titleBarDark ? "#ffffff" : "#000000"
    property string hoveredLink: ""
    property var devTools: null

    width: 1200
    height: 800
    minimumWidth: 360
    minimumHeight: 240
    visible: true
    color: barColor
    title: currentView && currentView.title ? currentView.title : app.name

    Material.theme: app.titleBarDark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    // MARK: Tabs

    function select(index) {
        if (index < 0 || index >= tabs.length)
            return
        currentView = tabs[index]
        currentView.forceActiveFocus()
    }

    // Opens a tab after `opener` (or at the end), in front unless `foreground` is false.
    function addTab(url, foreground, opener) {
        const view = tabComponent.createObject(webArea)
        const list = tabs.slice()
        const at = opener && list.indexOf(opener) >= 0 ? list.indexOf(opener) + 1 : list.length
        list.splice(at, 0, view)
        tabs = list
        if (url)
            view.url = url
        if (foreground || !currentView)
            select(at)
        return view
    }

    function closeTab(index) {
        if (index < 0 || index >= tabs.length)
            return
        const view = tabs[index]
        const list = tabs.slice()
        list.splice(index, 1)
        if (view === currentView)
            currentView = list.length ? list[Math.min(index, list.length - 1)] : null
        tabs = list
        view.destroy()
        if (list.length === 0)
            win.close()
        else
            currentView.forceActiveFocus()
    }

    function moveTab(offset) {
        const from = currentIndex, to = from + offset
        if (from < 0 || to < 0 || to >= tabs.length)
            return
        const list = tabs.slice()
        list.splice(to, 0, list.splice(from, 1)[0])
        tabs = list
    }

    // MARK: Where links go (the same rules as on macOS and Windows)

    function handleNewWindow(view, request) {
        const url = request.requestedUrl
        const text = url.toString()
        const blank = text === "" || text === "about:blank"
        const destination = blank ? "app" : app.destination(url)
        if (destination === "system") {
            app.openExternally(url)
            return
        }
        // A popup a script opens with a set size, such as "Sign in with Google", stays in the app
        // whatever its site, so it can report back to the page that opened it. Links to other
        // sites go to the browser.
        const popup = request.destination === WebEngineNewWindowRequest.InNewDialog
        if (destination === "browser" && !popup) {
            app.openExternally(url)
            return
        }
        if (request.destination === WebEngineNewWindowRequest.InNewWindow) {
            const window = appRoot.newWindow(null)
            request.openIn(window.currentView)
            return
        }
        const background = request.destination === WebEngineNewWindowRequest.InNewBackgroundTab
        request.openIn(addTab(null, !background, view))
    }

    function handleNavigation(request) {
        const destination = app.destination(request.url)
        if (destination === "system") {
            request.reject()
            app.openExternally(request.url)
        } else if (destination === "browser" && request.isMainFrame
                   && request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation) {
            // Clicked links to other sites go to the browser. Redirects and script navigation
            // stay, so single sign-on and OAuth keep working.
            request.reject()
            app.openExternally(request.url)
        }
    }

    // Only the app's own and allowed sites may ask for anything.
    function handlePermission(permission) {
        if (!app.isListed(permission.origin)) {
            permission.deny()
            return
        }
        switch (permission.permissionType) {
        case WebEnginePermission.PermissionType.Notifications:
            // Always granted, so the ⋮ menu's switch alone decides whether they're shown.
        case WebEnginePermission.PermissionType.ClipboardReadWrite:
        case WebEnginePermission.PermissionType.MouseLock:
        case WebEnginePermission.PermissionType.LocalFontsAccess:
            permission.grant()
            return
        default:
            permissionPrompt.ask(permission)
        }
    }

    // MARK: Page commands

    function find() {
        findBar.open()
    }

    function zoom(factor) {
        if (currentView)
            currentView.zoomFactor = factor === 1 ? 1 : Math.max(0.25, Math.min(5, currentView.zoomFactor * factor))
    }

    function openDevTools() {
        if (!devTools)
            devTools = devToolsComponent.createObject(win)
        devTools.show()
        devTools.raise()
    }

    Component {
        id: tabComponent

        WebEngineView {
            id: view

            property bool painted: false

            anchors.fill: parent
            visible: win.currentView === view
            profile: win.profile
            // The bar's color, not white, until the first page draws.
            backgroundColor: painted ? "white" : win.barColor
            settings.fullScreenSupportEnabled: true
            settings.javascriptCanOpenWindows: true
            settings.javascriptCanAccessClipboard: true
            settings.javascriptCanPaste: true
            settings.screenCaptureEnabled: true
            settings.focusOnNavigationEnabled: true

            onLoadProgressChanged: if (loadProgress >= 60) painted = true
            onLoadingChanged: (request) => {
                if (request.status !== WebEngineView.LoadStartedStatus)
                    painted = true
            }
            onNewWindowRequested: (request) => win.handleNewWindow(view, request)
            onNavigationRequested: (request) => win.handleNavigation(request)
            onPermissionRequested: (permission) => win.handlePermission(permission)
            onFullScreenRequested: (request) => {
                request.accept()
                win.visibility = request.toggleOn ? Window.FullScreen : Window.Windowed
            }
            onWindowCloseRequested: win.closeTab(win.tabs.indexOf(view))
            onFindTextFinished: (result) => findBar.showResult(result)
            onLinkHovered: (url) => win.hoveredLink = url.toString()
            // A crashed or killed page comes back rather than leaving a blank tab.
            onRenderProcessTerminated: (status) => {
                if (status !== WebEngineView.NormalTerminationStatus)
                    reloadAfterCrash.start()
            }

            Timer {
                id: reloadAfterCrash
                interval: 500
                onTriggered: view.reload()
            }
        }
    }

    Component {
        id: devToolsComponent

        Window {
            width: 1000
            height: 700
            title: qsTr("Developer Tools — %1").arg(app.name)

            WebEngineView {
                anchors.fill: parent
                profile: win.profile
                inspectedView: win.currentView
            }
        }
    }

    // MARK: The bar

    header: Rectangle {
        id: bar

        implicitHeight: 36
        color: win.barColor

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                id: tabArea

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                // With one tab, the bar shows the page's title; with more, the tabs.
                Label {
                    visible: win.tabs.length < 2
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    leftPadding: 14
                    text: win.title
                    color: win.barText
                    opacity: 0.85
                    elide: Text.ElideRight
                    font.pixelSize: 13
                }

                Row {
                    visible: win.tabs.length >= 2
                    height: parent.height

                    Repeater {
                        model: win.tabs

                        BrowserTab {
                            required property var modelData
                            required property int index

                            view: modelData
                            width: Math.min(240, (tabArea.width - newTabButton.width) / win.tabs.length)
                            height: tabArea.height
                            selected: modelData === win.currentView
                            textColor: win.barText
                            showDivider: index > 0
                            onActivated: win.select(index)
                            onCloseRequested: win.closeTab(index)
                        }
                    }

                    ToolButton {
                        id: newTabButton
                        width: 36
                        height: parent.height
                        text: "+"
                        font.pixelSize: 18
                        Material.foreground: win.barText
                        onClicked: win.addTab(app.url, true)
                    }
                }
            }

            ToolButton {
                Layout.fillHeight: true
                Layout.preferredWidth: 40
                text: "⋮"
                font.pixelSize: 18
                Material.foreground: win.barText
                onClicked: moreMenu.popup(this, 0, height)

                Menu {
                    id: moreMenu

                    MenuItem { text: qsTr("New Tab"); onTriggered: win.addTab(app.url, true) }
                    MenuItem { text: qsTr("New Window"); onTriggered: win.appRoot.newWindow(app.url) }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Find…"); onTriggered: win.find() }
                    MenuItem { text: qsTr("Reload"); onTriggered: win.currentView.reload() }
                    MenuItem { text: qsTr("Zoom In"); onTriggered: win.zoom(1.1) }
                    MenuItem { text: qsTr("Zoom Out"); onTriggered: win.zoom(1 / 1.1) }
                    MenuItem { text: qsTr("Actual Size"); onTriggered: win.zoom(1) }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Copy Page Link"); onTriggered: app.copyText(win.currentView.url.toString()) }
                    MenuItem { text: qsTr("Open Page in Browser"); onTriggered: app.openExternally(win.currentView.url) }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Notifications")
                        checkable: true
                        checked: app.notificationsEnabled
                        onToggled: app.notificationsEnabled = checked
                    }
                    MenuItem { text: qsTr("Allowed Sites…"); onTriggered: allowedSites.open() }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Developer Tools"); onTriggered: win.openDevTools() }
                    MenuItem { text: qsTr("Keyboard Shortcuts"); onTriggered: shortcutsOverlay.open() }
                    MenuItem { text: qsTr("WebAppWrap %1").arg(app.version); enabled: false }
                }
            }
        }

        // Loading progress, along the bottom of the bar.
        Rectangle {
            anchors.bottom: parent.bottom
            height: 2
            width: win.currentView ? parent.width * win.currentView.loadProgress / 100 : 0
            visible: win.currentView !== null && win.currentView.loading
            color: theme.accent
        }
    }

    // MARK: The page area

    Item {
        id: webArea
        anchors.fill: parent
    }

    // The address of the link under the pointer, as browsers show it.
    Label {
        visible: win.hoveredLink !== ""
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: Math.min(implicitWidth, parent.width * 0.6)
        padding: 4
        leftPadding: 8
        rightPadding: 8
        text: win.hoveredLink
        elide: Text.ElideMiddle
        font.pixelSize: 12
        color: "#202020"
        background: Rectangle { color: "#f2f2f2"; border.color: "#d0d0d0"; radius: Math.min(4, theme.radius) }
    }

    FindInPage {
        id: findBar
        view: win.currentView
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
    }

    AllowedSitesDialog {
        id: allowedSites
        anchors.centerIn: parent
    }

    ShortcutsOverlay {
        id: shortcutsOverlay
        anchors.centerIn: parent
        entries: [
            ["Ctrl+T", qsTr("New tab")],
            ["Ctrl+Click", qsTr("Open a link in a new background tab")],
            ["Ctrl+Shift+Click", qsTr("Open a link in a new tab and switch to it")],
            ["Ctrl+W", qsTr("Close the tab (the last one closes the window)")],
            ["Ctrl+Shift+W", qsTr("Close the window")],
            ["Ctrl+N", qsTr("New window")],
            ["Ctrl+Tab / Ctrl+Shift+Tab", qsTr("Next / previous tab (also Ctrl+L / Ctrl+H)")],
            ["Ctrl+1 … Ctrl+8, Ctrl+9", qsTr("Go to tab N, or to the last tab")],
            ["Ctrl+Shift+PgUp / PgDn", qsTr("Move the tab left / right")],
            ["Alt+← / Alt+→", qsTr("Back / forward")],
            ["Ctrl+Shift+H", qsTr("Go to the app's home page")],
            ["Ctrl+R or F5", qsTr("Reload")],
            ["Ctrl+Shift+R", qsTr("Reload, skipping the cache")],
            ["Ctrl+F", qsTr("Find on the page (Enter: next, Shift+Enter: previous, Esc: close)")],
            ["Ctrl++ / Ctrl+- / Ctrl+0", qsTr("Zoom in, zoom out, actual size")],
            ["Ctrl+Shift+C", qsTr("Copy the page's link")],
            ["Ctrl+Shift+O", qsTr("Open the page in your browser")],
            ["F12", qsTr("Developer tools")],
            ["F1", qsTr("This list")],
        ]
    }

    // Asks before a site uses the camera, microphone, screen or location. The answer is
    // remembered for the site.
    Dialog {
        id: permissionPrompt

        property var permission: null

        function ask(request) {
            permission = request
            open()
        }

        function describe(type) {
            switch (type) {
            case WebEnginePermission.PermissionType.MediaAudioCapture: return qsTr("use your microphone")
            case WebEnginePermission.PermissionType.MediaVideoCapture: return qsTr("use your camera")
            case WebEnginePermission.PermissionType.MediaAudioVideoCapture: return qsTr("use your camera and microphone")
            case WebEnginePermission.PermissionType.DesktopVideoCapture:
            case WebEnginePermission.PermissionType.DesktopAudioVideoCapture: return qsTr("share your screen")
            case WebEnginePermission.PermissionType.Geolocation: return qsTr("know your location")
            default: return qsTr("use a feature of your computer")
            }
        }

        anchors.centerIn: parent
        modal: true
        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }
        background: Rectangle {
            color: Material.dialogColor
            radius: Math.min(12, theme.radius)
        }
        title: permission ? qsTr("Allow %1 to %2?").arg(permission.origin.toString().replace(/^https?:\/\//, "").replace(/\/$/, ""))
                                                   .arg(describe(permission.permissionType)) : ""
        standardButtons: Dialog.Ok | Dialog.Cancel
        Component.onCompleted: {
            standardButton(Dialog.Ok).text = qsTr("Allow")
            standardButton(Dialog.Cancel).text = qsTr("Don’t Allow")
        }
        onAccepted: if (permission) permission.grant()
        onRejected: if (permission) permission.deny()
    }

    // MARK: Shortcuts

    Shortcut { sequences: ["Ctrl+T"]; onActivated: win.addTab(app.url, true) }
    Shortcut { sequences: ["Ctrl+W", "Ctrl+F4"]; onActivated: win.closeTab(win.currentIndex) }
    Shortcut { sequences: ["Ctrl+Shift+W"]; onActivated: win.close() }
    Shortcut { sequences: ["Ctrl+N"]; onActivated: win.appRoot.newWindow(app.url) }
    Shortcut { sequences: ["Ctrl+Tab", "Ctrl+PgDown", "Ctrl+L"]; onActivated: win.select((win.currentIndex + 1) % win.tabs.length) }
    Shortcut { sequences: ["Ctrl+Shift+Tab", "Ctrl+Backtab", "Ctrl+PgUp", "Ctrl+H"]; onActivated: win.select((win.currentIndex - 1 + win.tabs.length) % win.tabs.length) }
    Shortcut { sequences: ["Ctrl+Shift+PgUp"]; onActivated: win.moveTab(-1) }
    Shortcut { sequences: ["Ctrl+Shift+PgDown"]; onActivated: win.moveTab(1) }
    Shortcut { sequences: ["Ctrl+9"]; onActivated: win.select(win.tabs.length - 1) }
    Instantiator {
        model: 8
        delegate: Shortcut {
            required property int index
            sequences: ["Ctrl+" + (index + 1)]
            onActivated: win.select(index)
        }
    }
    Shortcut { sequences: ["Alt+Left", "Back"]; onActivated: if (win.currentView) win.currentView.goBack() }
    Shortcut { sequences: ["Alt+Right", "Forward"]; onActivated: if (win.currentView) win.currentView.goForward() }
    Shortcut { sequences: ["Ctrl+Shift+H"]; onActivated: if (win.currentView) win.currentView.url = app.url }
    Shortcut { sequences: ["Ctrl+R", "F5"]; onActivated: if (win.currentView) win.currentView.reload() }
    Shortcut { sequences: ["Ctrl+Shift+R", "Shift+F5"]; onActivated: if (win.currentView) win.currentView.reloadAndBypassCache() }
    Shortcut { sequences: ["Ctrl+F"]; onActivated: win.find() }
    Shortcut { sequences: ["Ctrl+=", "Ctrl++", "Ctrl+Shift+="]; onActivated: win.zoom(1.1) }
    Shortcut { sequences: ["Ctrl+-"]; onActivated: win.zoom(1 / 1.1) }
    Shortcut { sequences: ["Ctrl+0"]; onActivated: win.zoom(1) }
    Shortcut { sequences: ["Ctrl+Shift+C"]; onActivated: if (win.currentView) app.copyText(win.currentView.url.toString()) }
    Shortcut { sequences: ["Ctrl+Shift+O"]; onActivated: if (win.currentView) app.openExternally(win.currentView.url) }
    Shortcut { sequences: ["F12", "Ctrl+Shift+I"]; onActivated: win.openDevTools() }
    Shortcut { sequences: ["F1"]; onActivated: shortcutsOverlay.open() }
    Shortcut {
        sequences: ["Esc"]
        enabled: win.visibility === Window.FullScreen
        onActivated: if (win.currentView) win.currentView.triggerWebAction(WebEngineView.ExitFullScreen)
    }
}
