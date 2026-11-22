"""Create and bind the interaction outline. Run using UnrealEditor-Cmd -run=pythonscript."""
import unreal as ue

MATERIALS = '/Game/DeadEasyInc/Materials'
PARENT = f'{MATERIALS}/M_PP_InteractionOutline'
INSTANCE = f'{MATERIALS}/MI_PP_InteractionOutline'
PARAMETERS = f'{MATERIALS}/MPC_InteractionOutline'
HUD = '/Game/DeadEasyInc/UI/HUD/BP_BorderHUD'
MAIN = '/Game/DeadEasyInc/Maps/Main'
ACTOR_BLUEPRINTS = (
    '/Game/DeadEasyInc/Interaction/Blueprints/BP_GateSwitch',
    '/Game/DeadEasyInc/Buildings/Stations/BP_Workbench',
    '/Game/DeadEasyInc/Buildings/Stations/BP_ResearchDesk',
    '/Game/DeadEasyInc/Scavenging/Blueprints/BP_Handcart',
    '/Game/DeadEasyInc/Scavenging/Blueprints/BP_ElectricCart',
    '/Game/DeadEasyInc/Scavenging/Blueprints/BP_CorpseProcessor',
)

assets = ue.EditorAssetLibrary
editing = ue.MaterialEditingLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()


def get_or_create(path, cls, factory):
    if assets.does_asset_exist(path):
        return assets.load_asset(path)
    directory, name = path.rsplit('/', 1)
    asset = tools.create_asset(name, directory, cls, factory)
    if not asset:
        raise RuntimeError(f'Unable to create {path}')
    return asset


def configure_parameters():
    collection = get_or_create(PARAMETERS, ue.MaterialParameterCollection,
                               ue.MaterialParameterCollectionFactoryNew())
    scalars = list(collection.get_editor_property('scalar_parameters'))
    if not any(str(p.get_editor_property('parameter_name')) == 'UnfocusedStrength' for p in scalars):
        strength = ue.CollectionScalarParameter()
        strength.set_editor_property('parameter_name', 'UnfocusedStrength')
        strength.set_editor_property('default_value', 1.0)
        collection.set_editor_property('scalar_parameters', scalars + [strength])
        assert assets.save_loaded_asset(collection)
    blueprint = assets.load_asset(HUD)
    hud = ue.get_default_object(assets.load_blueprint_class(HUD))
    if hud.get_editor_property('outline_parameters') != collection:
        hud.set_editor_property('outline_parameters', collection)
        assert assets.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return collection


