import argparse
import json
import math
import os
import sys

import bpy


def emit(event, job_id, progress):
    print(json.dumps({"event": event, "jobId": job_id, "progress": progress}), flush=True)


def write_summary(path, payload):
    if not path:
        return
    directory = os.path.dirname(path)
    if directory:
        os.makedirs(directory, exist_ok=True)
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as stream:
        json.dump(payload, stream, indent=2)
    os.replace(temporary, path)


def color(value, fallback=(0.18, 0.18, 0.18, 1.0)):
    if not isinstance(value, dict):
        return fallback
    return tuple(float(value.get(key, fallback[index])) for index, key in enumerate(("r", "g", "b", "a")))


def apply_position_keyframes(obj, transform, width, height, pixel_scale=0.01, frame_scale=1.0):
    for keyframe in transform.get("positionKeyframes", []):
        frame = int(round(float(keyframe.get("frame", 0)) * frame_scale))
        obj.location.x = (float(keyframe.get("x", width * 0.5)) - width * 0.5) * pixel_scale
        obj.location.y = (height * 0.5 - float(keyframe.get("y", height * 0.5))) * pixel_scale
        obj.keyframe_insert(data_path="location", frame=frame, index=0)
        obj.keyframe_insert(data_path="location", frame=frame, index=1)


def transform_object(obj, layer, width, height, pixel_scale=0.01):
    transform = layer.get("transform", {})
    obj.location = (
        (float(transform.get("px", width * 0.5)) - width * 0.5) * pixel_scale,
        (height * 0.5 - float(transform.get("py", height * 0.5))) * pixel_scale,
        -float(transform.get("pz", 0.0)) * pixel_scale,
    )
    obj.rotation_euler[2] = -math.radians(float(transform.get("rx", 0.0)))
    obj.scale.x *= float(transform.get("sx", 1.0))
    obj.scale.y *= float(transform.get("sy", 1.0))
    apply_position_keyframes(obj, transform, width, height, pixel_scale, float(layer.get("_frameScale", 1.0)))


def resolve_path(job_path, value):
    if not value:
        return ""
    return value if os.path.isabs(value) else os.path.normpath(os.path.join(os.path.dirname(job_path), value))


def apply_pbr_overrides(objects, layer, job_path):
    base_texture = resolve_path(job_path, layer.get("material.baseColorTexture", ""))
    metallic_roughness_texture = resolve_path(job_path, layer.get("material.metallicRoughnessTexture", ""))
    normal_texture = resolve_path(job_path, layer.get("material.normalTexture", ""))
    emission_texture = resolve_path(job_path, layer.get("material.emissionTexture", ""))
    base_color = color(layer.get("material.base.color"), (0.8, 0.8, 0.8, 1.0))
    metallic = min(1.0, max(0.0, float(layer.get("material.metallic", 0.0))))
    roughness = min(1.0, max(0.04, float(layer.get("material.roughness", 0.5))))
    opacity = min(1.0, max(0.0, float(layer.get("material.opacity", 1.0))))
    for obj in objects:
        if obj.type != "MESH" or (obj.data.materials and not base_texture and "material.base.color" not in layer):
            continue
        material = bpy.data.materials.new(str(layer.get("name", "Model")) + " PBR")
        material.use_nodes = True
        principled = material.node_tree.nodes.get("Principled BSDF")
        principled.inputs["Base Color"].default_value = base_color
        principled.inputs["Metallic"].default_value = metallic
        principled.inputs["Roughness"].default_value = roughness
        principled.inputs["Alpha"].default_value = base_color[3] * opacity
        if base_texture and os.path.isfile(base_texture):
            texture = material.node_tree.nodes.new("ShaderNodeTexImage")
            texture.image = bpy.data.images.load(base_texture, check_existing=True)
            material.node_tree.links.new(texture.outputs["Color"], principled.inputs["Base Color"])
            material.node_tree.links.new(texture.outputs["Alpha"], principled.inputs["Alpha"])
        if metallic_roughness_texture and os.path.isfile(metallic_roughness_texture):
            texture = material.node_tree.nodes.new("ShaderNodeTexImage")
            texture.image = bpy.data.images.load(metallic_roughness_texture, check_existing=True)
            texture.image.colorspace_settings.name = "Non-Color"
            separate = material.node_tree.nodes.new("ShaderNodeSeparateColor")
            material.node_tree.links.new(texture.outputs["Color"], separate.inputs["Color"])
            material.node_tree.links.new(separate.outputs["Green"], principled.inputs["Roughness"])
            material.node_tree.links.new(separate.outputs["Blue"], principled.inputs["Metallic"])
        if normal_texture and os.path.isfile(normal_texture):
            texture = material.node_tree.nodes.new("ShaderNodeTexImage")
            texture.image = bpy.data.images.load(normal_texture, check_existing=True)
            texture.image.colorspace_settings.name = "Non-Color"
            normal = material.node_tree.nodes.new("ShaderNodeNormalMap")
            material.node_tree.links.new(texture.outputs["Color"], normal.inputs["Color"])
            material.node_tree.links.new(normal.outputs["Normal"], principled.inputs["Normal"])
        if emission_texture and os.path.isfile(emission_texture):
            texture = material.node_tree.nodes.new("ShaderNodeTexImage")
            texture.image = bpy.data.images.load(emission_texture, check_existing=True)
            emission_input = principled.inputs.get("Emission Color") or principled.inputs.get("Emission")
            if emission_input:
                material.node_tree.links.new(texture.outputs["Color"], emission_input)
        obj.data.materials.clear()
        obj.data.materials.append(material)


