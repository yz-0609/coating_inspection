"""Real serial monitoring; records actual elapsed time and incomplete runs honestly.
Use with the normal host service stopped: an RTU bus must have one active master.
This monitors registers; GPIO waves and actual inspection sessions are separate evidence.
"""
import argparse
import collections
import datetime
import json
import pathlib
import time
from mcu_cli import SerialTransport, heartbeat_frame, SNAPSHOT_REQUEST, decode_snapshot, validate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--hours", type=float, default=24)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    if args.hours <= 0:
        parser.error("--hours must be positive")
    output = pathlib.Path(args.output); output.parent.mkdir(parents=True, exist_ok=True)
    stream = None; started = time.monotonic(); heartbeat = 0
    report = dict(kind="real serial register monitor", port=args.port, requested_hours=args.hours,
                  started_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  completed=False, gpio_verified=False, instrument_verified=False,
                  snapshots=0, heartbeat_writes=0, serial_errors=0, resets_observed=0,
                  mcu_normal_checks=0, inspection_sessions=0,
                  inspection_sessions_note="This tool does not execute inspection sessions", states={})
    previous = None; states = collections.Counter()
    try:
        stream = SerialTransport(args.port)
        with output.with_suffix(".jsonl").open("w", encoding="utf-8") as samples:
            while time.monotonic()-started < args.hours*3600:
                heartbeat = (heartbeat+1) & 0xFFFF
                request = heartbeat_frame(heartbeat)
                if validate(stream.exchange(request), 6) != request:
                    raise RuntimeError("Heartbeat echo mismatch")
                report["heartbeat_writes"] += 1
                snapshot = decode_snapshot(stream.exchange(SNAPSHOT_REQUEST))
                states[str(snapshot["state"])] += 1
                report["snapshots"] += 1
                if previous:
                    advance = (snapshot["uptime"]-previous["uptime"]) & 0xFFFFFFFF
                    if advance > 10000:
                        report["resets_observed"] += 1
                    else:
                        report["mcu_normal_checks"] += (snapshot["mcu_heartbeat"]-previous["mcu_heartbeat"]) & 0xFFFF
                previous = snapshot
                samples.write(json.dumps(dict(elapsed=time.monotonic()-started, snapshot=snapshot))+"\n")
                samples.flush()
                report["last_snapshot"] = snapshot
                report["states"] = dict(states)
                report["actual_elapsed_seconds"] = round(time.monotonic()-started, 3)
                output.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
                time.sleep(0.2)
        report["completed"] = True
    except KeyboardInterrupt:
        report["stopped_reason"] = "Operator interrupted before requested duration"
    except Exception as error:
        report["serial_errors"] += 1
        report["stopped_reason"] = str(error)
    finally:
        if stream: stream.close()
        report["actual_elapsed_seconds"] = round(time.monotonic()-started, 3)
        report["states"] = dict(states)
        output.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    if not report["completed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
