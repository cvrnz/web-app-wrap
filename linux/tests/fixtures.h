#pragma once

// Fixtures for webappwrap_tests.cpp.

inline constexpr const char json5Sample[] = R"(// comment
            [ { name: 'A', url: "b", /* block */ zoom: .5, hex: 0x10, big: +Infinity, }, ])";

inline constexpr const char batchFile[] = R"([
            { "name": "WebAppWrap/Photos", "url": "kphotos.vercel.app", "image": "icons/p.svg", "zoom": 0.8,
              "iconBackground": "#fff", "iconColor": "#e5e5e5", "titleBarColor": "#1a1a1a" },
            { name: 'Linear', url: 'linear.app' }, // trailing comma next
        ])";

inline constexpr const char batchWithProblems[] = R"([
            { "name": "A", "url": "a.com", "colour": "#fff" },
            { "url": "nope nope" },
            { "name": "B", "url": "b.com", "zoom": 9, "titleBarColor": "blue-ish" },
            { "name": "a", "url": "c.com" },
            "oops"
        ])";

inline constexpr const char batchDuplicateNames[] = R"([{ "name": "X/Mail", "url": "a.com" }, { "name": "mail", "url": "b.com" }])";

inline constexpr const char macAppsJson[] = R"({"apps" : [{"id" : "E621E1F8-C36C-495A-93FC-0C247A3E6E5F", "settings" : {"name" : "WebAppWrap/Photos",
            "url" : "kphotos.vercel.app", "zoom" : 0.8, "icon" : "e621-1234.svg", "iconBackground" : "#FFFFFF"}}]})";

inline constexpr const char htmlPlainAndDark[] = R"x(<meta name="theme-color" media="(prefers-color-scheme: dark)" content="#000">
                                         <meta content='#ff0000' name='theme-color'>)x";

inline constexpr const char htmlLightAndDark[] = R"x(<meta name="theme-color" media="(prefers-color-scheme: dark)" content="#000">
                                         <meta name="theme-color" media="(prefers-color-scheme: light)" content="#fff">)x";

inline constexpr const char htmlManifestLink[] = R"x(<link rel="icon manifest" href="/app.webmanifest">)x";

inline constexpr const char manifestJson[] = R"x({"theme_color": "rgb(0, 0, 0)"})x";

inline constexpr const char svgSquare[] = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><rect x="2" y="2" width="6" height="6" fill="%1"/></svg>)";