def add_solid(layer, width, height, order):
    solid_width = float(layer.get("solidWidth", width))
    solid_height = float(layer.get("solidHeight", height))
    transform = layer.get("transform", {})
    px = float(transform.get("px", width * 0.5))
    py = float(transform.get("py", height * 0.5))
    sx = float(transform.get("sx", 1.0))
    sy = float(transform.get("sy", 1.0))
    rotation = math.radians(float(transform.get("rx", 0.0)))

    bpy.ops.mesh.primitive_plane_add(size=1.0, location=((px - width * 0.5) * 0.01, (height * 0.5 - py) * 0.01, -order * 0.001))
    obj = bpy.context.object
    obj.name = str(layer.get("name", "Solid"))
    obj.scale = (solid_width * sx * 0.01, solid_height * sy * 0.01, 1.0)
    obj.rotation_euler[2] = -rotation
    apply_position_keyframes(obj, transform, width, height, frame_scale=float(layer.get("_frameScale", 1.0)))

    material = bpy.data.materials.new(obj.name + " Material")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    emission = nodes.new("ShaderNodeEmission")
    rgba = color(layer.get("solidColor"))
    emission.inputs["Color"].default_value = rgba
    emission.inputs["Strength"].default_value = 1.0
    material.node_tree.links.new(emission.outputs["Emission"], output.inputs["Surface"])
    obj.data.materials.append(material)


def add_model(layer, width, height, job_path):
    source = resolve_path(job_path, layer.get("sourcePath", ""))
    before = set(bpy.context.scene.objects)
    imported_source = False
    if source and os.path.isfile(source):
        extension = os.path.splitext(source)[1].lower()
        try:
            if extension in (".gltf", ".glb"):
                bpy.ops.import_scene.gltf(filepath=source)
                imported_source = True
            elif extension == ".fbx":
                bpy.ops.import_scene.fbx(filepath=source)
                imported_source = True
            elif extension == ".obj":
                bpy.ops.wm.obj_import(filepath=source)
                imported_source = True
        except (RuntimeError, AttributeError):
            imported_source = False
    if not imported_source:
        geometry = int(layer.get("fixedGeometry", 0))
        if geometry == 1:
            bpy.ops.mesh.primitive_plane_add()
        elif geometry == 3:
            bpy.ops.mesh.primitive_uv_sphere_add()
        elif geometry == 4:
            bpy.ops.mesh.primitive_cylinder_add()
        elif geometry == 5:
            bpy.ops.mesh.primitive_cone_add()
        else:
            bpy.ops.mesh.primitive_cube_add()

    imported = [obj for obj in bpy.context.scene.objects if obj not in before]
    roots = [obj for obj in imported if obj.parent is None]
    for obj in roots:
        transform_object(obj, layer, width, height)
    apply_pbr_overrides(imported, layer, job_path)


