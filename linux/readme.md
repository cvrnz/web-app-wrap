# WebAppWrap for Linux

A desktop app that turns any web app into its own Linux app, with real tabs.

Give it a name, a web address, and optionally an SVG icon. It makes an app with:

- an entry in the app launcher, with its own icon
- its own windows, which Hyprland and other compositors treat as a separate
  app (its own Wayland app ID)
- its own cookies, sign-ins and storage
- the site's notifications, shown by the desktop's notification service

This is the Linux version, built with C++, Qt 6 and Qt WebEngine (Chromium), the
way Omarchy's own apps are built. The macOS version is in [../macos](../macos)
and the Windows version in [../windows](../windows). Run the commands below from
this `linux` folder.

## Tabs in wrapped apps

The window has a single bar at the top, in the title bar color you pick, with a
**⋮** (More) button on the right. With one tab open, the bar shows the page's
title. With two or more, it shows the tabs, up to 240px wide each, and a `+`
for a new one. Every tab uses the bar's color, and the selected tab has
brighter text and a close button.

| Action | Result |
| --- | --- |
| Ctrl+T, or the `+` after the last tab | New tab at the base URL |
| Ctrl+click a link (or middle-click) | Opens in a new background tab |
| Ctrl+Shift+click a link | Opens in a new tab and switches to it |
| Ctrl+W, or the tab's × (middle-click on the tab also works) | Closes the tab. Closing the last tab closes the window |
| Ctrl+Shift+W | Closes the window and all its tabs |
| Ctrl+N | New window |
| Ctrl+Tab / Ctrl+Shift+Tab (also Ctrl+L / Ctrl+H, Ctrl+PgDn / Ctrl+PgUp) | Next / previous tab |
| Ctrl+1 … Ctrl+8, Ctrl+9 | Go to tab N, or to the last tab |
| Ctrl+Shift+PgUp / Ctrl+Shift+PgDn | Move the tab left / right |
| Alt+← / Alt+→ | Back / forward |
| Ctrl+Shift+H | Go to the base URL |
| Ctrl+R or F5 | Reload |
| Ctrl+Shift+R | Reload, skipping the cache |
| Ctrl+F | Find on the page. Enter goes to the next match, Shift+Enter to the previous one, Esc closes it |
| Ctrl++ / Ctrl+- / Ctrl+0 | Zoom in, zoom out, actual size |
| Ctrl+Shift+C | Copy the page's link |
| Ctrl+Shift+O | Open the page in your default browser |
| F12 or Ctrl+Shift+I | Developer tools |
| F1 | List these shortcuts |

The **⋮** menu has the page commands above, plus:

- **Notifications**: a switch, on by default, for showing the site's
  notifications. Clicking one tells the page, as a browser would, and brings
  the app and its tab forward. Some sites also need notifications turned on in
  their own settings, for example Gmail's **Settings → Desktop
  notifications**.
- **Allowed Sites…**: other sites the app may open, each with its subdomains.
  Type a domain (`github.com`) or paste a link and click **Add**. Each app has
  its own list, and changes apply to open tabs straight away.
- **Copy Cookies from Helium**: copies Helium's cookies into the app again
  and reloads its tabs, for when you've signed in to something in Helium while
  the app was open.

Every app copies Helium's cookies each time it opens, before its first page
loads, so it's signed in wherever Helium is. This is for sites that won't let
you sign in inside WebAppWrap: Google's sign-in (used by YouTube) says "This
browser or app may not be secure" for embedded browsers. It's a one-way copy:
signing in or out in the app doesn't reach Helium, and Helium's cookies replace
the app's for the same site at the next launch. It reads Helium's "Chromium
Safe Storage" password from the keyring with `secret-tool`, so the keyring
must be unlocked; if it can't copy, the app opens with its own cookies.

Launching an app that's already running brings its window forward instead of
starting a second copy. On Hyprland, WebAppWrap asks Hyprland to focus the
window, since Hyprland ignores activation requests unless
`misc:focus_on_activate` is on.

### Which links stay in the app

The routing works the same way as on macOS and Windows:

- **In the app:** the base URL's host and its subdomains (`www.` is ignored),
  and the sites in **⋮ → Allowed Sites…** and their subdomains.
- **In your default browser:** any clicked link to a different host.
- **In the app that handles the link:** `mailto:`, `tel:`, `zoommtg:` and other
  non-web links.

