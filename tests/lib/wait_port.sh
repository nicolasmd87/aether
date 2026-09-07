# wait_port PORT [HOST] [TRIES] — block until something accepts a TCP
# connection on PORT.
#
# Every server test greps its log for READY, which proves the server PRINTED
# it and nothing more: the listen socket can still be a moment behind. The
# fixed `sleep 0.3` that followed was covering for exactly that gap, and it
# is wrong in both directions at once. It waits 0.3s on a server that was
# ready immediately, and it gives up after 0.3s on a loaded machine, which is
# the port-not-yet-listening flake the sweep sees under load.
#
# curl exits 7, and only 7, when the connection itself could not be made. Any
# other exit means something accepted the connection, which is the whole of
# what "the port is up" means. That holds whatever protocol runs on top: a
# TLS or HTTP/2 port answers a plain HTTP probe with a handshake failure, a
# different exit code, and that still proves the listener is there.
#
# Uses curl because every one of these tests already does; no new dependency,
# and it behaves the same on Linux, macOS and MSYS2.
wait_port() {
    _wp_port="$1"
    _wp_host="${2:-127.0.0.1}"
    _wp_tries="${3:-200}"
    while [ "$_wp_tries" -gt 0 ]; do
        curl -s -o /dev/null --max-time 1 "http://$_wp_host:$_wp_port/" 2>/dev/null
        _wp_rc=$?
        if [ "$_wp_rc" -ne 7 ]; then
            return 0
        fi
        _wp_tries=$((_wp_tries - 1))
        sleep 0.05
    done
    echo "  [FAIL] nothing listening on $_wp_host:$_wp_port"
    return 1
}