def add_camera(layer, width, height):
    bpy.ops.object.camera_add()
    camera = bpy.context.object
    transform_object(camera, layer, width, height)
    camera.rotation_euler[0] += math.radians(90.0)
    projection = int(layer.get("cameraProjectionMode", 0))
    if projection == 1:
        camera.data.type = "ORTHO"
        camera.data.ortho_scale = float(layer.get("cameraOrthoHeight", height)) * 0.01
    else:
        camera.data.type = "PERSP"
        camera.data.angle = math.radians(float(layer.get("cameraFov", 50.0)))
    camera.data.lens = max(1.0, float(layer.get("cameraZoom", camera.data.lens)))
    camera.data.clip_start = max(0.001, float(layer.get("cameraNearClip", 0.1)) * 0.01)
    camera.data.clip_end = max(camera.data.clip_start + 1.0, float(layer.get("cameraFarClip", 100000.0)) * 0.01)
    if layer.get("cameraDepthOfField", False):
        camera.data.dof.use_dof = True
        camera.data.dof.focus_distance = max(0.001, float(layer.get("cameraFocusDistance", 1000.0)) * 0.01)
        camera.data.dof.aperture_fstop = max(0.1, float(layer.get("cameraAperture", 2.8)))
    bpy.context.scene.camera = camera


def add_light(layer, width, height):
    light_types = {0: "POINT", 1: "SPOT", 2: "SUN", 4: "AREA"}
    artifact_type = int(layer.get("light.type", 0))
    if artifact_type == 3:
        rgba = color(layer.get("light.color"), (1.0, 1.0, 1.0, 1.0))
        bpy.context.scene.world.color = tuple(channel * float(layer.get("light.intensity", 100.0)) / 100.0 for channel in rgba[:3])
        return
    data = bpy.data.lights.new(str(layer.get("name", "Light")), light_types.get(artifact_type, "POINT"))
    data.color = color(layer.get("light.color"), (1.0, 1.0, 1.0, 1.0))[:3]
    data.energy = max(0.0, float(layer.get("light.intensity", 100.0))) * 10.0
    data.use_shadow = bool(layer.get("light.castsShadows", True))
    if data.type == "SPOT":
        data.spot_size = math.radians(float(layer.get("light.coneAngle", 45.0)))
        angle = max(0.001, float(layer.get("light.coneAngle", 45.0)))
        feather = float(layer.get("light.coneFeather", 10.0))
        data.spot_blend = min(1.0, max(0.0, feather / angle))
    elif data.type == "AREA":
        data.shape = "DISK" if int(layer.get("light.areaShape", 0)) == 1 else "RECTANGLE"
        data.size = max(0.01, float(layer.get("light.areaWidth", 100.0)) * 0.01)
        if data.shape == "RECTANGLE":
            data.size_y = max(0.01, float(layer.get("light.areaHeight", 100.0)) * 0.01)
    obj = bpy.data.objects.new(data.name, data)
    bpy.context.collection.objects.link(obj)
    transform_object(obj, layer, width, height)


