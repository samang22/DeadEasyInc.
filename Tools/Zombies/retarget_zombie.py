import unreal,json
E=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();base='/Game/Assets/StylizedDarkFantasyZombie'
bp=unreal.load_asset('/Game/DeadEasyInc/Zombies/Blueprints/BP_BorderZombie');c=unreal.get_default_object(bp.generated_class());mesh=c.get_component_by_class(unreal.SkeletalMeshComponent)
source=unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple');target=unreal.load_asset(base+'/Meshes/SK_SZombie_Variant_1')
rigs=[]
for name,sk in [('IK_Manny_ZombieSource',source),('IK_SZombie',target)]:
 path=base+'/Retargeting/'+name
 rig=unreal.load_asset(path) if E.does_asset_exist(path) else at.create_asset(name,base+'/Retargeting',unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
 ctl=unreal.IKRigController.get_controller(rig);ctl.set_skeletal_mesh(sk)
 for chain in list(ctl.get_retarget_chains()):ctl.remove_retarget_chain(chain.chain_name)
 if sk==source:
  chains=[('Root','root','root'),('Spine','spine_01','spine_05'),('Neck','neck_01','neck_02'),('Head','head','head'),('LeftClavicle','clavicle_l','clavicle_l'),('RightClavicle','clavicle_r','clavicle_r'),('LeftArm','upperarm_l','hand_l'),('RightArm','upperarm_r','hand_r'),('LeftLeg','thigh_l','ball_l'),('RightLeg','thigh_r','ball_r')];pelvis='pelvis'
 else:
  chains=[('Root','RL_BoneRoot','RL_BoneRoot'),('Spine','CC_Base_Waist','CC_Base_Spine02'),('Neck','CC_Base_NeckTwist01','CC_Base_NeckTwist02'),('Head','CC_Base_Head','CC_Base_Head'),('LeftClavicle','CC_Base_L_Clavicle','CC_Base_L_Clavicle'),('RightClavicle','CC_Base_R_Clavicle','CC_Base_R_Clavicle'),('LeftArm','CC_Base_L_Upperarm','CC_Base_L_Hand'),('RightArm','CC_Base_R_Upperarm','CC_Base_R_Hand'),('LeftLeg','CC_Base_L_Thigh','CC_Base_L_ToeBase'),('RightLeg','CC_Base_R_Thigh','CC_Base_R_ToeBase')];pelvis='CC_Base_Hip'
 ctl.set_retarget_root(pelvis);ctl.set_root_motion_bone(chains[0][1])
 for side in ['Left','Right']:
  suffix='l' if side=='Left' else 'r';cc='L' if side=='Left' else 'R'
  for finger,ccfinger in [('thumb','Thumb'),('index','Index'),('middle','Mid'),('ring','Ring'),('pinky','Pinky')]:
   start,end=(finger+'_01_'+suffix,finger+'_03_'+suffix) if sk==source else ('CC_Base_'+cc+'_'+ccfinger+'1','CC_Base_'+cc+'_'+ccfinger+'3')
   chains.append((side+ccfinger,start,end))
 for chain,start,end in chains:ctl.add_retarget_chain(chain,start,end,'')
 E.save_loaded_asset(rig);rigs.append(rig)
path=base+'/Retargeting/RTG_Manny_SZ​ombie'.replace('\u200b','')
rt=unreal.load_asset(path) if E.does_asset_exist(path) else at.create_asset(path.rsplit('/',1)[1],base+'/Retargeting',unreal.IKRetargeter,unreal.IKRetargetFactory())
ctl=unreal.IKRetargeterController.get_controller(rt)
ctl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE,rigs[0]);ctl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET,rigs[1]);ctl.add_default_ops();ctl.auto_map_chains(unreal.AutoMapChainType.FUZZY,True);ctl.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
E.save_loaded_asset(rt)
anims=[unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/'+p) for p in ['MM_Idle','Walk/MF_Unarmed_Walk_Fwd','Attack/MM_Attack_01']]
i=unreal.IKRetargetBatchOperationInputs();i.assets_to_retarget=[E.find_asset_data(a.get_path_name()) for a in anims];i.source_mesh=source;i.target_mesh=target;i.ik_retarget_asset=rt;i.target_path=base+'/Animations';i.prefix='SZ_';i.overwrite_existing_files=True
out=unreal.IKRetargetBatchOperation.run_batch_retarget(i)
unreal.log('RETARGET OUTPUT '+str(out))
for a in out:E.save_loaded_asset(a.get_asset())
open('C:/Report/Projects/DeadEasyInc/Saved/Logs/zombie_retarget.json','w').write(json.dumps({'outputs':[a.get_asset().get_path_name() for a in out]}))
