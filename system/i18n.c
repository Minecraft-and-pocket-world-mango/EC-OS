#include "i18n.h"
#include "lib.h"

static const char *S_EN[STR_COUNT] = {
    "Create File",
    "Open in DOS",
    "Refresh",
    "Next wallpaper",
    "Shutdown",
    "Open",
    "Delete",
    "Rename",
    "Copy",
    "Right-click: menu  |  ESC: exit",
    "Settings",
    "File Explorer",
    "MiniDOS",
    "Language",
    "Appearance",
    "Auto-start Desktop",
    "Apply",
    "Close",
    "Back",
    "items",
    "Create failed",
    "Unknown AFM application",
    "Shutting down...",
    "Shutdown unavailable"
};

static const char *S_FR[STR_COUNT] = {
    "Creer un fichier",
    "Ouvrir en DOS",
    "Actualiser",
    "Fond d'ecran suivant",
    "Eteindre",
    "Ouvrir",
    "Supprimer",
    "Renommer",
    "Copier",
    "Clic droit : menu  |  ECHAP : quitter",
    "Parametres",
    "Explorateur de fichiers",
    "MiniDOS",
    "Langue",
    "Apparence",
    "Demarrage auto du bureau",
    "Appliquer",
    "Fermer",
    "Retour",
    "elements",
    "Echec de creation",
    "Application AFM inconnue",
    "Extinction...",
    "Extinction indisponible"
};

static const char *S_DE[STR_COUNT] = {
    "Datei erstellen",
    "In DOS oeffnen",
    "Aktualisieren",
    "Naechstes Hintergrundbild",
    "Herunterfahren",
    "Oeffnen",
    "Loeschen",
    "Umbenennen",
    "Kopieren",
    "Rechtsklick: Menue  |  ESC: beenden",
    "Einstellungen",
    "Datei-Explorer",
    "MiniDOS",
    "Sprache",
    "Erscheinungsbild",
    "Desktop automatisch starten",
    "Anwenden",
    "Schliessen",
    "Zurueck",
    "Elemente",
    "Erstellen fehlgeschlagen",
    "Unbekannte AFM-Anwendung",
    "Herunterfahren...",
    "Herunterfahren nicht verfuegbar"
};

static const char *S_IT[STR_COUNT] = {
    "Crea file",
    "Apri in DOS",
    "Aggiorna",
    "Sfondo successivo",
    "Spegni",
    "Apri",
    "Elimina",
    "Rinomina",
    "Copia",
    "Tasto destro: menu  |  ESC: esci",
    "Impostazioni",
    "Esplora file",
    "MiniDOS",
    "Lingua",
    "Aspetto",
    "Avvio automatico desktop",
    "Applica",
    "Chiudi",
    "Indietro",
    "elementi",
    "Creazione non riuscita",
    "Applicazione AFM sconosciuta",
    "Spegnimento...",
    "Spegnimento non disponibile"
};

static const char *S_PT[STR_COUNT] = {
    "Criar arquivo",
    "Abrir no DOS",
    "Atualizar",
    "Proximo plano de fundo",
    "Desligar",
    "Abrir",
    "Excluir",
    "Renomear",
    "Copiar",
    "Clique direito: menu  |  ESC: sair",
    "Configuracoes",
    "Explorador de arquivos",
    "MiniDOS",
    "Idioma",
    "Aparencia",
    "Iniciar desktop automaticamente",
    "Aplicar",
    "Fechar",
    "Voltar",
    "itens",
    "Falha ao criar",
    "Aplicativo AFM desconhecido",
    "Desligando...",
    "Desativacao indisponivel"
};

static const char *const *i18n_table[LANG_COUNT] = {
    S_EN, S_FR, S_DE, S_IT, S_PT,
    S_EN, S_EN, S_EN, S_EN, S_EN
};

static const char *lang_codes[LANG_COUNT] = {
    "EN", "FR", "DE", "IT", "PT", "ZH", "TW", "JA", "KO", "RU"
};

static const char *lang_names[LANG_COUNT] = {
    "English", "Francais", "Deutsch", "Italiano", "Portugues",
    "Chinese (S)", "Chinese (T)", "Japanese", "Korean", "Russian"
};

static lang_t current_lang = LANG_EN;
static char   placeholder[96];

void i18n_init(void)
{
    current_lang = LANG_EN;
}

void i18n_set_lang(lang_t lang)
{
    if ((int)lang < 0 || lang >= LANG_COUNT)
        return;
    current_lang = lang;
}

lang_t i18n_get_lang(void)
{
    return current_lang;
}

int i18n_lang_is_placeholder(lang_t lang)
{
    return lang >= LANG_ZH;
}

const char *i18n_lang_name(lang_t lang)
{
    if ((int)lang < 0 || lang >= LANG_COUNT)
        return "?";
    return lang_names[lang];
}

const char *i18n_str(str_id_t id)
{
    const char *s;
    int i = 0;

    if ((int)id < 0 || id >= STR_COUNT)
        return "";

    s = i18n_table[current_lang][id];

    if (!i18n_lang_is_placeholder(current_lang))
        return s;

    placeholder[i++] = '[';
    placeholder[i++] = lang_codes[current_lang][0];
    placeholder[i++] = lang_codes[current_lang][1];
    placeholder[i++] = ']';
    placeholder[i++] = ' ';

    while (*s && i < (int)sizeof(placeholder) - 1)
        placeholder[i++] = *s++;
    placeholder[i] = '\0';

    return placeholder;
}
