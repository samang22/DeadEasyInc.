"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
Imports the generated WAVs and fills empty weapon audio slots without changing weapon balance.
"""
import unreal, json
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
manifest=json.loads((root/'SourceAssets/Audio/Weapons/manifest.json').read_text(encoding='utf-8'))
tools=unreal.AssetToolsHelpers.get_asset_tools()
rows=[r for sounds in manifest['weapons'].values() for r in sounds.values()]
tasks=[]
for r in rows:
    if unreal.EditorAssetLibrary.does_asset_exist(r['asset']):continue
    task=unreal.AssetImportTask()
    task.set_editor_property('filename',str(root/r['wav']))
    task.set_editor_property('destination_path',r['asset'].rsplit('/',1)[0])
    task.set_editor_property('destination_name',r['asset'].rsplit('/',1)[1])
    task.set_editor_property('automated',True);task.set_editor_property('save',True)
    tasks.append(task)
if tasks:tools.import_asset_tasks(tasks)
for r in rows:
    w=unreal.load_asset(r['asset'])
    if not isinstance(w,unreal.SoundWave):raise RuntimeError('Missing SoundWave: '+r['asset'])
    w.set_editor_property('loading_behavior',unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
    w.set_sound_asset_compression_type(unreal.SoundAssetCompressionType.PCM)
    w.set_editor_property('looping',False)
    unreal.EditorAssetLibrary.save_loaded_asset(w)

def asset(name,cls,factory):
    path='/Game/DeadEasyInc/Audio/Weapons/Settings/'+name
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,path.rsplit('/',1)[0],cls,factory)
concurrency=asset('SC_WeaponSFX',unreal.SoundConcurrency,unreal.SoundConcurrencyFactory())
c=concurrency.get_editor_property('concurrency');c.set_editor_property('max_count',8)
c.set_editor_property('resolution_rule',unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
c.set_editor_property('volume_scale',.8);c.set_editor_property('voice_steal_release_time',.025)
concurrency.set_editor_property('concurrency',c);unreal.EditorAssetLibrary.save_loaded_asset(concurrency)
attenuation=asset('SA_WeaponImpact',unreal.SoundAttenuation,unreal.SoundAttenuationFactory())
a=attenuation.get_editor_property('attenuation')
a.set_editor_property('attenuate',True);a.set_editor_property('spatialize',True)
a.set_editor_property('attenuation_shape',unreal.AttenuationShape.SPHERE)
a.set_editor_property('attenuation_shape_extents',unreal.Vector(100,0,0));a.set_editor_property('falloff_distance',1700.)
attenuation.set_editor_property('attenuation',a);unreal.EditorAssetLibrary.save_loaded_asset(attenuation)
fields={'Swing':'swing_sound','HitOrganic':'organic_impact_sound','HitMetal':'metal_impact_sound','HitStructure':'structure_impact_sound','Fire':'fire_sound','ReloadStart':'reload_start_sound','ReloadInsert':'reload_insert_sound','ReloadEnd':'reload_end_sound','DryFire':'dry_fire_sound','Equip':'equip_sound','Unequip':'unequip_sound'}
volumes={'Knife':.7,'Bat':.75,'Axe':.75,'Pistol':.7,'Rifle':.6,'Shotgun':.8,'Sniper':.75}
report={'wave_count':len(rows),'bindings':{},'waves':[]}
for weapon,sounds in manifest['weapons'].items():
    d=unreal.load_asset('/Game/DeadEasyInc/Data/Weapons/DA_'+weapon)
    audio=d.get_editor_property('audio');fresh=not audio.get_editor_property('equip_sound')
    bound={}
    for event,r in sounds.items():
        field=fields[event]
        if not audio.get_editor_property(field):audio.set_editor_property(field,unreal.load_asset(r['asset']))
        bound[field]=audio.get_editor_property(field).get_path_name()
    audio.set_editor_property('concurrency',concurrency);audio.set_editor_property('impact_attenuation',attenuation)
    if fresh:audio.set_editor_property('volume',volumes[weapon])
    d.set_editor_property('audio',audio);unreal.EditorAssetLibrary.save_loaded_asset(d)
    report['bindings'][weapon]=bound
for r in rows:
    w=unreal.load_asset(r['asset']);duration=w.get_editor_property('duration');channels=w.get_editor_property('num_channels')
    if channels!=1 or abs(duration-r['duration'])>.02:raise RuntimeError('Invalid imported audio: '+r['asset'])
    report['waves'].append({'asset':r['asset'],'duration':duration,'channels':channels})
(root/'Saved/Logs/WeaponAudioBindings.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('WEAPON AUDIO: imported and bound '+str(len(rows))+' SoundWaves for '+str(len(report['bindings']))+' weapons')
