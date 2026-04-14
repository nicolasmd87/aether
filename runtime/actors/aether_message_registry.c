#include "aether_message_registry.h"
#include <stdlib.h>
#include <string.h>

// Reserved system message ID shared with the runtime scheduler.
// Must match MSG_IO_READY in runtime/scheduler/multicore_scheduler.h.
// When a user defines `message IoReady { fd: int, events: int }`, the
// registry assigns this ID so the generated dispatch table routes
// scheduler-delivered I/O readiness notifications to the user's handler.
#define AETHER_RESERVED_ID_IO_READY 255

MessageRegistry* create_message_registry(void) {
    MessageRegistry* registry = (MessageRegistry*)malloc(sizeof(MessageRegistry));
    registry->messages = NULL;
    registry->next_id = 0;  // Start at 0 for 0-255 range
    return registry;
}

void free_message_registry(MessageRegistry* registry) {
    MessageDef* msg = registry->messages;
    while (msg) {
        MessageDef* next = msg->next;
        
        MessageFieldDef* field = msg->fields;
        while (field) {
            MessageFieldDef* next_field = field->next;
            free(field->name);
            free(field->c_type);
            free(field->element_c_type);
            free(field);
            field = next_field;
        }
        
        free(msg->name);
        free(msg);
        msg = next;
    }
    free(registry);
}

int register_message_type(MessageRegistry* registry, const char* name, MessageFieldDef* fields) {
    MessageDef* existing = lookup_message(registry, name);
    if (existing) {
        return existing->message_id;
    }

    MessageDef* def = (MessageDef*)malloc(sizeof(MessageDef));
    if (!def) return -1;
    def->name = strdup(name);
    if (!def->name) { free(def); return -1; }

    // Reserved system IDs: `IoReady` must land on the scheduler's
    // MSG_IO_READY slot so await_io()-driven notifications dispatch
    // through the user's receive handler.
    if (strcmp(name, "IoReady") == 0) {
        def->message_id = AETHER_RESERVED_ID_IO_READY;
    } else {
        // Skip any reserved slot so user messages never collide with
        // system IDs. With only one reserved slot (255), user IDs top out
        // at 254, which is far beyond any realistic program.
        if (registry->next_id == AETHER_RESERVED_ID_IO_READY) {
            registry->next_id++;
        }
        def->message_id = registry->next_id++;
    }

    def->fields = fields;
    def->next = registry->messages;
    registry->messages = def;

    return def->message_id;
}

MessageDef* lookup_message(MessageRegistry* registry, const char* name) {
    MessageDef* msg = registry->messages;
    while (msg) {
        if (strcmp(msg->name, name) == 0) {
            return msg;
        }
        msg = msg->next;
    }
    return NULL;
}

MessageDef* lookup_message_by_id(MessageRegistry* registry, int id) {
    MessageDef* msg = registry->messages;
    while (msg) {
        if (msg->message_id == id) {
            return msg;
        }
        msg = msg->next;
    }
    return NULL;
}

MessageInstance* create_message_instance(MessageDef* def) {
    MessageInstance* inst = (MessageInstance*)malloc(sizeof(MessageInstance));
    inst->message_id = def->message_id;
    
    int field_count = 0;
    MessageFieldDef* field = def->fields;
    while (field) {
        field_count++;
        field = field->next;
    }
    
    inst->field_count = field_count;
    inst->field_values = (void**)calloc(field_count, sizeof(void*));
    
    return inst;
}

void free_message_instance(MessageInstance* msg) {
    free(msg->field_values);
    free(msg);
}
