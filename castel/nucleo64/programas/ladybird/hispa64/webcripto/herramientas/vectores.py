# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hispa64/webcripto/herramientas/vectores.py - Hispa-64, etapa 10 i: hace mcripto-vectores.h
# (en el anfitrión, con Python cryptography: otra implementación, para comparar) y comprueba
# los vectores de las RFC que van a mano en mcripto.c. Uso: python3 -I vectores.py mcripto-vectores.h
import hashlib, hmac, sys
from cryptography.hazmat.primitives.asymmetric import rsa, ec, padding, ed25519, x25519
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM, AESOCB3, ChaCha20Poly1305
from cryptography.hazmat.primitives.keywrap import aes_key_wrap
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature, encode_dss_signature
H=bytes.fromhex
def c(nombre, b):
    s = ', '.join('0x%02x' % x for x in b)
    return 'static const uint8_t %s[] = { %s };\n' % (nombre, s)
out = ['/* Hecho por gen.py con Python cryptography (otra implementación, en el anfitrión): no cambiar a mano. */\n']
# ---- RSA 2048 fija ----
k = rsa.generate_private_key(public_exponent=65537, key_size=2048)
p8 = k.private_bytes(serialization.Encoding.DER, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
spki = k.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
msg = 'Hispa-64: ñandú'.encode()
s1 = k.sign(msg, padding.PKCS1v15(), hashes.SHA256())
s2 = k.sign(msg, padding.PSS(mgf=padding.MGF1(hashes.SHA384()), salt_length=20), hashes.SHA384())
o1 = k.public_key().encrypt(msg, padding.OAEP(mgf=padding.MGF1(hashes.SHA256()), algorithm=hashes.SHA256(), label=b'etiqueta'))
o2 = k.public_key().encrypt(msg, padding.OAEP(mgf=padding.MGF1(hashes.SHA1()), algorithm=hashes.SHA1(), label=None))
n = k.private_numbers()
def be(v): return v.to_bytes((v.bit_length()+7)//8, 'big')
out += [c('RSA_P8', p8), c('RSA_SPKI', spki), c('RSA_MSG', msg), c('RSA_PKCS1_SHA256', s1), c('RSA_PSS_SHA384_20', s2),
        c('RSA_OAEP_SHA256_ETIQ', o1), c('RSA_OAEP_SHA1', o2), c('RSA_N', be(n.public_numbers.n)), c('RSA_E', be(n.public_numbers.e)), c('RSA_D', be(n.d))]
# ---- P-521: una firma de Python para comprobar ----
e5 = ec.generate_private_key(ec.SECP521R1())
pub5 = e5.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
r, s = decode_dss_signature(e5.sign(msg, ec.ECDSA(hashes.SHA512())))
out += [c('P521_PUB', pub5), c('P521_FIRMA', r.to_bytes(66,'big') + s.to_bytes(66,'big'))]
# ---- P-384: PKCS #8 con la pública, y su SPKI ----
e3 = ec.generate_private_key(ec.SECP384R1())
out += [c('P384_P8', e3.private_bytes(serialization.Encoding.DER, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())),
        c('P384_SPKI', e3.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo))]
# ---- P-256: un punto comprimido y el mismo sin comprimir ----
e2 = ec.generate_private_key(ec.SECP256R1())
out += [c('P256_COMPR', e2.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.CompressedPoint)),
        c('P256_SINCOMPR', e2.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint))]
open(sys.argv[1], 'w').write(''.join(out))

