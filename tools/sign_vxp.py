"""Sign using the user's installed VXPSign backend; never copy private keys."""
import hashlib, json, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIGNER = Path(r'D:\MRE\keystore\signing\re3vxpsigner')

def main():
    source = ROOT / 'build/PocketJava.vxp'
    target = ROOT / 'build/PocketJava-signed.vxp'
    result = subprocess.run([
        sys.executable, str(SIGNER / 'tools/sign_re3_sha1.py'),
        str(source), str(target), str(SIGNER / 'key/cert100-key.pem')
    ], check=True, capture_output=True, text=True, encoding='utf-8')
    print(result.stdout)
    assert 'VERIFY    : OK' in result.stdout
    original, signed = source.read_bytes(), target.read_bytes()
    offset = struct.unpack_from('<I', original, len(original)-12)[0]
    assert original[:offset] == signed[:offset], 'Signing modified executable bytes'
    assert signed[-86:-80] == b'\xb4VDE10'
    assert struct.unpack_from('<I', signed, len(signed)-80)[0] == 100
    digest = hashlib.sha256(signed).hexdigest().upper()
    assert digest in target.with_name(target.name+'.sha256').read_text()
    # The signer writes a CRLF sidecar with a binary-mode marker; rewrite it as the
    # canonical "<hex>  <name>\n" form so sha256sum -c works on Windows and Linux.
    target.with_name(target.name+'.sha256').write_bytes(
        f'{digest.lower()}  {target.name}\n'.encode('ascii'))
    report = {
        'unsigned': str(source), 'signed': str(target),
        'bytes': len(signed), 'sha256': digest,
        'signer': str(SIGNER / 'tools/sign_re3_sha1.py'),
        'signature': 'RSA-SHA1 PKCS#1 v1.5; verified by OpenSSL',
        'cert_id': 100, 'imsi': '*', 'executable_bytes_unchanged': True,
        'hardware_verified': False
    }
    (ROOT/'build/signing-report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')

if __name__ == '__main__':
    main()
