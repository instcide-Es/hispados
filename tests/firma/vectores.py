#!/usr/bin/env python3
# Hispa-DOS - tests/firma/vectores.py: casos para prueba_cripto (SHA-256 de
# hashlib; firmas Ed25519 hechas con «cryptography», buenas y estropeadas).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import hashlib, os, random
from cryptography.hazmat.primitives.asymmetric import ed25519
from cryptography.hazmat.primitives import serialization

R = random.Random(931)
def hx(b): return b.hex() if b else '-'
def rb(n): return bytes(R.getrandbits(8) for _ in range(n))

# SHA-256: los bordes de los bloques (55, 56, 63, 64, 119...) y trozos raros
for n in [0, 1, 3, 55, 56, 57, 63, 64, 65, 111, 112, 119, 120, 127, 128, 129, 1000, 4096, 70000]:
    d = rb(n)
    for trozo in ([n or 1, 1, 7, 64, 100] if n < 5000 else [n, 4096, 333]):
        print('sha', hx(d), hashlib.sha256(d).hexdigest(), trozo)
print('sha', hx(b'abc'), 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad', 3)

# RFC 8032, 7.1, prueba 1 (mensaje vacío) y 2 (un octeto)
print('ed d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a '
      'e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b - - 1')
print('ed 3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c '
      '92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00 - 72 1')

pre = b'HISPADOS-VERSIONES 1\n'
for i in range(24):
    k = ed25519.Ed25519PrivateKey.from_private_bytes(rb(32))
    pub = k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    m = rb(R.choice([0, 1, 31, 64, 200, 1500, 4000]))
    f = k.sign(pre + m)
    print('ed', pub.hex(), f.hex(), hx(pre), hx(m), 1)
    if i % 3 == 0:
        print('ed', pub.hex(), f.hex(), '-', hx(pre + m), 1)       # da igual por dónde se parta
    # estropeadas: un bit del mensaje, de R, de S, otra clave, sin prefijo
    if m:
        mm = bytearray(m); mm[R.randrange(len(m))] ^= 1 << R.randrange(8)
        print('ed', pub.hex(), f.hex(), hx(pre), hx(bytes(mm)), 0)
    ff = bytearray(f); ff[R.randrange(32)] ^= 1 << R.randrange(8)
    print('ed', pub.hex(), bytes(ff).hex(), hx(pre), hx(m), 0)
    ff = bytearray(f); ff[32 + R.randrange(31)] ^= 1 << R.randrange(8)
    print('ed', pub.hex(), bytes(ff).hex(), hx(pre), hx(m), 0)
    print('ed', pub.hex(), f.hex(), '-', hx(m), 0)
    otra = ed25519.Ed25519PrivateKey.from_private_bytes(rb(32)).public_key().public_bytes(
        serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    print('ed', otra.hex(), f.hex(), hx(pre), hx(m), 0)
    # S + L (la misma firma «maleable»): no vale
    L = 2**252 + 27742317777372353535851937790883648493
    s = int.from_bytes(f[32:], 'little') + L
    if s < 2**256:
        print('ed', pub.hex(), (f[:32] + s.to_bytes(32, 'little')).hex(), hx(pre), hx(m), 0)
# una clave que no es un punto de la curva
print('ed', '02' + '00' * 31, '00' * 64, '-', '-', 0)
