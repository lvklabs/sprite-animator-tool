#ifndef SETTINGS_H
#define SETTINGS_H

#define APP_NAME                "LVK Sprite Animation Tool"
#define APP_VERSION             "1.2.1"
#define APP_ABOUT               "<b>" APP_NAME " " APP_VERSION "</b><br/>"\
                                "(c) 2010, 2011, 2012 LVK<br/><br/>"\
                                "This software is FREE."\
                                "This software is distributed in the hope that it will be useful, "\
                                "but WITHOUT ANY WARRANTY; without even the implied warranty of "\
                                "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.<br/><br/>"\
                                "<a href=\"" LVK_SITE "\">" LVK_SITE "</a><br/>"\
                                "<a href=\"mailto:" LVK_EMAIL "\">" LVK_EMAIL "</a>"

#define LVK_NAME                "LVK"
#define LVK_DOMAIN              "lvklabs.com"
#define LVK_SITE                "http://www." LVK_DOMAIN
#define LVK_EMAIL               "contact@" LVK_DOMAIN

#define MAX_RECENT_FILES        10
#define KEY_RECENT_FILE         "RecentFiles/filename"

#ifdef DEBUG_UNDO
#  define MAX_UNDO_TIMES          5
#else
#  define MAX_UNDO_TIMES          200
#endif

#endif // SETTINGS_H
