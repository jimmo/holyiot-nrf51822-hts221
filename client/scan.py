import asyncio
import struct

from bleak import discover

async def run():
    while True:
        devices = await discover()
        for d in devices:
            manuf = d.metadata.get('manufacturer_data', {}).get(65535, [])
            if d.name.startswith('temp') and len(manuf) == 4:
                h, t = struct.unpack('<hh', bytes(manuf))
                print(d.name, h / 2, t / 8)

loop = asyncio.get_event_loop()
loop.run_until_complete(run())
