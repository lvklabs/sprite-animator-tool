#ifndef SETTINGS_H
#define SETTINGS_H

#define APP_NAME "LVK Sprite Animation Tool"
// Keep in sync with CMakeLists.txt project(VERSION ...) until refactored to a configured header.
#define APP_VERSION "2.0.1"
#define APP_ABOUT                                                                                  \
    "<b>" APP_NAME " " APP_VERSION "</b><br/>"                                                     \
    "(c) 2010-2012 LVK Labs; 2026 modernization by Pablo Perez De Angelis<br/><br/>"               \
    "This software is FREE."                                                                       \
    "This software is distributed in the hope that it will be useful, "                            \
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "                              \
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.<br/><br/>"                               \
    "License: " LVK_LICENSE "<br/>"                                                                \
    "Fork: <a href=\"" LVK_FORK_URL "\">" LVK_FORK_URL "</a><br/>"                                 \
    "<a href=\"" LVK_SITE "\">" LVK_SITE "</a><br/>"                                               \
    "<a href=\"mailto:" LVK_EMAIL "\">" LVK_EMAIL "</a>"

#define LVK_NAME "LVK"
#define LVK_DOMAIN "lvklabs.com"
#define LVK_SITE "http://www." LVK_DOMAIN
#define LVK_EMAIL "contact@" LVK_DOMAIN
// Active fork URL + license string, surfaced in the About dialog so users
// can find the maintained source and the licensing terms. The legacy
// LVK_SITE/LVK_EMAIL above are kept for attribution to the original authors.
#define LVK_FORK_URL "https://github.com/lvklabs/sprite-animator-tool"
#define LVK_LICENSE "GPL-3.0-or-later"

#define MAX_RECENT_FILES 10
#define KEY_RECENT_FILE "RecentFiles/filename"

#ifdef DEBUG_UNDO
#define MAX_UNDO_TIMES 5
#else
#define MAX_UNDO_TIMES 200
#endif

#endif // SETTINGS_H
