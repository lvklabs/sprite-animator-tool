#ifndef SETTINGS_H
#define SETTINGS_H

#define APP_NAME                "Lvk Sprite Animation Tool"
#define APP_VERSION             "1.1"
#define APP_ABOUT               APP_NAME " " APP_VERSION "\n"\
                                "(c) 2010 LavandaInk\n\n"\
                                "This software is only for LavandaInk internal use.\n\n"\
                                LVK_SITE "\n"\
                                LVK_EMAIL

#define LVK_NAME                "LavandaInk"
#define LVK_DOMAIN              "lavandaink.com.ar"
#define LVK_SITE                "http://www." LVK_DOMAIN
#define LVK_EMAIL               "contact@lavandaink.com.ar"

#define MAX_RECENT_FILES        10
#define KEY_RECENT_FILE         "RecentFiles/filename"

#ifdef DEBUG_UNDO
#  define MAX_UNDO_TIMES          5
#else
#  define MAX_UNDO_TIMES          200
#endif

#endif // SETTINGS_H
