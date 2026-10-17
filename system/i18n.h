#ifndef EC_I18N_H
#define EC_I18N_H

typedef enum {
    LANG_EN = 0,
    LANG_FR,
    LANG_DE,
    LANG_IT,
    LANG_PT,
    LANG_ZH,
    LANG_ZH_TW,
    LANG_JA,
    LANG_KO,
    LANG_RU,
    LANG_COUNT
} lang_t;

typedef enum {
    STR_MENU_CREATE_FILE = 0,
    STR_MENU_OPEN_DOS,
    STR_MENU_REFRESH,
    STR_MENU_WALLPAPER,
    STR_MENU_SHUTDOWN,
    STR_MENU_OPEN,
    STR_MENU_DELETE,
    STR_MENU_RENAME,
    STR_MENU_COPY,
    STR_HINT_TASKBAR,
    STR_TITLE_SETTINGS,
    STR_TITLE_FILES,
    STR_TITLE_DOS,
    STR_SET_LANGUAGE,
    STR_SET_APPEARANCE,
    STR_SET_AUTOSTART,
    STR_SET_APPLY,
    STR_SET_CLOSE,
    STR_FE_BACK,
    STR_FE_ITEMS,
    STR_MSG_CREATE_FAIL,
    STR_MSG_UNKNOWN_APP,
    STR_MSG_SHUTDOWN_OK,
    STR_MSG_SHUTDOWN_FAIL,
    STR_COUNT
} str_id_t;

void        i18n_init(void);
void        i18n_set_lang(lang_t lang);
lang_t      i18n_get_lang(void);
const char *i18n_str(str_id_t id);
const char *i18n_lang_name(lang_t lang);
int         i18n_lang_is_placeholder(lang_t lang);

#endif
