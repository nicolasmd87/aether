/* aether_h2.h — HTTP/2 server-side glue (#260 Tier 2).
 *
 * Wraps libnghttp2 to give Aether's HTTP server an h2 + h2c data path.
 * The wrapper is fully encapsulated: callers feed bytes via
 * aether_h2_session_feed() and pull bytes to write via
 * aether_h2_session_pending(); nghttp2 handles framing, HPACK,
 * stream state, flow control, and SETTINGS negotiation.
 *
 * Trust + lifetime model:
 *   - One nghttp2_session per connection. Created at handshake/upgrade,
 *     destroyed when the connection closes.
 *   - Per-stream HttpRequest / HttpServerResponse pairs are constructed
 *     when nghttp2 reports END_STREAM on the request HEADERS, dispatched
 *     into the existing route table, and torn down when the response
 *     finishes writing.
 *   - The dispatch path runs synchronously on the connection's worker
 *     thread (matching the existing HTTP/1.1 path) — no per-stream
 *     actor children in v1; multiplexing is sequential per connection.
 *     Future work can spawn per-stream actors for concurrent dispatch.
 *
 * When the build doesn't link libnghttp2 the wrapper compiles to no-op
 * stubs; the only call site is server_handle_h2_connection which is
 * gated behind conn->is_h2, set only when ALPN selects "h2" (which
 * itself only happens when http_server_set_h2_raw was called).
 */

#ifndef AETHER_HTTP2_H
#define AETHER_HTTP2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct HttpServer;        /* forward — defined in aether_http_server.h */

typedef struct AetherH2Session AetherH2Session;

/* Create a new server-side h2 session bound to (server, conn_userdata).
 * `conn_userdata` is opaque storage the wire I/O callbacks use to
 * pull bytes from the underlying socket / TLS layer; the wrapper
 * passes it back to those callbacks unmodified.
 *
 * Returns NULL on out-of-memory or when libnghttp2 isn't linked. */
AetherH2Session* aether_h2_session_new(struct HttpServer* server,
                                       void* conn_userdata);

/* Tear down the session. Safe to call with NULL. */
void aether_h2_session_free(AetherH2Session* sess);

/* Submit the connection-preface SETTINGS frame the server is required
 * to send first (RFC 7540 §3.5). Returns 0 on success, -1 on error.
 * Call once after creating the session, before the main feed/drain
 * loop. */
int aether_h2_session_send_initial_settings(AetherH2Session* sess);

/* Feed `len` bytes received from the wire into the session.
 * Returns the number of bytes consumed (always equal to `len` on
 * success; the wrapper buffers internally), -1 on a fatal session
 * error (peer protocol violation, OOM, etc.) — caller must close
 * the connection. */
int aether_h2_session_feed(AetherH2Session* sess,
                           const uint8_t* data, size_t len);

/* Drain pending output bytes nghttp2 wants to send. The supplied
 * callback writes them to the wire (TLS or plain socket); it
 * returns the number of bytes accepted, or a negative value to
 * indicate I/O failure (which the wrapper then propagates as -1).
 *
 * Returns 0 when nghttp2's output queue is empty AND the session is
 * not finished, 1 when the session has cleanly closed (caller may
 * tear down), -1 on I/O failure. */
typedef int (*AetherH2WriteFn)(void* conn_userdata,
                               const uint8_t* buf, size_t len);
int aether_h2_session_drain(AetherH2Session* sess,
                            AetherH2WriteFn write_fn);

/* True when both peers have sent their final frames and no streams
 * remain open. Caller polls after each feed/drain to decide when to
 * close the connection. */
int aether_h2_session_want_close(AetherH2Session* sess);

/* h2c (cleartext) upgrade entry point.
 *
 * Called by the HTTP/1.1 parser when it sees a complete request
 * carrying `Connection: Upgrade`, `Upgrade: h2c`, and
 * `HTTP2-Settings: <base64>` headers (RFC 7540 §3.2). The wrapper:
 *
 *   1. Decodes the HTTP2-Settings payload from base64url (no-pad).
 *   2. Sends `101 Switching Protocols` over the supplied write
 *      callback so the peer transitions immediately.
 *   3. Creates a fresh server-side h2 session pre-loaded with the
 *      decoded peer SETTINGS plus the original HTTP/1.1 request as
 *      stream 1 (per RFC 7540 §3.2.1).
 *
 * Returns the new session on success, NULL on any failure (caller
 * falls back to HTTP/1.1 with a 400 / closes the connection).
 *
 * `request_method` / `request_path` / `request_headers` describe
 * the HTTP/1.1 request that triggered the upgrade — the wrapper
 * synthesises stream 1's request out of them so the route handler
 * sees a normal h2 stream with no "this came from h2c" caveat.
 */
AetherH2Session* aether_h2_session_from_h2c_upgrade(
    struct HttpServer* server,
    void* conn_userdata,
    const char* request_method,
    const char* request_path,
    const char* request_headers,  /* full \r\n-delimited block */
    const char* http2_settings_b64);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* AETHER_HTTP2_H */
