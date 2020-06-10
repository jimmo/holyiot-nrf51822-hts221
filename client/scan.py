import asyncio
import struct

from bleak import discover

NAMES = {
    'th64d28b01': 'outside',
    'th4a011940': 'kitchen',
    'thc37fd516': 'taylor bedroom',
    'th4951ba35': 'taylor bathroom',
}

async def run():
    while True:
        print()
        devices = await discover()
        seen = set()
        for d in devices:
            #print(d)
            manuf = d.metadata.get('manufacturer_data', {}).get(65535, [])
            if d.name not in seen and d.name.startswith('th') and len(d.name) == 10 and len(manuf) == 4:
                h, t = struct.unpack('<hh', bytes(manuf))
                print(d.rssi, NAMES.get(d.name, d.name), h / 2, t / 8)
                seen.add(d.name)

loop = asyncio.get_event_loop()
loop.run_until_complete(run())
