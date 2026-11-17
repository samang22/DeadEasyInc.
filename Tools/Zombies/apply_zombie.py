import unreal,json
E=unreal.EditorAssetLibrary;base='/Game/Assets/StylizedDarkFantasyZombie'
bp=unreal.load_asset('/Game/DeadEasyInc/Zombies/Blueprints/BP_BorderZombie');c=unreal.get_default_object(bp.generated_class());m=c.get_component_by_class(unreal.SkeletalMeshComponent)
sk=unreal.load_asset(base+'/Meshes/SK_SZombie_Variant_1');bounds=sk.get_imported_bounds();unreal.log('ZOMBIE BOUNDS '+str(bounds))
for prop,name in [('idle_animation','SZ_MM_Idle'),('walk_animation','SZ_MF_Unarmed_Walk_Fwd'),('attack_animation','SZ_MM_Attack_01')]:
 a=unreal.load_asset(base+'/Animations/'+name);assert a and a.get_skeleton()==sk.get_editor_property('skeleton');c.set_editor_property(prop,a)
m.set_skeletal_mesh_asset(sk);m.set_editor_property('override_materials',[]);m.set_physics_asset(sk.get_editor_property('physics_asset'))
# Preserve capsule and horizontal placement; imported mesh uses Unreal Z-up.
loc=m.get_editor_property('relative_location');loc.z=-90.0-(bounds.origin.z-bounds.box_extent.z);m.set_editor_property('relative_location',loc)
settings=c.get_editor_property('settings');settings.set_editor_property('headshot_bones',['CC_Base_Head']);settings.set_editor_property('solid_corpse_bones',['CC_Base_Hip','CC_Base_Pelvis','CC_Base_Waist','CC_Base_Spine01','CC_Base_Spine02']);settings.set_editor_property('corpse_push_bone','CC_Base_Hip')
E.save_loaded_asset(settings);unreal.BlueprintEditorLibrary.compile_blueprint(bp);E.save_loaded_asset(bp)
unreal.log('ZOMBIE REPLACEMENT SAVED')
