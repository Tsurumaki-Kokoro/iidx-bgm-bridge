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
from legacy_audio import SspBank, SPECS, adpcm_template

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
    def test_ssp_table_lookup_and_bounds(self):
        sound=template(1001);struct.pack_into('<H',sound,30,14)
        bank=bytearray(80072)+sound;bank[:13]=b'sys_sound.ssp'
        struct.pack_into('<I',bank,16,80072);struct.pack_into('<I',bank,72+14*320+11*4,80072)
        reader=SspBank(bank)
        self.assertEqual(reader.entry(14,11),sound)
        for group,index in [(250,0),(14,80),(14,0)]:
            with self.assertRaises(ValueError):reader.entry(group,index)
        with self.assertRaises(ValueError):SspBank(bank[:-20]).entry(14,11)
        corrupt=bytearray(bank);struct.pack_into('<H',corrupt,80072+30,15)
        with self.assertRaises(ValueError):SspBank(corrupt).entry(14,11)

    def test_all_legacy_profiles_and_fallbacks_exist(self):
        for profile,spec in SPECS.items():
            ini=configparser.ConfigParser(interpolation=None);ini.read(ROOT/f'profiles/{profile}.ini')
            self.assertEqual(ini['profile']['schedule'],'fixed')
            self.assertEqual(ini['playback']['strategy'],'staged_tracks' if spec.get('bank') else 'layered_sync')
            if not spec.get('bank'):self.assertTrue((ROOT/f'profiles/{profile}_compat.ini').is_file())

    def test_profiles_are_self_contained(self):
        for path in (ROOT/'profiles').glob('*.ini'):
            ini=configparser.ConfigParser(interpolation=None);ini.read(path)
            self.assertEqual(len(ini['map']),29 if path.stem in ('resort18','lincle19') else 31)
            for target in ini['map'].values():
                full=target if target.startswith('/') else ini['profile']['resource_root']+'/'+target
                self.assertTrue(full.startswith('/data/sound/system/bgm_bridge/'))
                self.assertNotIn('..',full)

    def test_host_registry_matches_compiled_specs(self):
        from host_support import builds
        header=(ROOT/'hosts/builds.hpp').read_text()
        for family,info in builds().items():
            self.assertIn(info['version'],header)
            for alias in info.get('aliases',[]):self.assertIn(alias,header)
            self.assertIn('"'+family+'"',header)
            self.assertNotIn('sha256',info)

    def test_version_detection_accepts_patched_dll(self):
        from host_support import detect
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as directory:
            game=Path(directory);(game/'prop').mkdir();(game/'modules').mkdir()
            dll=game/'modules/bm2dx.dll';dll.write_bytes(b'original fixture')
            (game/'prop/ea3-config.xml').write_text('<ea3><model>LDJ</model><ext>2025090300</ext></ea3>')
            bootstrap=game/'prop/bootstrap.xml'
            bootstrap.write_text('<config><!-- <release_code>2000010100</release_code> --><release_code>2026081900</release_code></config>')
            self.assertEqual(detect(game)[0],'iidx33')
            dll.write_bytes(b'patched fixture with different size and content')
            self.assertEqual(detect(game)[0],'iidx33')
            bootstrap.write_text('<config><release_code>2026091700</release_code></config>')
            self.assertEqual(detect(game)[0],'iidx34')
            bootstrap.write_text('<config><release_code>2026091600</release_code></config>')
            self.assertEqual(detect(game)[1]['version'],'2026091600')
            bootstrap.write_text('<config><release_code>2099010100</release_code></config>')
            with self.assertRaises(ValueError):detect(game)
            bootstrap.write_text('<config><release_code>invalid</release_code></config>')
            with self.assertRaises(ValueError):detect(game)
            bootstrap.write_text('<config><release_code>2026081900</release_code><release_code>2026091700</release_code></config>')
            with self.assertRaises(ValueError):detect(game)
            bootstrap.unlink()
            (game/'prop/ea3-config.xml').write_text('<ea3><model>LDJ</model><ext>2026081900</ext></ea3>')
            self.assertEqual(detect(game)[0],'iidx33')
            (game/'prop/ea3-config.xml').write_text('<ea3><model>KFC</model><ext>2026081900</ext></ea3>')
            with self.assertRaises(ValueError):detect(game)

    def test_intro_loop_is_preserved(self):
        source=template(1001);struct.pack_into('<II',source,20,400,4004)
        target=template(1001);data=importer.patch(source,target)
        self.assertEqual(data[20:29],source[20:29])
        self.assertEqual(data[32:],source[32:])

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

    def test_short_adpcm_blocks_preserve_frames(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as directory:
            work=Path(directory);frames=1001;source=work/'template.sd9'
            source.write_bytes(adpcm_template(template(frames),128))
            tone=np.repeat((.2*np.sin(np.arange(frames)*2*np.pi*440/44100))[:,None],2,axis=1).ravel()
            (tone*32767).round().astype('<i2').tofile(work/'tone.pcm')
            importer.run(ROOT/'build/audio_codec.exe','encode',source,work/'tone.pcm',work/'encoded.sd9')
            data,decoded=importer.decode(work/'encoded.sd9',work/'decoded.pcm')
            self.assertEqual(len(decoded),len(tone))
            self.assertEqual(struct.unpack_from('<H',data,64)[0],128)
            self.assertGreater(10*np.log10(np.mean(tone**2)/np.mean((tone-decoded)**2)),20)

    def test_reject_invalid_sd9(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as directory:
            path=Path(directory)/'bad.sd9';path.write_bytes(b'bad')
            with self.assertRaises(ValueError):importer.sd9(path)

if __name__=='__main__':unittest.main()