Redirects and script-driven navigation to other hosts stay in the app, so
single sign-on (SSO) and OAuth sign-in work. A sign-in popup a script opens
with a set size, such as "Sign in with Google", opens in a tab inside the app,
so it can report back to the page that opened it.

Only the app's own and allowed sites can ask for permissions. Notifications,
the clipboard and pointer lock are allowed for them; the camera, microphone,
screen sharing and location ask first, and the answer is remembered. Downloads
go to `~/Downloads`, with a notification when each one finishes. Click it to
open the file. A page that crashes reloads by itself.

## Requirements

- Linux with Qt 6.9 or later and Qt WebEngine. Omarchy (Arch) has everything:
  `qt6-base`, `qt6-declarative`, `qt6-svg`, `qt6-wayland` and `qt6-webengine`.
- A notification service (on Omarchy, the one the desktop already runs) and
  xdg-desktop-portal, for the file chooser.
- To build: `qmake6`, `make` and a C++20 compiler (`base-devel`).

Qt WebEngine's Chromium is usually a few versions behind Chrome, and it doesn't
include Widevine, so sites that need DRM for video, such as Netflix, won't play
it. Most web apps (mail, chat, documents, calendars) work as in Chrome.

## Build and install

```sh
./scripts/build     # creates build/webappwrap, versioned with the current commit
./scripts/test      # runs the tests offscreen
./scripts/install   # builds the Arch package and installs it (asks for your password)
```

Installing puts `webappwrap` and `waw` (the same program) in `/usr/bin` and
**WebAppWrap** in the app launcher.

## Use

Open **WebAppWrap** from the app launcher. Its window, **My Apps**, lists every
app made in it, with its icon, address and whether it's up to date.

- **Add App** (or `+`) adds an app. Adding doesn't build it. Fill in:
  1. The app name and the web address. You can leave out `https://`. Addresses
     that start with `localhost` use `http://`.
  2. Optionally, an icon: drop an SVG on the icon or click **Choose SVG…**.
     WebAppWrap draws it on a solid background (#1C1C1C unless you pick
     another), as a rounded square like Omarchy's own icons. **One Color**
     fills the whole SVG with a color you pick. At zoom 1 the whole SVG fits;
     drag left to shrink it, or right to zoom in and crop. Without an SVG,
     WebAppWrap draws a colored tile with the name's first letter.
  3. The **Title bar color**. After you enter the web address, WebAppWrap
     fills it in with the site's theme color, from its
     `<meta name="theme-color">` tag or its web app manifest. Pick another color
     if you like; **Use Site Color** switches back. Sites with no theme color get
     `#1C1C1C`. The bar's text turns light or dark to stay readable.
- Double-click an app, or select it and press Enter, to edit it.
- Delete takes an app off the list. The app itself stays installed.
- Right-click an app to build just that one, open it, or uninstall it.
  Uninstalling removes its launcher entry and icon, and keeps its sign-ins in
  case you build it again.
- **Build Apps** (or Space) builds every app that needs it, one at a time.
  Each row shows its progress and any error.

An app is **skipped** when nothing about it has changed since it was last built
and it was built by this version of WebAppWrap. It's rebuilt if it's no longer
installed. WebAppWrap's version is the git commit it was built from
(`scripts/build` writes it in, with `-dirty` for uncommitted changes), so after
you update WebAppWrap, **Build Apps** rebuilds every app. Builds made without
`scripts/build` have no version and always rebuild.

Rebuilding an app, for example to change its icon or address, keeps its
sign-ins, because they're tied to its name. Press `?` for the window's
keyboard shortcuts.

Names can include folders (`WebAppWrap/Photos`), as on macOS and Windows, so the
same list and batch files work everywhere. Linux launchers have no folders, so
on Linux only the last part names the app.

### Where things are kept

| Path | What it holds |
| --- | --- |
| `~/.local/share/webappwrap/apps.json` | Every app in My Apps: its settings, and when, from which settings and by which WebAppWrap version it was last built. The same format as on macOS |
| `~/.local/share/webappwrap/icons/` | A copy of each app's icon SVG |
| `~/.local/share/webappwrap/previous-icons/` | Icons that were replaced, or whose app was removed, named after the app and the time |
| `~/.local/share/webappwrap/apps/<app>/` | One built app: `app.json` (its name, address, color and version), `icon.png`, `settings.json` (the Notifications switch and Allowed Sites) and `profile/` (its cookies, sign-ins and storage) |
| `~/.local/share/applications/webappwrap-<app>.desktop` | Its launcher entry |
| `~/.cache/webappwrap/<app>/` | Its web cache |

