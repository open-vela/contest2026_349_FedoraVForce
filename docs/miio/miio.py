#!/usr/bin/env python3
"""Minimal miio LAN client (reference for the board C port).

Protocol: 32-byte header + AES-128-CBC(JSON).
  key = MD5(token), iv = MD5(key + token), PKCS7 padding
  header: magic 0x2131 | length | 0x00000000 | device id | stamp | MD5(header[:16] + token + payload)
usage: miio.py <ip> <method> [json-params]
"""
import hashlib, json, os, socket, struct, sys, time
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import padding

def md5(b):
    return hashlib.md5(b).digest()

def main():
    ip, method = sys.argv[1], sys.argv[2]
    params = json.loads(sys.argv[3]) if len(sys.argv) > 3 else []
    token = bytes.fromhex(open(os.path.expanduser('~/.miio-token')).read().strip())
    key = md5(token)
    iv = md5(key + token)

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(3)
    s.sendto(bytes.fromhex('21310020' + 'ff' * 28), (ip, 54321))
    d, _ = s.recvfrom(1024)
    _, _, _, did, stamp = struct.unpack('>HHIII', d[:16])
    t0 = time.monotonic()

    msg = json.dumps({'id': int(time.time()) % 100000, 'method': method,
                      'params': params}, separators=(',', ':')).encode()
    p = padding.PKCS7(128).padder()
    enc = Cipher(algorithms.AES(key), modes.CBC(iv)).encryptor()
    payload = enc.update(p.update(msg) + p.finalize()) + enc.finalize()

    head = struct.pack('>HHIII', 0x2131, 32 + len(payload), 0, did,
                       stamp + int(time.monotonic() - t0) + 1)
    pkt = head + md5(head + token + payload) + payload
    s.sendto(pkt, (ip, 54321))
    d, _ = s.recvfrom(4096)

    dec = Cipher(algorithms.AES(key), modes.CBC(iv)).decryptor()
    u = padding.PKCS7(128).unpadder()
    body = u.update(dec.update(d[32:]) + dec.finalize()) + u.finalize()
    print(body.rstrip(b'\x00').decode())

main()
