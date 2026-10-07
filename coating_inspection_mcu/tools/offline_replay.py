"""Replay the production pure C modules in virtual time; no GPIO/DMA evidence."""
import argparse
import json
import pathlib
import subprocess
import time
from mcu_cli import command_frame, heartbeat_frame, SNAPSHOT_REQUEST, decode_snapshot, validate, postcondition


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", required=True)
    parser.add_argument("--output")
    args = parser.parse_args()
    started = time.monotonic()
    child = subprocess.Popen([args.emulator], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             text=True, encoding="utf-8", bufsize=1)
    steps = []

    def line(command):
        child.stdin.write(command + "\n")
        child.stdin.flush()
        reply = child.stdout.readline().strip()
        if reply.startswith("ERROR") or child.poll() is not None:
            raise RuntimeError(reply or "Emulator stopped")
        steps.append(dict(command=command, reply=reply))
        return reply

    def exchange(request):
        return bytes.fromhex(line("F " + request.hex()))

    def snapshot():
        return decode_snapshot(exchange(SNAPSHOT_REQUEST))

    def command(epoch, seq, opcode, token=0, arg=0, ack=1):
        validate(exchange(command_frame(epoch, token, seq, opcode, arg)), 16)
        s = snapshot()
        assert (s["ack_epoch"], s["ack_seq"], s["ack_status"]) == (epoch, seq, ack), s
        return s

    def heartbeat(value):
        request = heartbeat_frame(value)
        assert validate(exchange(request), 6) == request

    try:
        assert json.loads(line("S"))["state"] == 5
        line("I 1"); line("T 40")
        assert command(7, 1, 1)["outputs"] == 4
        heartbeat(1)
        assert command(7, 2, 5)["state"] == 2
        assert command(7, 3, 2, 99)["state"] == 3
        line("I 3"); line("T 80")
        s = snapshot(); assert s["event_code"] == 1
        assert command(7, 4, 4, arg=s["event_seq"])["event_code"] == 0
        line("I 1"); line("T 120"); line("T 800"); heartbeat(2)
        line("T 1300"); line("I 3"); line("T 1340")
        s = snapshot(); assert s["event_code"] == 1 and s["event_seq"] == 2
        command(7, 5, 4, arg=s["event_seq"])
        s=command(7, 6, 3, 99, 1)
        assert s["outputs"] == 1 and postcondition("pass", s, 99)
        line("H 3")
        s=command(7, 6, 3, 99, 1)
        assert s["outputs"] == 4 and not postcondition("pass", s, 99)
        line("H 7"); heartbeat(3)
        command(7, 7, 5)
        command(7, 8, 2, 100)
        line("I 0"); line("T 1380")
        assert snapshot()["fault"] == 2
        assert command(7, 9, 6, 100)["outputs"] == 4
        command(8, 1, 1)
        command(8, 2, 5, ack=5)  # new SYNC requires a new heartbeat change
        heartbeat(3); command(8, 2, 5, ack=5)
        heartbeat(4); assert command(8, 3, 5)["state"] == 1
        line("I 1"); line("T 1420")
        command(8, 4, 2, 101)
        assert command(8, 5, 3, 101, 2)["outputs"] == 2
        line("T 2880"); s = snapshot(); assert s["outputs"] == 4 and s["fault"] == 1
        heartbeat(4); assert snapshot()["outputs"] == 4
        heartbeat(5); assert snapshot()["outputs"] == 4
        assert command(8, 5, 3, 101, 2)["outputs"] == 4
        report = dict(kind="offline virtual-time replay", hardware_verified=False,
                      actual_elapsed_seconds=round(time.monotonic()-started, 3),
                      virtual_duration_ms=2880, passed=True, steps=steps)
        if args.output:
            output = pathlib.Path(args.output); output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"Offline replay passed: {len(steps)} transactions/steps; virtual 2880 ms; hardware unverified")
    finally:
        child.stdin.close()
        try:
            child.wait(timeout=2)
        except subprocess.TimeoutExpired:
            child.terminate(); child.wait(timeout=2)


if __name__ == "__main__":
    main()
