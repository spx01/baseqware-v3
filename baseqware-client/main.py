# websocket test

import asyncio
from websockets.server import serve
import json


async def handle_client(websocket):
    print("Client connected")

    # Receive and decode the first message
    message = await websocket.recv()
    window_dims = json.loads(message)
    print(f"Received: {window_dims}")

    data = [
        {
            "type": "box",
            "x": window_dims["width"] / 2 - 50,
            "y": window_dims["height"] / 2 - 50,
            "w": 100,
            "h": 100,
        }
    ]
    while True:
        print(json.dumps(data))
        await websocket.send(json.dumps(data))
        await asyncio.sleep(0.2)


async def main():
    async with serve(handle_client, "localhost", 8765):
        await asyncio.Future()  # run forever


asyncio.run(main())
