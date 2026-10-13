# nuphy.io against the V2 firmware, without a V2

`server.py` serves the firmware's protocol core (`nph_proto.c`, built by `nph_shim.c` into `libnph.dylib`) over HTTP.
`proxy.py` mirrors https://drive.nuphy.io locally with a fake keyboard injected that forwards every 64-byte report to it, so the real
nuphy.io app talks to our firmware code (`cc -shared -fPIC -I ../../firmware/air75v2/src/core -o libnph.dylib nph_shim.c ../../firmware/air75v2/src/core/nph_proto.c`).

    python3 server.py &        # :8766  firmware core
    python3 proxy.py &         # :8767  nuphy.io + fake keyboard      (open http://127.0.0.1:8767/#/pressKey)
    python3 proxy.py log 8768  # :8768  nuphy.io + a recorder for your REAL keyboard (writes real.log via server.py /rec)

`/poke?w=key|default|appdef|kbfunc` edits the simulated device's data to bisect what nuphy.io checks. Found this way: the corner key is only
selectable when the factory default (command B1) at slot 60 is volume-mute. The site's "upgrade mode" pop-up is an artifact of the fake device.
Air Deck can use the same core with `http://localhost:5173/?fw`.

Other boards / models: `./build.sh air96v2` rebuilds `libnph.dylib` from that board's generated data (default `air75v2`), and `proxy.py <usb product name> <port> <pid>` sets the USB product ID the fake keyboard presents - nuphy.io picks its model from the PID (`python3 proxy.py "Air96 V2 (open firmware)" 8767 0x102D` is an Air100 V3).
