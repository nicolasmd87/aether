# An INDEPENDENT RFC 6455 server (python3-websockets), used as the peer our
# client dials. The point is third-party validation: the library rejects an
# unmasked client frame itself, per RFC 6455 s5.1, so if our mask were wrong
# or absent this fails rather than quietly interoperating.
import asyncio, sys
import websockets

async def echo(ws, path=None):
    try:
        async for msg in ws:
            await ws.send("echo:" + msg)
    except websockets.ConnectionClosed:
        pass

async def main():
    # Bind port 0 and report the kernel's choice, so parallel runs cannot
    # collide on a number nobody chose.
    async with websockets.serve(echo, "127.0.0.1", 0) as server:
        port = server.sockets[0].getsockname()[1]
        print("READY", port, flush=True)
        await asyncio.Future()

asyncio.run(main())
