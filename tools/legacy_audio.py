"""Local-only IIDX 18–23 resource readers and preparation rules."""
import configparser
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
from datetime import datetime
import numpy as np

SPECS={
    'resort18':dict(title='beatmania IIDX 18 Resort Anthem',version='2011071200',bank=True,entry=11),
    'lincle19':dict(title='beatmania IIDX 19 Lincle',version='2012062500',bank=True,entry=8),
    'tricoro20':dict(title='beatmania IIDX 20 tricoro',version='2013090900',suffix=''),
    'spada21':dict(title='beatmania IIDX 21 SPADA',version='2014071600',suffix=''),
    'pendual22':dict(title='beatmania IIDX 22 PENDUAL',version='2015080500',suffix=''),
    'pendual22_future':dict(title='beatmania IIDX 22 PENDUAL',version='2015080500',suffix='_02'),
    'copula23':dict(title='beatmania IIDX 23 copula',version='2016083100',suffix=''),
}

class SspBank:
    """SSP offset table: 72-byte prefix, 250 groups, 80 uint32 offsets/group."""
    def __init__(self,data):
        if len(data)<72 or data[:13]!=b'sys_sound.ssp':raise ValueError('Invalid SSP header')
        self.data=data;self.header=struct.unpack_from('<I',data,16)[0]
        if self.header!=80072 or self.header>len(data):raise ValueError('Unsupported SSP table size')
    def entry(self,group,variant):
        if not (0<=group<250 and 0<=variant<80):raise ValueError('Invalid SSP index')
        offset=struct.unpack_from('<I',self.data,72+group*320+variant*4)[0]
        if offset<self.header or offset+32>len(self.data) or self.data[offset:offset+4]!=b'SD9\0':raise ValueError('Invalid/missing SSP entry')
        size=struct.unpack_from('<I',self.data,offset+8)[0]+32
        if size<122 or offset+size>len(self.data):raise ValueError('Truncated SSP entry')
        result=self.data[offset:offset+size]
        if struct.unpack_from('<H',result,30)[0]!=group:raise ValueError('SSP sound ID mismatch')
        return result

def source_location(source,spec):
    if spec.get('bank'):
        candidates=[source] if source.is_file() else [source/'sys_sound.ssp',source/'data/sd_data/sys_sd/sys_sound.ssp',source/'data/sound/sys_sd/sys_sound.ssp']
        for path in candidates:
            if path.is_file():return path
        raise ValueError('Select sys_sound.ssp, its sys_sd directory, or the source game root')
    if (source/'data/sound/system').is_dir():return source/'data/sound/system'
    if (source/'syssd_bgm_default_l0.sd9').is_file():return source
    raise ValueError('Select the source game root or data/sound/system directory')

def commit_tree(stage,output,asset_path):
    final=output/asset_path;final.parent.mkdir(parents=True,exist_ok=True);backup=None
    if final.exists():
        backup=final.with_name(final.name+'.backup-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'));final.rename(backup)
    try:shutil.move(str(stage/asset_path),str(final))
    except Exception:
        if backup is not None:backup.rename(final)
        raise
    return final

