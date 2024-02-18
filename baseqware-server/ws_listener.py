import asyncio
from websockets.sync.client import connect
from time import sleep


def hello():
    with connect("ws://localhost:8765") as websocket:
        websocket.send("Hello world!")
        # sleep(1)
        # websocket.send(">stop")
        for message in websocket:
            print(message)


hello()
