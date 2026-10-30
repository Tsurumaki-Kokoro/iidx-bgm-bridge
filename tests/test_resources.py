"""Resource-tool tests using generated tones, with no copyrighted audio fixtures."""
import configparser
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import import_resources as importer

def template(frames):
    coefficients=[(256,0),(512,-256),(0,0),(192,64),(240,0),(460,-208),(392,-232)]
    fmt=struct.pack('<HHIIHHHHH',2,2,44100,44100*256//244,256,4,32,244,7)+b''.join(struct.pack('<hh',*p) for p in coefficients)
    block=bytearray(256);struct.pack_into('<hh',block,2,16,16)
    wav=b'WAVEfmt '+struct.pack('<I',len(fmt))+fmt+b'fact'+struct.pack('<II',4,frames)+b'data'+struct.pack('<I',256)+block
    data=bytearray(32)+b'RIFF'+struct.pack('<I',len(wav))+wav
    data[:4]=b'SD9\0';struct.pack_into('<I',data,8,len(data)-32)
    struct.pack_into('<I',data,24,frames*4)
    return data

class Resources(unittest.TestCase):
    def test_profiles_are_self_contained(self):
        for path in (ROOT/'profiles').glob('*.ini'):
            ini=configparser.ConfigParser(interpolation=None);ini.read(path)
            self.assertEqual(len(ini['map']),31)
            for target in ini['map'].values():
                full=target if target.startswith('/') else ini['profile']['resource_root']+'/'+target
                self.assertTrue(full.startswith('/data/sound/system/bgm_bridge/sinobuz24/'))
                self.assertNotIn('..',full)

    def test_patch_preserves_payload_and_signed_gain(self):
        source=template(1001);target=bytearray(source);target[30:32]=b'\x34\x12'
        changed=importer.patch(source,target,-132)
        self.assertEqual(changed[32:],source[32:])
        self.assertEqual(struct.unpack_from('<h',changed,18)[0],-132)
        self.assertEqual(changed[30:32],target[30:32])

    def test_codec_partial_block_roundtrip(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as directory:
            work=Path(directory);frames=1001
            source=work/'template.sd9';source.write_bytes(template(frames))
            tone=np.repeat((.2*np.sin(np.arange(frames)*2*np.pi*440/44100))[:,None],2,axis=1).ravel()
            (tone*32767).round().astype('<i2').tofile(work/'tone.pcm')
            importer.run(ROOT/'build/audio_codec.exe','encode',source,work/'tone.pcm',work/'encoded.sd9')
            _,decoded=importer.decode(work/'encoded.sd9',work/'decoded.pcm')
            self.assertEqual(len(decoded),len(tone))
            snr=10*np.log10(np.mean(tone**2)/np.mean((tone-decoded)**2))
            self.assertGreater(snr,20)
            self.assertEqual(struct.unpack_from('<I',(work/'encoded.sd9').read_bytes(),110)[0],frames)

    def test_reject_invalid_sd9(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as directory:
            path=Path(directory)/'bad.sd9';path.write_bytes(b'bad')
            with self.assertRaises(ValueError):importer.sd9(path)

if __name__=='__main__':unittest.main()