def adpcm_template(data,align):
    result=bytearray(data);fmt=result.find(b'fmt ')+8
    if fmt<8 or struct.unpack_from('<HH',result,fmt)!=(2,2):raise ValueError('Stereo MS ADPCM template required')
    samples=align-12
    if align<64 or samples<=0:raise ValueError('Invalid ADPCM block size')
    struct.pack_into('<I',result,fmt+8,44100*align//samples)
    struct.pack_into('<H',result,fmt+12,align);struct.pack_into('<H',result,fmt+18,samples)
    return result

def encode_mix(api,template,pcm,stage):
    """Keep a 20 dB quality floor; shorter legal blocks can improve old mixes."""
    original=template.read_bytes();fmt=original.find(b'fmt ')+8
    original_align=struct.unpack_from('<H',original,fmt+12)[0]
    for align in dict.fromkeys((original_align,128,64)):
        candidate=stage/'mix-template.sd9';candidate.write_bytes(adpcm_template(original,align))
        api.run(api.BIN/'audio_codec.exe','encode',candidate,stage/'mix.pcm',stage/'mix.sd9')
        mix,decoded=api.decode(stage/'mix.sd9',stage/'roundtrip.pcm')
        snr=10*np.log10(np.mean(pcm**2)/max(np.mean((decoded-pcm)**2),1e-30))
        if snr>20 and np.max(np.abs(decoded))<.99:return mix,float(snr),align
    raise ValueError(f'Full-mix quality check failed: SNR={snr:.2f} dB')

def prepare_legacy(api,profile,source,game,output):
    spec=SPECS[profile];source=source_location(source.resolve(),spec)
    game,output=game.resolve(),output.resolve();family,host=api.detect(game)
    profiles=api.ROOT/'modules/bgm_profiles' if api.FROZEN else api.ROOT/'profiles'
    ini=configparser.ConfigParser(interpolation=None);ini.read(profiles/f'{profile}.ini',encoding='utf-8')
    routes=list(ini['map']);asset_path=Path('data/sound/system/bgm_bridge')/profile
    # All source entries are validated before any installed resources are replaced.
    bank=SspBank(source.read_bytes()) if spec.get('bank') else None
    payloads={}
    if bank:
        payloads={f'l{i}':bank.entry(14,index) for i,index in enumerate((spec['entry'],0,1))}
        payloads['decide']=bank.entry(20,0)
    else:
        suffix=spec['suffix']
        payloads={f'l{i}':(source/f'syssd_bgm_default_l{i}{suffix}.sd9').read_bytes() for i in range(3)}
        payloads['decide']=(source/f'syssd_music_decide_se_default{suffix}.sd9').read_bytes()
        payloads['cardout']=(source/f'syssd_bgm_cardout{suffix}.sd9').read_bytes()
    for route in routes:api.sd9(game/'data/sound/system'/route)
    output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='bgm-bridge-',dir=output) as temporary:
        stage=Path(temporary);inputs={}
        for key,data in payloads.items():
            path=stage/f'{key}.sd9';path.write_bytes(data);api.sd9(path);inputs[key]=path
        headers=[];stems=[]
        for i in range(3):
            header,pcm=api.decode(inputs[f'l{i}'],stage/f'l{i}.pcm');headers.append(header);stems.append(pcm)
        if not bank:
            api.require(len({(len(p),h[20:29]) for p,h in zip(stems,headers)})==1,'Source stems have incompatible loops')
            frames=len(stems[0])//2
            api.require(struct.unpack_from('<II',headers[0],20)==(0,frames*4),'Zero-start, full-length stem loops required')
        reports=[]
        for variant in (0,1):
            base=game/f'data/sound/system/{variant}'
            refdata,ref=api.decode(base/'syssd_bgm_default_l3.sd9',stage/'reference.pcm')
            ref*=(128-struct.unpack_from('<h',refdata,18)[0])/127*62/127
            reference=api.measure(ref,stage/'measure.pcm')
            if bank:
                gains=[]
                for pcm in stems:
                    gain=10**((reference-api.measure(pcm,stage/'measure.pcm'))/20)/(62/127)
                    gain=min(gain,.89/(max(np.max(np.abs(pcm)),1e-10)*62/127))
                    gains.append((128-round(128-127*gain))/127)
                native=stage/asset_path/'default'/str(variant);native.mkdir(parents=True)
                for route in routes:
                    v,name=route.split('/')
                    if int(v)!=variant:continue
                    target=api.sd9(base/name)
                    if name=='syssd_music_decide_se.sd9':data=api.patch(payloads['decide'],target,struct.unpack_from('<h',target,18)[0])
                    else:
                        layer=int(name[-5]) if name.endswith(('_l0.sd9','_l1.sd9','_l2.sd9')) else 2
                        data=api.patch(headers[layer],target,round(128-127*gains[layer]))
                        api.require(data[20:29]==headers[layer][20:29] and data[32:]==headers[layer][32:],'Source loop/payload changed')
                    (native/name).write_bytes(data)
                reports.append(dict(variant=variant,reference_lufs=reference,stage_lufs=[api.measure(p*g*62/127,stage/'measure.pcm') for p,g in zip(stems,gains)],sample_peaks=[float(np.max(np.abs(p*g*62/127))) for p,g in zip(stems,gains)]))
            else:
                weights=[((128-struct.unpack_from('<h',h,18)[0])/127)**2 for h in headers]
                selected=sum(p*w for p,w in zip(stems,weights))*96/127
                def peak_for(w):
                    return max(np.max(np.abs(stems[0]*w[0])),np.max(np.abs(sum(p*g for p,g in zip(stems[:2],w[:2]))*110/127)),np.max(np.abs(sum(p*g for p,g in zip(stems,w))*96/127)))
                gain=min(10**((reference-api.measure(selected,stage/'measure.pcm'))/20),.89/max(peak_for(weights),1e-10))
                attenuation=[round(128-127*w*gain) for w in weights];quantized=[(128-a)/127 for a in attenuation]
                api.require(peak_for(quantized)<.94,'Quantized peak ceiling exceeded')
                actual=sum(p*g for p,g in zip(stems,quantized))*96/127
                native=stage/asset_path/'layered/default'/str(variant);native.mkdir(parents=True)
                for i in range(3):
                    data=api.patch(headers[i],api.sd9(base/f'syssd_bgm_default_l{i}.sd9'),attenuation[i])
                    api.require(data[32:]==headers[i][32:],'Stem payload changed')
                    (native/f'syssd_bgm_default_l{i}.sd9').write_bytes(data)
                (actual*32767).round().astype('<i2').tofile(stage/'mix.pcm')
                mix,snr,block_align=encode_mix(api,inputs['l2'],actual,stage)
                compat=stage/asset_path/'compat/default'/str(variant);compat.mkdir(parents=True)
                for route in routes:
                    v,name=route.split('/')
                    if int(v)!=variant:continue
                    target=api.sd9(base/name)
                    if name.endswith(('_l0.sd9','_l1.sd9')):
                        i=int(name[-5]);data=api.patch(headers[i],target,round(128-127*quantized[i]*127/62))
                    elif name.startswith('syssd_bgm_default_'):data=api.patch(mix,target,round(128-127*127/62))
                    elif name=='syssd_music_decide_se.sd9':data=api.patch(payloads['decide'],target,struct.unpack_from('<h',target,18)[0])
                    else:data=api.patch(payloads['cardout'],target)
                    (compat/name).write_bytes(data)
                reports.append(dict(variant=variant,reference_lufs=reference,selected_lufs=api.measure(actual,stage/'measure.pcm'),sample_peak=float(peak_for(quantized)),roundtrip_snr_db=float(snr),adpcm_block_align=block_align))
            print(f'{profile} / {variant}: ready',flush=True)
        selected={profile} if bank else {profile,profile+'_compat'}
        count=api.check_assets(stage,profiles,selected)
        dest=stage/asset_path
        files=[dict(path=p.relative_to(dest).as_posix(),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(dest.rglob('*.sd9'))]
        api.require(all(peak<.94 for r in reports for peak in r.get('sample_peaks',[])),'Staged peak ceiling exceeded')
        (dest/'manifest.json').write_text(json.dumps(dict(schema=1,profile=profile,source_version=spec['version'],host=family,host_version=host['version'],source_sha256={key:hashlib.sha256(value).hexdigest() for key,value in payloads.items()},calibration=reports,files=files),indent=2)+'\n',encoding='utf-8')
        final=commit_tree(stage,output,asset_path)
        print(f'Imported {len(files)} files; validated {count} mappings. Output: {final}')