# ---- comprobar los de las RFC escritos a mano en mcripto.c ----
assert hashlib.sha1(b'abc').hexdigest() == 'a9993e364706816aba3e25717850c26c9cd0d89d'
assert hashlib.sha256(b'abc').hexdigest() == 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'
assert hashlib.sha384(b'abc').hexdigest() == 'cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7'
assert hashlib.sha512(b'abc').hexdigest() == 'ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f'
assert hashlib.sha256(b'abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq').hexdigest() == '248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1'
assert hashlib.sha3_256(b'abc').hexdigest() == '3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532'
assert hashlib.sha3_384(b'abc').hexdigest() == 'ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25'
assert hashlib.sha3_512(b'abc').hexdigest() == 'b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0'
assert hashlib.shake_128(b'').hexdigest(32) == '7f9c2ba4e88f827d616045507605853ed73b8093f6efbc88eb1a6eacfa66ef26'
assert hashlib.shake_256(b'').hexdigest(64) == '46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762fd75dc4ddd8c0f200cb05019d67b592f6fc821c49479ab48640292eacb3b7c4be'
assert hmac.new(b'\x0b'*20, b'Hi There', 'sha256').hexdigest() == 'b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7'
assert hmac.new(b'\x0b'*20, b'Hi There', 'sha512').hexdigest() == '87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854'
assert hmac.new(b'Jefe', b'what do ya want for nothing?', 'sha384').hexdigest() == 'af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649'
assert hmac.new(b'\x0b'*20, b'Hi There', 'sha1').hexdigest() == 'b617318655057264e28bc0b6fb378c8ef146be00'
assert hmac.new(b'Jefe', b'what do ya want for nothing?', 'sha3_256').hexdigest() == hmac.new(b'Jefe', b'what do ya want for nothing?', 'sha3_256').hexdigest()
print('HMAC-SHA3-256 Jefe:', hmac.new(b'Jefe', b'what do ya want for nothing?', 'sha3_256').hexdigest())
assert hashlib.pbkdf2_hmac('sha1', b'password', b'salt', 1).hex() == '0c60c80f961f0e71f3a9b524af6012062fe037a6'
assert hashlib.pbkdf2_hmac('sha1', b'password', b'salt', 4096).hex() == '4b007901b765489abead49d926f721d065a429c1'
assert hashlib.pbkdf2_hmac('sha256', b'passwd', b'salt', 1, 64).hex() == '55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783'
ikm=b'\x0b'*22; salt=H('000102030405060708090a0b0c'); info=H('f0f1f2f3f4f5f6f7f8f9')
assert HKDF(hashes.SHA256(), 42, salt, info).derive(ikm).hex() == '3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865'
K=H('2b7e151628aed2a6abf7158809cf4f3c'); P=H('6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710')
e=Cipher(algorithms.AES(K), modes.CBC(H('000102030405060708090a0b0c0d0e0f'))).encryptor(); assert (e.update(P)+e.finalize()).hex()=='7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b273bed6b8e3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a7'
e=Cipher(algorithms.AES(K), modes.CTR(H('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff'))).encryptor(); assert (e.update(P)+e.finalize()).hex()=='874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff5ae4df3edbd5d35e5b4f09020db03eab1e031dda2fbe03d1792170a0f3009cee'
e=Cipher(algorithms.AES(K), modes.CTR(H('ffffffffffffffffffffffffffffffff'))).encryptor(); print('CTR vuelta:', (e.update(P[:40])).hex())
e=Cipher(algorithms.AES(K), modes.CTR(H('00000000000000000000000ffffffffe'))).encryptor(); print('CTR acarreo:', (e.update(P[:64])).hex())
gk=H('feffe9928665731c6d6a8f9467308308'); giv=H('cafebabefacedbaddecaf888'); gp=H('d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39'); ga=H('feedfacedeadbeeffeedfacedeadbeefabaddad2')
assert AESGCM(gk).encrypt(giv,gp,ga).hex()=='42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091'+'5bc94fbc3221a5db94fae95ae7121a47'
print('GCM IV16:', AESGCM(gk).encrypt(H('000102030405060708090a0b0c0d0e0f'), gp, ga).hex())
assert aes_key_wrap(H('000102030405060708090a0b0c0d0e0f'), H('00112233445566778899aabbccddeeff')).hex()=='1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5'
ok=H('000102030405060708090a0b0c0d0e0f'); on=H('bbaa99887766554433221100')
assert AESOCB3(ok).encrypt(on,b'',b'').hex()=='785407bfffc8ad9edcc5520ac9111ee6'
assert AESOCB3(ok).encrypt(H('bbaa99887766554433221101'),H('0001020304050607'),H('0001020304050607')).hex()=='6820b3657b6f615a5725bda0d3b4eb3a257c9af1f8f03009'
po=bytes(range(40)); print('OCB 40:', AESOCB3(ok).encrypt(H('bbaa9988776655443322110d'),po,po).hex())
ck=H('808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f'); cn=H('070000004041424344454647'); ca=H('50515253c0c1c2c3c4c5c6c7')
cp=b"Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it."
r=ChaCha20Poly1305(ck).encrypt(cn,cp,ca); assert r[-16:].hex()=='1ae10b594f09e26a7e902ecbd0600691'; print('ChaCha:', r.hex())
sk=ed25519.Ed25519PrivateKey.from_private_bytes(H('9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60'))
assert sk.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw).hex()=='d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a'
assert sk.sign(b'').hex()=='e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b'
sk2=ed25519.Ed25519PrivateKey.from_private_bytes(H('4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb'))
assert sk2.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw).hex()=='3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c'
assert sk2.sign(H('72')).hex()=='92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00'
a=x25519.X25519PrivateKey.from_private_bytes(H('77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a'))
b=x25519.X25519PrivateKey.from_private_bytes(H('5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb'))
assert a.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw).hex()=='8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a'
assert b.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw).hex()=='de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f'
assert a.exchange(b.public_key()).hex()=='4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742'
# RFC 6979 A.2.5: P-256, SHA-256, «sample»
d=int('C9AFA9D845BA75166B5C215767B1D6934E50C3DB36E89B127B8A622B120F6721',16)
pk=ec.derive_private_key(d, ec.SECP256R1()).public_key()
assert pk.public_numbers().x==int('60FED4BA255A9D31C961EB74C6356D68C049B8923B61FA6CE669622E60F29FB6',16)
assert pk.public_numbers().y==int('7903FE1008B8BC99A41AE9E95628BC64F2F1B20C2D7E9F5177A3C294D4462299',16)
pk.verify(encode_dss_signature(int('EFD48B2AACB6A8FD1140DD9CD45E81D69D2C877B56AAF991C34D0EA84EAF3716',16), int('F7CB1C942D657C41D436C7A1B6E29F65F3E900DBB9AFF4064DC4AB2F843ACDA8',16)), b'sample', ec.ECDSA(hashes.SHA256()))
d=int('6B9D3DAD2E1B8C1C05B19875B6659F4DE23C3B667BF297BA9AA47740787137D896D5724E4C70A825F872C9EA60D2EDF5',16)
pk=ec.derive_private_key(d, ec.SECP384R1()).public_key()
pk.verify(encode_dss_signature(int('94EDBB92A5ECB8AAD4736E56C691916B3F88140666CE9FA73D64C4EA95AD133C81A648152E44ACF96E36DD1E80FABE46',16), int('99EF4AEB15F178CEA1FE40DB2603138F130E740A19624526203B6351D0A3A94FA329C145786E679E7B82C71A38628AC8',16)), b'sample', ec.ECDSA(hashes.SHA384()))
# NIST CAVP KAS ECC CDH P-256, COUNT = 0
q=ec.EllipticCurvePublicNumbers(int('700c48f77f56584c5cc632ca65640db91b6bacce3a4df6b42ce7cc838833d287',16), int('db71e509e3fd9b060ddb20ba5c51dcc5948d46fbf640dfe0441782cab85fa4ac',16), ec.SECP256R1()).public_key()
di=ec.derive_private_key(int('7d7dc5f71eb29ddaf80d6214632eeae03d9058af1fb6d22ed80badb62bc1a534',16), ec.SECP256R1())
assert di.public_key().public_numbers().x==int('ead218590119e8876b29146ff89ca61770c4edbbf97d38ce385ed281d8a6b230',16)
assert di.exchange(ec.ECDH(), q).hex()=='46fc62106420ff012e54a434fbdd2d25ccc5852060561e68040dd7778997bd7b'
print('todo bien')
