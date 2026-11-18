"""Configure the existing retro material and Main volume without changing exposure."""
import json
from pathlib import Path
import unreal

ROOT = '/Game/DeadEasyInc/Materials/'
assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
material = unreal.load_asset(ROOT + 'M_PP_Retro')
instance = unreal.load_asset(ROOT + 'MI_PP_Retro')
assert material and instance

# Reuse the asset identities so existing references remain valid.
editing.delete_all_material_expressions(material)
material.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
material.set_editor_property('blendable_location', unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)

def expression(cls, x, y):
    return editing.create_material_expression(material, cls, x, y)

def custom(description, code, inputs, output, x, y):
    node = expression(unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('description', description)
    node.set_editor_property('code', code)
    node.set_editor_property('output_type', output)
    node.set_editor_property('inputs', [unreal.CustomInput(input_name=name) for name in inputs])
    return node

def wire(source, output, target, input_name):
    assert editing.connect_material_expressions(source, output, target, input_name)

screen = expression(unreal.MaterialExpressionScreenPosition, -1100, -300)
resolution = expression(unreal.MaterialExpressionVectorParameter, -1100, -100)
resolution.set_editor_property('parameter_name', 'PixelResolution')
resolution.set_editor_property('default_value', unreal.LinearColor(640, 360, 0, 0))
uv = custom('Pixel centers with viewport-to-buffer UV conversion', '''
float2 R = max(Resolution.xy, float2(1, 1));
float2 PixelUV = (min(floor(saturate(UV) * R), R - 1) + 0.5) / R;
return ViewportUVToSceneTextureUV(PixelUV, 14);
''', ['UV', 'Resolution'], unreal.CustomMaterialOutputType.CMOT_FLOAT2, -750, -300)
wire(screen, 'ViewportUV', uv, 'UV')
wire(resolution, '', uv, 'Resolution')
scene = expression(unreal.MaterialExpressionSceneTexture, -450, -300)
scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
scene.set_editor_property('filtered', False)
wire(uv, '', scene, 'UVs')
color = expression(unreal.MaterialExpressionComponentMask, -200, -300)
color.set_editor_properties({'r': True, 'g': True, 'b': True, 'a': False})
wire(scene, 'Color', color, '')

parameters = {}
for index, (name, default) in enumerate([('ColorSteps', 24.0), ('DitherStrength', 0.8)]):
    node = expression(unreal.MaterialExpressionScalarParameter, -450, 100 + index * 180)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', default)
    parameters[name] = node

quantize = custom('Stable 4x4 ordered dithering and perceptual color steps', '''
float2 R = max(Resolution.xy, float2(1, 1));
int2 P = (int2)min(floor(saturate(UV) * R), R - 1);
int2 Lo = P & 1;
int2 Hi = (P >> 1) & 1;
float Rank = 4.0 * (2 * (Lo.x ^ Lo.y) + Lo.y) + 2 * (Hi.x ^ Hi.y) + Hi.y;
float Threshold = (Rank + 0.5) / 16.0 - 0.5;
float Steps = clamp(floor(ColorSteps + 0.5), 2.0, 256.0) - 1.0;
float3 Perceptual = pow(saturate(Color), 1.0 / 2.2);
float3 Quantized = saturate(floor(Perceptual * Steps + 0.5 + Threshold * saturate(DitherStrength)) / Steps);
return pow(Quantized, 2.2);
''', ['Color', 'UV', 'Resolution', 'ColorSteps', 'DitherStrength'],
    unreal.CustomMaterialOutputType.CMOT_FLOAT3, 150, -200)
wire(color, '', quantize, 'Color')
wire(screen, 'ViewportUV', quantize, 'UV')
wire(resolution, '', quantize, 'Resolution')
for name, node in parameters.items():
    wire(node, '', quantize, name)
assert editing.connect_material_property(quantize, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
editing.recompile_material(material)
editing.set_material_instance_parent(instance, material)
editing.update_material_instance(instance)
assert assets.save_loaded_asset(material)
assert assets.save_loaded_asset(instance)

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/DeadEasyInc/Maps/Main')
volumes = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
           if isinstance(actor, unreal.PostProcessVolume)]
assert len(volumes) == 1, 'Expected the existing single Main post process volume'
volume = volumes[0]
settings = volume.get_editor_property('settings')
blendables = settings.get_editor_property('weighted_blendables')
previous = list(blendables.get_editor_property('array'))
kept = []
for item in previous:
    obj = item.get_editor_property('object')
    # Replace the retro slot, including the plugin material selected during investigation.
    if obj and not obj.get_path_name().startswith(('/PixelizationMaterials/', ROOT + 'M_PP_Retro', ROOT + 'MI_PP_Retro')):
        kept.append(item)
kept.append(unreal.WeightedBlendable(weight=1.0, object=instance))
blendables.set_editor_property('array', kept)
settings.set_editor_property('weighted_blendables', blendables)
settings.set_editor_property('override_motion_blur_amount', True)
settings.set_editor_property('motion_blur_amount', 0.0)
volume.set_editor_property('settings', settings)
volume.set_editor_property('enabled', True)
volume.set_editor_property('unbound', True)
volume.set_editor_property('blend_weight', 1.0)
assert level.save_current_level()
statistics = editing.get_statistics(instance)
report = {'volume': volume.get_actor_label(), 'material': instance.get_path_name(),
          'blendables': [{'object': x.object.get_path_name(), 'weight': x.weight} for x in kept],
          'motion_blur': settings.motion_blur_amount, 'statistics': str(statistics),
          'scalars': list(map(str, editing.get_scalar_parameter_names(material))),
          'vectors': list(map(str, editing.get_vector_parameter_names(material)))}
Path(unreal.Paths.project_saved_dir(), 'Logs', 'RetroPostProcessSetup.json').write_text(json.dumps(report, indent=2))
unreal.log('RETRO POST PROCESS CONFIGURED')
