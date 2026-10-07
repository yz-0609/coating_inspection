"""MCU register/command CLI. Physical transport requires optional pyserial.
No background heartbeat or automatic RESET is performed.
"""
import argparse
import json
import struct
import time


def crc(data):
    value = 0xFFFF
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xA001 if value & 1 else 0)
    return value


def frame(payload):
    return payload + struct.pack("<H", crc(payload))


def command_frame(epoch, token, seq, opcode, argument=0):
    words = [epoch >> 16, epoch & 0xFFFF, token >> 16, token & 0xFFFF,
             seq >> 16, seq & 0xFFFF, opcode, argument >> 16, argument & 0xFFFF]
    return frame(bytes([1, 16, 1, 16, 0, 9, 18]) + struct.pack(">9H", *words))


def heartbeat_frame(value):
    return frame(struct.pack(">BBHH", 1, 6, 0x0100, value))


SNAPSHOT_REQUEST = frame(bytes([1, 4, 0, 0, 0, 22]))


def validate(reply, function):
    if len(reply) < 5 or crc(reply) or reply[0] != 1:
        raise RuntimeError("Invalid or missing response/CRC")
    if reply[1] == (function | 0x80):
        raise RuntimeError(f"Modbus exception 0x{reply[2]:02X}")
    if reply[1] != function:
        raise RuntimeError("Response function mismatch")
    return reply


def decode_snapshot(reply):
    validate(reply, 4)
    if len(reply) != 49 or reply[2] != 44:
        raise RuntimeError("Expected full 22-register snapshot")
    w = struct.unpack(">22H", reply[3:-2])
    join = lambda i: (w[i] << 16) | w[i + 1]
    return dict(state=w[0], inputs=w[1], outputs=w[2], fault=w[3], health=w[4],
                mcu_heartbeat=w[5], uptime=join(6), token=join(8), ack_seq=join(10),
                ack_status=w[12], event_seq=join(13), event_code=w[15],
                reset_reason=w[16], errors=join(17), version=w[19], ack_epoch=join(20))


def postcondition(operation, snapshot, token):
    if snapshot["version"] != 0x0200:
        return False
    state, outputs = snapshot["state"], snapshot["outputs"]
    if operation in ("sync", "abort"):
        return state == 5 and outputs == 4 and snapshot["token"] == 0
    if operation == "reset":
        return state in (1, 2) and outputs == 0 and snapshot["token"] == 0
    if operation == "ack":
        return snapshot["event_code"] == 0
    return (state == (3 if operation == "start" else 4) and snapshot["token"] == token
            and bool(snapshot["inputs"] & 1)
            and outputs == {"start": 0, "pass": 1, "fail": 2}[operation])


class SerialTransport:
    def __init__(self, port):
        try:
            import serial
        except ImportError as error:
            raise RuntimeError("Install pyserial: python -m pip install -r tools/requirements.txt") from error
        self.serial = serial.Serial(port, 115200, bytesize=serial.EIGHTBITS,
                                    parity=serial.PARITY_EVEN, stopbits=serial.STOPBITS_ONE,
                                    timeout=0.02, write_timeout=0.15)

    def exchange(self, request):
        self.serial.reset_input_buffer()
        self.serial.write(request)
        self.serial.flush()
        reply = bytearray()
        deadline = time.monotonic() + 0.15
        wanted = None
        while time.monotonic() < deadline:
            reply.extend(self.serial.read(1))
            if len(reply) >= 2 and reply[1] & 0x80:
                wanted = 5
            elif len(reply) >= 3 and reply[1] in (3, 4):
                wanted = 5 + reply[2]
            elif len(reply) >= 2 and reply[1] in (6, 16):
                wanted = 8
            if wanted and len(reply) == wanted:
                time.sleep(0.00175)
                return bytes(reply)
        raise TimeoutError("MCU response exceeded 150 ms")

    def close(self):
        self.serial.close()


def integer(text):
    value = int(text, 0)
    if not 0 <= value <= 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("Value must be uint32")
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    sub = parser.add_subparsers(dest="operation", required=True)
    sub.add_parser("snapshot")
    hb = sub.add_parser("heartbeat")
    hb.add_argument("value", type=integer)
    for name in ("sync", "reset", "start", "pass", "fail", "ack", "abort"):
        cmd = sub.add_parser(name)
        cmd.add_argument("--epoch", required=True, type=integer)
        cmd.add_argument("--seq", required=True, type=integer)
        cmd.add_argument("--token", default=0, type=integer)
        if name == "ack":
            cmd.add_argument("--event", required=True, type=integer)
    args = parser.parse_args()
    transport = SerialTransport(args.port)
    try:
        if args.operation == "heartbeat":
            if args.value > 0xFFFF:
                parser.error("Heartbeat must be uint16")
            request = heartbeat_frame(args.value)
            reply = validate(transport.exchange(request), 6)
            if reply != request:
                raise RuntimeError("Heartbeat echo mismatch")
        elif args.operation != "snapshot":
            op = {"sync": 1, "start": 2, "pass": 3, "fail": 3, "ack": 4, "reset": 5, "abort": 6}
            argument = {"pass": 1, "fail": 2}.get(args.operation, getattr(args, "event", 0))
            request = command_frame(args.epoch, args.token, args.seq, op[args.operation], argument)
            reply = validate(transport.exchange(request), 16)
            if len(reply) != 8 or reply[:6] != request[:6]:
                raise RuntimeError("Command write echo mismatch")
            deadline = time.monotonic() + 1.0
            while True:
                snapshot = decode_snapshot(transport.exchange(SNAPSHOT_REQUEST))
                if snapshot["ack_epoch"] == args.epoch and snapshot["ack_seq"] == args.seq:
                    print(json.dumps(snapshot))
                    if snapshot["ack_status"] != 1:
                        raise RuntimeError("MCU rejected the business command")
                    if not postcondition(args.operation, snapshot, args.token):
                        raise RuntimeError("ACK matches, but current state does not confirm execution")
                    return
                if time.monotonic() >= deadline:
                    raise TimeoutError("Business ACK exceeded 1000 ms")
                time.sleep(0.02)
        print(json.dumps(decode_snapshot(transport.exchange(SNAPSHOT_REQUEST))))
    finally:
        transport.close()


if __name__ == "__main__":
    main()
