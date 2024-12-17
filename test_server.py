#!/usr/bin/env python3
import asyncio
import websockets
import json
import datetime

async def handle_client(websocket):
    print(f"New client connected from {websocket.remote_address}")
    try:
        while True:
            # Create sample market data
            market_data = {
                "type": "market_data",
                "symbol": "AAPL",
                "price": 150.0,
                "quantity": 100,
                "timestamp": datetime.datetime.now().isoformat()
            }
            
            # Send market data
            await websocket.send(json.dumps(market_data))
            print(f"Sent: {market_data}")
            
            # Wait for messages from client
            try:
                message = await websocket.recv()
                print(f"Received: {message}")
            except websockets.exceptions.ConnectionClosed:
                break
            
            # Wait a bit before sending next update
            await asyncio.sleep(1)
            
    except websockets.exceptions.ConnectionClosed:
        print(f"Client {websocket.remote_address} disconnected")

async def main():
    print("Starting WebSocket server on ws://localhost:8080")
    async with websockets.serve(handle_client, "localhost", 8080):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    # Install requirements if needed:
    # pip install websockets
    asyncio.run(main())
