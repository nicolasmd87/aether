#ifndef AETHER_SHARED_MAP_H
#define AETHER_SHARED_MAP_H

#include <stdint.h>

// Opaque shared map handle
typedef struct AetherSharedMap AetherSharedMap;

// Create a new shared map. Returns a token for hosted language access.
AetherSharedMap* aether_shared_map_new(uint64_t* out_token);

// Aether-side: put/get (no token needed — caller owns the map)
void aether_shared_map_put(AetherSharedMap* map, const char* key, const char* value);
const char* aether_shared_map_get(AetherSharedMap* map, const char* key);
void aether_shared_map_free(AetherSharedMap* map);

// Hosted-language-side: get inputs and put outputs via token.
// Phase 1 (during hosted execution): get = reads inputs, put = writes to output area
// After revoke: token is dead, no access.
const char* aether_shared_map_get_by_token(uint64_t token, const char* key);
int aether_shared_map_put_by_token(uint64_t token, const char* key, const char* value);

// Invalidate a token (called after hosted code returns)
void aether_shared_map_revoke_token(uint64_t token);

// Freeze inputs — after this, hosted code can read inputs but not overwrite them.
void aether_shared_map_freeze_inputs(AetherSharedMap* map);

// Key iteration (for injecting map contents into hosted language hashes)
int aether_shared_map_count(AetherSharedMap* map);
const char* aether_shared_map_key_at(AetherSharedMap* map, int index);
const char* aether_shared_map_value_at(AetherSharedMap* map, int index);

// Token-based iteration
int aether_shared_map_count_by_token(uint64_t token);
const char* aether_shared_map_key_at_by_token(uint64_t token, int index);
const char* aether_shared_map_value_at_by_token(uint64_t token, int index);

// Cross-process shared memory serialization
// Format: key\0value\0key\0value\0\0 (double-null terminated)
// Returns shm name (caller must free). Sets AETHER_MAP_SHM env var.
char* aether_shared_map_to_shm(AetherSharedMap* map);
// Read outputs back from shm into map (hosted process wrote new keys)
void aether_shared_map_read_outputs_from_shm(AetherSharedMap* map, const char* shm_name);
// Clean up shm
void aether_shared_map_unlink_shm(const char* shm_name);

#endif
