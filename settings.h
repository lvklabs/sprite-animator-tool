#ifndef SETTINGS_H
#define SETTINGS_H

#define APP_NAME                "Lvk Sprite Animation Tool"
#define APP_VERSION             "0.1"

#define MAX_RECENT_FILES        10
#define KEY_RECENT_FILE         "RecentFiles/filename"

#ifdef DEBUG_UNDO
#  define MAX_UNDO_TIMES          5
#else
#  define MAX_UNDO_TIMES          200
#endif

#endif // SETTINGS_H