def configure_cycles_device(scene, requested):
    requested = str(requested or "auto").upper()
    if requested == "CPU":
        scene.cycles.device = "CPU"
        return "CPU"
    preferences = bpy.context.preferences.addons.get("cycles")
    if not preferences:
        scene.cycles.device = "CPU"
        return "CPU"
    cycles_preferences = preferences.preferences
    candidates = [requested] if requested not in ("", "AUTO", "GPU") else ["OPTIX", "HIP", "METAL", "ONEAPI", "CUDA"]
    for compute_type in candidates:
        try:
            cycles_preferences.compute_device_type = compute_type
            cycles_preferences.get_devices()
            enabled = False
            for device in cycles_preferences.devices:
                device.use = device.type != "CPU"
                enabled = enabled or device.use
            if enabled:
                scene.cycles.device = "GPU"
                return compute_type
        except (TypeError, RuntimeError):
            continue
    scene.cycles.device = "CPU"
    return "CPU"


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--job", required=True)
    args = parser.parse_args(argv)

    with open(args.job, "r", encoding="utf-8") as stream:
        job = json.load(stream)

    output = job["output"]
    composition = job["composition"]
    snapshot = job.get("snapshot", {})
    diagnostics = job.get("diagnostics", {})
    width = int(output["width"])
    height = int(output["height"])
    job_id = str(job.get("jobId", "artifact-cycles"))

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT" if not hasattr(scene, "cycles") else "CYCLES"
    quality = job.get("quality", {})
    requested_device = os.environ.get("ARTIFACT_CYCLES_DEVICE", quality.get("device", "auto"))
    active_device = configure_cycles_device(scene, requested_device) if scene.render.engine == "CYCLES" else "CPU"
    scene.render.resolution_x = width
    scene.render.resolution_y = height
    scene.render.resolution_percentage = 100
    scene.render.fps = max(1, int(round(float(composition.get("fps", 30.0)))))
    scene.render.image_settings.file_format = "OPEN_EXR" if str(output.get("format", "png")).lower() == "exr" else "PNG"
    scene.render.film_transparent = True

    bpy.ops.object.camera_add(location=(0.0, 0.0, 10.0))
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = float(height) * 0.01
    scene.camera = camera

    layers = snapshot.get("layers", [])
    has_camera = False
    supported_layers = 0
    for order, layer in enumerate(layers):
        if not layer.get("isVisible", True):
            continue
        layer_type = int(layer.get("type", -1))
        layer["_frameScale"] = float(composition.get("fps", 30.0)) / 24.0
        if layer_type == 3:
            add_solid(layer, width, height, order)
            supported_layers += 1
        elif layer_type == 18:
            add_model(layer, width, height, args.job)
            supported_layers += 1
        elif layer_type == 11:
            add_camera(layer, width, height)
            has_camera = True
            supported_layers += 1
        elif layer_type == 12:
            add_light(layer, width, height)
            supported_layers += 1

    if has_camera:
        bpy.data.objects.remove(camera, do_unlink=True)

    frame_start = int(composition["frameStart"])
    frame_end = int(composition["frameEnd"])
    total = max(1, frame_end - frame_start)
    os.makedirs(output["path"], exist_ok=True)
    emit("renderStarted", job_id, 0)
    print(json.dumps({"event": "renderDevice", "jobId": job_id, "device": active_device}), flush=True)
    for frame in range(frame_start, frame_end):
        cancel_file = diagnostics.get("cancelFile", "")
        if cancel_file and os.path.exists(cancel_file):
            emit("renderCanceled", job_id, int(((frame - frame_start) * 100) / total))
            write_summary(diagnostics.get("summaryFile", ""), {
                "event": "renderCanceled", "jobId": job_id, "device": active_device,
                "supportedLayerCount": supported_layers,
                "unsupportedLayerCount": len(layers) - supported_layers,
            })
            return 7
        scene.frame_set(frame)
        scene.render.filepath = os.path.join(output["path"], f"{composition.get('id', 'render')}_{frame:05d}")
        bpy.ops.render.render(write_still=True)
        emit("renderProgress", job_id, int(((frame - frame_start + 1) * 100) / total))
    emit("renderCompleted", job_id, 100)
    write_summary(diagnostics.get("summaryFile", ""), {
        "event": "renderCompleted", "jobId": job_id, "device": active_device,
        "supportedLayerCount": supported_layers,
        "unsupportedLayerCount": len(layers) - supported_layers,
        "frameCount": total, "outputPath": output["path"],
    })
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
