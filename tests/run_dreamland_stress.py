"""Drive a fresh, isolated headless runtime with the Dreamland stress fixture.

Launch with PSX_V82_TEST_DREAMLAND_PROJECTILES=1 and both prepared mods enabled.
This exercises real imported callbacks, not mocked engine calls. Never use the
owner's port. ROMs, prepared data and the runtime working directory stay external.
"""
import argparse
import json
import socket
import time


def run(port):
    if port == 4535:
        raise ValueError("Port 4535 belongs to the owner; use an isolated headless instance")

    def command(**request):
        with socket.create_connection(("127.0.0.1", port), timeout=10) as conn:
            conn.sendall(json.dumps(request).encode() + b"\n")
            result = json.loads(conn.makefile("rb").readline())
        if not result.get("ok"):
            raise RuntimeError(result)
        return result

    def counters():
        result = command(cmd="mod_counters")
        return result, {v["name"]: v for v in result["counters"]}

    def frames(count):
        first = command(cmd="mod_counters")["frame"]
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            if command(cmd="mod_counters")["frame"] - first >= count:
                return
            time.sleep(.1)
        raise TimeoutError("Runtime stopped advancing during stress setup")

    _, values = counters()
    if not values.get("v82.stress.enabled"):
        raise RuntimeError("Launch the debug runtime with PSX_V82_TEST_DREAMLAND_PROJECTILES=1")
    if not values.get("v82.match.selected"):
        frames(100)
        for _ in range(8):
            command(cmd="press", buttons=65527, frames=12)
            frames(75)
        for _ in range(10):
            # Select the imported arena in the isolated menu fixture.
            command(cmd="write_ram", addr="0x8006b418", val="0x12")
            command(cmd="press", buttons=49151, frames=12)
            frames(260)
            _, values = counters()
            if values.get("v82.match.selected"):
                break
        else:
            raise RuntimeError("Fresh-boot menu route did not start a match")
        frames(1100)
        command(cmd="press", buttons=49151, frames=12)

    deadline = time.monotonic() + 90
    while time.monotonic() < deadline:
        result, values = counters()
        if values.get("v82.stress.sweep_complete"):
            break
        time.sleep(.15)
    else:
        raise TimeoutError("Mushroom event sweep did not complete")
    for name in ("good_mushroom", "bad_mushroom", "valid_projectile"):
        if not values.get("v82.stress." + name, {}).get("count"):
            raise AssertionError("Missing native coverage: " + name)

    first_water = values["v82.dreamland.water_frame"]["count"]
    deadline = time.monotonic() + 90
    while time.monotonic() < deadline:
        result, values = counters()
        if values["v82.dreamland.water_frame"]["count"] - first_water >= 1000:
            print(json.dumps({"passed": True, "frame": result["frame"],
                              "counters": {k: v["count"] for k, v in values.items()
                                           if k.startswith("v82.stress.") or
                                           k in ("v82.dreamland.nearest", "v82.dreamland.water_frame")}}))
            return
        time.sleep(.15)
    raise TimeoutError("Gameplay stopped after the projectile sweep")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4537)
    run(parser.parse_args().port)