`<app>` is the name in lowercase with dashes: `My Mail` is `my-mail`. These
follow `$XDG_DATA_HOME` and `$XDG_CACHE_HOME` when they're set.

### Many apps at once (JSON)

`waw batch` builds every app in a JSON file, in the same format as on macOS and
Windows. Apps built from the command line aren't added to My Apps.

```json
[
  {
    "name": "WebAppWrap/Photos",
    "url": "https://kphotos.vercel.app",
    "image": "icons/photos.svg",
    "zoom": 0.8,
    "iconBackground": "#ffffff",
    "iconColor": "#e5e5e5",
    "titleBarColor": "#1a1a1a"
  },
  { "name": "Linear", "url": "linear.app" }
]
```

| Field | Required | Meaning |
| --- | --- | --- |
| `name` | yes | App name. It may include folders, which Linux ignores |
| `url` | yes | Web address |
| `image` | no | Icon SVG path, relative to the JSON file unless absolute (`~` works). Other images, such as PNG, are cropped to fill the icon, zooming past any plain border |
| `zoom` | no | 0.5–3. At 1 the whole SVG fits in the icon. For other images, 1–3 on top of the automatic border crop |
| `iconBackground` | no | The color behind the SVG. Defaults to `#1C1C1C` |
| `iconColor` | no | Fills the whole SVG with one color. Without it, the SVG keeps its own colors |
| `titleBarColor` | no | Defaults to the site's theme color |

The file can also be JSONC or JSON5, so comments, trailing commas, unquoted
keys and single-quoted strings are fine. Before building, WebAppWrap checks
the whole file and lists every problem at once. If any apps fail to build, it
lists them at the end without stopping the rest.

### Command line

```sh
waw build --name "Linear" --url linear.app
waw build --name "Grafana" --url localhost:3000 --image ~/grafana.svg --zoom 0.8 \
  --icon-background "#ffffff" --icon-color "#e5e5e5" --title-bar-color "#1a1a1a"
waw batch ~/apps.json            # every app in a JSON file
waw icon --name "Linear" --out Linear.png --size 512   # just the icon
waw list                         # installed apps (--json for scripts)
waw remove "Linear"              # keeps its sign-ins; --delete-data removes them too
waw help
```

`build` and `batch` also take `--json` to print their results as JSON. Every
command exits 0 on success and 1 on failure, with the errors on stderr.

## How it works

There's one program, `webappwrap`. With no arguments it opens the window;
`webappwrap run <app>` runs an app; the other commands make and manage apps.
Making an app writes three things: its `app.json`, its icon as a 512px PNG,
and a desktop entry that runs `webappwrap run <app>` with
`StartupWMClass=webappwrap-<app>`. Nothing is copied per app, so updating
WebAppWrap updates every app.

A running app sets its Wayland app ID to `webappwrap-<app>`, which matches its
desktop entry, so the compositor and launcher show its own name and icon. Each
app has its own persistent Qt WebEngine profile, stored in its own folder. The
user agent is Chrome's, without the `QtWebEngine/<version>` that Qt adds,
because some sites turn away browsers they don't recognize.

Notifications go to `org.freedesktop.Notifications` over D-Bus, with the app's
icon and the page's own image. The page's tag decides whether a newer
notification replaces an older one.

## Source

| File | Job |
| --- | --- |
| `src/main.cpp` | Picks the window, an app, or the command line |
| `src/appregistry.*`, `batchfile.*`, `json5.*`, `inputnormalizer.*`, `hexcolor.*`, `linkpolicy.*`, `themecolor.*` | The list, batch files and rules shared with macOS and Windows |
| `src/appbuilder.*`, `iconrenderer.*`, `paths.*` | Making apps and their icons, and where files go |
| `src/runtime.*`, `notifier.*` | A running app: its settings, links, notifications, single instance and focus |
| `src/applistmodel.*`, `appdraft.*`, `portalfilepicker.*` | The My Apps window |
| `src/cli.*` | `waw` |
| `src/theme.*`, `translator.*` | Follow the Omarchy accent, window rounding and language (from the Omarchy app template, after Omacut, MIT, David Heinemeier Hansson) |
| `src/qml/` | The windows |

Only English is shipped. Every string goes through `qsTr`/`tr`, and catalogs
for other languages can be added to `src/i18n/`.

Source code and issues: https://github.com/cvrnz/web-app-wrap
