#include "toml_parser.h"
#include <ctype.h>

static char* trim(char* str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static char* remove_quotes(char* str) {
    if (!str) return str;
    size_t len = strlen(str);
    if (len >= 2 && ((str[0] == '"' && str[len-1] == '"') ||
                     (str[0] == '\'' && str[len-1] == '\''))) {
        str[len-1] = '\0';
        return str + 1;
    }
    return str;
}

TomlDocument* toml_parse_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    TomlDocument* doc = calloc(1, sizeof(TomlDocument));
    if (!doc) { fclose(f); return NULL; }
    int section_capacity = 16;
    doc->sections = calloc(section_capacity, sizeof(TomlSection));
    if (!doc->sections) { fclose(f); free(doc); return NULL; }
    doc->section_count = 0;

    char line[512];
    TomlSection* current_section = NULL;
    int entry_capacity = 0;  // per-section capacity

    while (fgets(line, sizeof(line), f)) {
        char* trimmed = trim(line);

        // Skip empty lines and comments
        if (trimmed[0] == '\0' || trimmed[0] == '#') continue;

        // Section header [section]
        if (trimmed[0] == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                char* section_name = trim(trimmed + 1);

                // Grow sections array if needed
                if (doc->section_count >= section_capacity) {
                    int new_cap = section_capacity * 2;
                    TomlSection* new_secs = realloc(doc->sections, (size_t)new_cap * sizeof(TomlSection));
                    if (!new_secs) { fclose(f); return doc; }
                    doc->sections = new_secs;
                    section_capacity = new_cap;
                }

                // Create new section
                current_section = &doc->sections[doc->section_count++];
                entry_capacity = 32;
                current_section->name = strdup(section_name);
                if (!current_section->name) { current_section->name = NULL; continue; }
                current_section->entries = calloc(entry_capacity, sizeof(TomlKeyValue));
                current_section->entry_count = 0;
            }
            continue;
        }

        // Key = value
        char* eq = strchr(trimmed, '=');
        if (eq && current_section) {
            *eq = '\0';
            char* key = trim(trimmed);
            char* value = trim(eq + 1);
            value = remove_quotes(value);

            // Grow entries array if needed
            if (current_section->entry_count >= entry_capacity) {
                int new_cap = entry_capacity * 2;
                TomlKeyValue* new_entries = realloc(current_section->entries, (size_t)new_cap * sizeof(TomlKeyValue));
                if (!new_entries) { fclose(f); return doc; }
                current_section->entries = new_entries;
                entry_capacity = new_cap;
            }

            TomlKeyValue* entry = &current_section->entries[current_section->entry_count++];
            entry->key = strdup(key);
            entry->value = strdup(value);
            if (!entry->key || !entry->value) {
                // Roll back partial entry
                free(entry->key);
                free(entry->value);
                current_section->entry_count--;
            }
        }
    }

    fclose(f);
    return doc;
}

const char* toml_get_value(TomlDocument* doc, const char* section, const char* key) {
    if (!doc) return NULL;

    for (int i = 0; i < doc->section_count; i++) {
        if (doc->sections[i].name && strcmp(doc->sections[i].name, section) == 0) {
            for (int j = 0; j < doc->sections[i].entry_count; j++) {
                if (doc->sections[i].entries[j].key &&
                    strcmp(doc->sections[i].entries[j].key, key) == 0) {
                    return doc->sections[i].entries[j].value;
                }
            }
        }
    }
    return NULL;
}

TomlKeyValue* toml_get_section_entries(TomlDocument* doc, const char* section, int* count) {
    if (!doc || !count) return NULL;
    *count = 0;

    for (int i = 0; i < doc->section_count; i++) {
        if (doc->sections[i].name && strcmp(doc->sections[i].name, section) == 0) {
            *count = doc->sections[i].entry_count;
            return doc->sections[i].entries;
        }
    }
    return NULL;
}

void toml_free_document(TomlDocument* doc) {
    if (!doc) return;

    for (int i = 0; i < doc->section_count; i++) {
        free(doc->sections[i].name);
        for (int j = 0; j < doc->sections[i].entry_count; j++) {
            free(doc->sections[i].entries[j].key);
            free(doc->sections[i].entries[j].value);
        }
        free(doc->sections[i].entries);
    }
    free(doc->sections);
    free(doc);
}
