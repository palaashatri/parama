#!/usr/bin/env python3
"""One cold UEFI boot of a PARAMA ISO under QEMU/OVMF, with evidence and checks.

Called by run_qemu_qa.sh once per run. Exit 0 only if every check passes:
  * COM1 shows the M1 markers in order and no [BRAHMA][FATAL]
  * the kernel reports its framebuffer mode, matching --expect-resolution if given
    (Limine silently falls back when a mode is unavailable, so this fails closed)
  * a screendump was captured (PPM for checking, PNG kept as evidence)
  * the screendump equals the host reference render pixel-for-pixel
This is QEMU/OVMF evidence only; it does not replace the VirtualBox gate.
"""
import argparse
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time

MARKERS = [
    b'[BRAHMA] PARAMA OS starting',
    b'[BHUMI] Boot frame smoke test passed',
    b'[BRAHMA] Boot diagnostic framebuffer drawn',
    b'[BRAHMA] BOOT_OK',
]
FB_RE = re.compile(rb'\[BRAHMA\] Framebuffer: (\d+)x(\d+)x(\d+) pitch (\d+)')


def fail(msg):
    print(f'[QEMU][FAIL] {msg}', file=sys.stderr)
    return 1


def read_ppm(path):
    data = open(path, 'rb').read()
    tokens, pos = [], 0
    while len(tokens) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b'#':
            pos = data.index(b'\n', pos) + 1
            continue
        end = pos
        while not data[end:end + 1].isspace():
            end += 1
        tokens.append(data[pos:end])
        pos = end
    if tokens[0] != b'P6' or tokens[3] != b'255':
        raise ValueError(f'{path}: unsupported PPM header {tokens}')
    w, h = int(tokens[1]), int(tokens[2])
    pixels = data[pos + 1:pos + 1 + w * h * 3]
    if len(pixels) != w * h * 3:
        raise ValueError(f'{path}: truncated PPM')
    return w, h, pixels


def monitor(sock_path, commands):
    """Drive the HMP monitor. Input sent before the first prompt is dropped, so
    wait for '(qemu) ' before every command; screendump is synchronous, so the
    next prompt means the file has been written."""
    s = socket.socket(socket.AF_UNIX)
    s.connect(sock_path)
    s.settimeout(10)

    def wait_prompt():
        buf = b''
        while not buf.endswith(b'(qemu) '):
            chunk = s.recv(4096)
            if not chunk:
                raise ConnectionError('monitor closed')
            buf += chunk
        return buf

    try:
        wait_prompt()
        for cmd in commands:
            s.sendall(cmd.encode() + b'\n')
            if cmd != 'quit':
                reply = re.sub(rb'\x1b\[[0-9;]*[A-Za-z]', b'', wait_prompt())
                if b'Try "help' in reply or b'rror' in reply or b'invalid' in reply:
                    raise RuntimeError(f'monitor rejected {cmd!r}: '
                                       f'{reply.decode(errors="replace").strip()[-200:]}')
        # Closing immediately can drop 'quit'; QEMU closes the socket on exit.
        try:
            while s.recv(4096):
                pass
        except (socket.timeout, ConnectionError):
            pass
    finally:
        s.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--iso', required=True)
    ap.add_argument('--firmware', required=True)
    ap.add_argument('--log', required=True)
    ap.add_argument('--png', required=True)
    ap.add_argument('--renderer', required=True, help='host render_splash binary')
    ap.add_argument('--expect-resolution', default='')
    ap.add_argument('--timeout', type=float, default=35)
    a = ap.parse_args()

    for path in (a.log, a.png):
        if os.path.exists(path):
            os.remove(path)
    with tempfile.TemporaryDirectory() as tmp:
        sock = os.path.join(tmp, 'mon.sock')
        ppm = os.path.join(tmp, 'screen.ppm')
        ref = os.path.join(tmp, 'reference.ppm')
        png_tmp = os.path.join(tmp, 'screen.png')
        qemu = subprocess.Popen([
            'qemu-system-x86_64', '-machine', 'q35', '-m', '512M', '-smp', '1',
            '-nodefaults', '-device', 'VGA', '-display', 'none',
            '-serial', f'file:{a.log}',
            '-drive', f'if=pflash,format=raw,readonly=on,file={a.firmware}',
            '-cdrom', a.iso, '-boot', 'order=d', '-no-reboot', '-no-shutdown',
            '-monitor', f'unix:{sock},server,nowait',
        ], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        log = b''
        deadline = time.monotonic() + a.timeout
        try:
            while time.monotonic() < deadline:
                if qemu.poll() is not None:
                    return fail(f'emulator exited early ({qemu.returncode}): '
                                f'{qemu.stderr.read().decode(errors="replace").strip()}')
                if os.path.exists(a.log):
                    log = open(a.log, 'rb').read()
                    if MARKERS[-1] in log or b'[BRAHMA][FATAL]' in log:
                        break
                time.sleep(0.25)
            # The kernel halts after BOOT_OK, so the framebuffer is final here.
            # HMP syntax: screendump FILENAME [-f FORMAT] (PPM is the default).
            # Dump into the temp dir: HMP cannot quote paths containing spaces.
            cmds = [f'screendump {ppm}', f'screendump {png_tmp} -f png'] if MARKERS[-1] in log else []
            try:
                monitor(sock, cmds + ['quit'])
            except (RuntimeError, OSError) as exc:
                return fail(str(exc))
            try:
                qemu.wait(timeout=10)
            except subprocess.TimeoutExpired:
                print('[QEMU][WARN] emulator ignored quit; killing it', file=sys.stderr)
        finally:
            if qemu.poll() is None:
                qemu.kill()
                qemu.wait()
        log = open(a.log, 'rb').read() if os.path.exists(a.log) else b''
        if os.path.exists(png_tmp):
            shutil.move(png_tmp, a.png)

        if b'[BRAHMA][FATAL]' in log:
            return fail('kernel reported [BRAHMA][FATAL]')
        pos = 0
        for marker in MARKERS:
            found = log.find(marker, pos)
            if found < 0:
                return fail(f'serial marker missing or out of order: {marker.decode()}')
            pos = found + len(marker)
        m = FB_RE.search(log)
        if not m:
            return fail('kernel did not report its framebuffer mode')
        fb_w, fb_h, bpp, pitch = (int(g) for g in m.groups())
        if a.expect_resolution and f'{fb_w}x{fb_h}' != a.expect_resolution:
            return fail(f'requested {a.expect_resolution} but firmware/Limine granted '
                        f'{fb_w}x{fb_h} (mode unavailable on this platform)')
        if not os.path.exists(ppm) or not os.path.exists(a.png) or os.path.getsize(a.png) == 0:
            return fail('screendump was not captured')
        w, h, shot = read_ppm(ppm)
        if (w, h) != (fb_w, fb_h):
            return fail(f'screendump is {w}x{h} but kernel framebuffer is {fb_w}x{fb_h}')
        subprocess.run([a.renderer, str(w), str(h), ref], check=True)
        _, _, expected = read_ppm(ref)
        if shot != expected:
            diff = sum(1 for i in range(0, len(shot), 3) if shot[i:i + 3] != expected[i:i + 3])
            return fail(f'screen differs from reference render in {diff} of {w * h} pixels')
        print(f'[QEMU][PASS] {fb_w}x{fb_h}x{bpp} pitch {pitch}: markers ordered, '
              f'screen matches reference pixel-for-pixel')
        return 0


if __name__ == '__main__':
    sys.exit(main())