def configure_material():
    collection = configure_parameters()
    material = get_or_create(PARENT, ue.Material, ue.MaterialFactoryNew())
    editing.delete_all_material_expressions(material)
    material.set_editor_property('material_domain', ue.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property('blendable_location', ue.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    # Run after the existing retro pass so the outline stays clean and the scene is passed through.
    material.set_editor_property('blendable_priority', 100)

    def expr(cls, x, y):
        return editing.create_material_expression(material, cls, x, y)

    def scene_texture(texture, y):
        node = expr(ue.MaterialExpressionSceneTexture, -650, y)
        node.set_editor_property('scene_texture_id', texture)
        return node

    screen = expr(ue.MaterialExpressionScreenPosition, -650, -440)
    color = scene_texture(ue.SceneTextureId.PPI_POST_PROCESS_INPUT0, -270)
    stencil = scene_texture(ue.SceneTextureId.PPI_CUSTOM_STENCIL, -100)
    depth = scene_texture(ue.SceneTextureId.PPI_SCENE_DEPTH, 90)
    custom_depth = scene_texture(ue.SceneTextureId.PPI_CUSTOM_DEPTH, 260)
    tint = expr(ue.MaterialExpressionVectorParameter, -650, 440)
    tint.set_editor_property('parameter_name', 'OutlineColor')
    tint.set_editor_property('default_value', ue.LinearColor(.18, .78, 1., 1.))
    idle_tint = expr(ue.MaterialExpressionVectorParameter, -650, 630)
    idle_tint.set_editor_property('parameter_name', 'UnfocusedOutlineColor')
    idle_tint.set_editor_property('default_value', ue.LinearColor(0., 0., 0., 1.))
    width = expr(ue.MaterialExpressionScalarParameter, -650, 820)
    width.set_editor_property('parameter_name', 'Thickness')
    width.set_editor_property('default_value', 2.0)

    strength = expr(ue.MaterialExpressionCollectionParameter, -650, 985)
    strength.set_editor_property('collection', collection)
    strength.set_editor_property('parameter_name', 'UnfocusedStrength')

    outline = expr(ue.MaterialExpressionCustom, -90, -200)
    outline.set_editor_property('description', 'Visible outer silhouettes (245: unfocused, 246: focused)')
    outline.set_editor_property('output_type', ue.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = (
        ('UV', screen, 'ViewportUV'), ('Scene', color, 'Color'),
        ('CenterStencil', stencil, 'Color'), ('CenterDepth', depth, 'Color'),
        ('CenterCustomDepth', custom_depth, 'Color'), ('Tint', tint, ''),
        ('UnfocusedTint', idle_tint, ''), ('Thickness', width, ''),
        ('UnfocusedStrength', strength, ''),
    )
    custom_inputs = []
    for name, _, _ in inputs:
        pin = ue.CustomInput()
        pin.set_editor_property('input_name', name)
        custom_inputs.append(pin)
    outline.set_editor_property('inputs', custom_inputs)
    outline.set_editor_property('code', '''
if (CenterStencil.r > 244.5 && CenterStencil.r < 246.5)
    return Scene.rgb;

const float2 pixel = View.ViewSizeAndInvSize.zw;
const float2 stepUV = pixel * clamp(Thickness, 0.0, 6.0);
if (Thickness <= 0.0)
    return Scene.rgb;

// Test only covered neighbors, and compare linear depths to suppress outlines through geometry.
const float2 directions[8] = {
    float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1),
    float2(.7071, .7071), float2(-.7071, .7071),
    float2(.7071, -.7071), float2(-.7071, -.7071)
};
const float3 luminanceWeights = float3(.2126, .7152, .0722);
const float luminance = dot(Scene.rgb, luminanceWeights);
const float lit = smoothstep(.035, .18, luminance);
bool unfocusedEdge = false;
[unroll] for (int i = 0; i < 8; ++i)
{
    const float2 tap = clamp(UV + stepUV * directions[i], pixel * 0.5, 1.0 - pixel * 0.5);
    const float stamp = SceneTextureLookup(ViewportUVToSceneTextureUV(tap, PPI_CustomStencil), PPI_CustomStencil, false).r;
    if (stamp < 244.5 || stamp >= 246.5)
        continue;
    const float target = SceneTextureLookup(ViewportUVToSceneTextureUV(tap, PPI_CustomDepth), PPI_CustomDepth, false).r;
    const float surface = SceneTextureLookup(ViewportUVToSceneTextureUV(tap, PPI_SceneDepth), PPI_SceneDepth, false).r;
    const float limit = min(surface, CenterDepth.r);
    if (target > limit + max(1.0, limit * 0.001))
        continue;
    if (stamp > 245.5)
    {
        const float neutralTint = dot(Tint.rgb, luminanceWeights);
        const float3 highlight = lerp(neutralTint.xxx, Tint.rgb, lerp(.40, 1.0, lit));
        return lerp(Scene.rgb, highlight * lerp(.48, 1.0, lit), lerp(.48, .80, lit));
    }
    unfocusedEdge = true;
}
if (!unfocusedEdge)
    return Scene.rgb;

// Preserve the scene hue. In deep shadows add just enough luminance to read the edge.
const float3 hue = lerp(float3(.72, .82, 1.0), Scene.rgb / max(luminance, .025), smoothstep(.005, .04, luminance));
const float3 lifted = Scene.rgb + hue * max(.015, luminance * .18);
const float3 darkened = lerp(Scene.rgb, UnfocusedTint.rgb, .35);
return lerp(Scene.rgb, saturate(lerp(lifted, darkened, lit)), saturate(UnfocusedStrength));
''')
    for name, node, output in inputs:
        assert editing.connect_material_expressions(node, output, outline, name), name
    assert editing.connect_material_property(outline, '', ue.MaterialProperty.MP_EMISSIVE_COLOR)
    editing.recompile_material(material)
    assert assets.save_loaded_asset(material)

    instance = get_or_create(INSTANCE, ue.MaterialInstanceConstant, ue.MaterialInstanceConstantFactoryNew())
    if instance.get_editor_property('parent') != material:
        editing.set_material_instance_parent(instance, material)
        editing.update_material_instance(instance)
        assert assets.save_loaded_asset(instance)
    return instance


def opt_in_blueprints():
    subsystem = ue.get_engine_subsystem(ue.SubobjectDataSubsystem)
    library = ue.SubobjectDataBlueprintFunctionLibrary
    for path in ACTOR_BLUEPRINTS:
        blueprint = assets.load_asset(path)
        assert blueprint, path
        handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
        if any(isinstance(library.get_associated_object(library.get_data(handle)), ue.InteractionOutlineComponent)
               for handle in handles):
            continue
        params = ue.AddNewSubobjectParams()
        params.set_editor_property('parent_handle', handles[0])
        params.set_editor_property('new_class', ue.InteractionOutlineComponent)
        params.set_editor_property('blueprint_context', blueprint)
        handle, message = subsystem.add_new_subobject(params)
        if not library.is_handle_valid(handle):
            raise RuntimeError(f'{path}: {message}')
        ue.BlueprintEditorLibrary.compile_blueprint(blueprint)
        assert assets.save_loaded_asset(blueprint), path


def bind_to_main(instance):
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    assert levels.load_level(MAIN)
    volumes = [actor for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
               if isinstance(actor, ue.PostProcessVolume) and actor.get_editor_property('unbound')
               and actor.get_editor_property('enabled')]
    assert len(volumes) == 1, 'Expected one enabled unbound post-process volume in Main'
    volume = volumes[0]
    settings = volume.get_editor_property('settings')
    blendables = settings.get_editor_property('weighted_blendables')
    existing = list(blendables.get_editor_property('array'))
    if any(item.get_editor_property('object') == instance for item in existing):
        return
    entry = ue.WeightedBlendable()
    entry.set_editor_property('weight', 1.0)
    entry.set_editor_property('object', instance)
    existing.append(entry)
    blendables.set_editor_property('array', existing)
    settings.set_editor_property('weighted_blendables', blendables)
    volume.set_editor_property('settings', settings)
    assert levels.save_current_level()


if __name__ == '__main__':
    outline = configure_material()
    opt_in_blueprints()
    bind_to_main(outline)
    ue.log('INTERACTION OUTLINE: material, opt-in blueprints and Main volume saved')
