---
name: niagara-authoring
description: Create, modify, compile, and visually evaluate AAA-quality Niagara particle systems in the editor.
---
# Skill: Niagara VFX Authoring
## Description
Create, modify, compile, and visually evaluate AAA-quality Niagara particle systems in the editor.
## Arguments
- {{system_path}}: Long package path of the Niagara System (e.g. /Game/VFX/NS_Explosion)
- {{emitter_name}}: Name of the target emitter to modify
## Steps
1. **Asset Creation**:
   Use `create_niagara_system` to initialize a new blank Niagara System.
   
2. **Add Emitter Handles**:
   Use `add_niagara_emitter` to duplicate and register a standard high-quality base emitter. Choose from:
   - `SpriteBurst`: Classic CPU/GPU sprite bursts.
   - `RibbonTrail`: Continuous ribbon path rendering.
   - `MeshDebris`: Instanced mesh particle rendering.
   - `GPUSimulation`: High performance large scale particle simulations.

3. **Dynamic Logic Modules**:
   Use `add_niagara_module` to insert standard logic blocks into appropriate script stack layers (`EmitterSpawn`, `EmitterUpdate`, `ParticleSpawn`, `ParticleUpdate`). Common module types:
   - `AddVelocity`, `GravityForce`, `Drag`, `Collision`, `LightRenderer`, `AccelerationForce`.

4. **Parameter and Curve Binding**:
   - **Module Stack Input Pins (`set_niagara_module_pin`)**:
     Configure constants or time-value curves for module inputs (e.g., setting size/color curves over particle life).
   
   - **System/Emitter Level User Parameters & Curves (`set_niagara_parameter`)**:
     Use `set_niagara_parameter` to set exposed User parameters or dynamic curve parameter overrides on the system store:
     
     *Scalar / Color User Parameter*:
     ```json
     {
       "SystemAsset": "/Game/VFX/NS_Explosion",
       "ParameterScope": "User",
       "ParameterName": "SpawnRate",
       "DataType": "Float",
       "Value": 500.0
     }
     ```

     *Float Curve Override*:
     ```json
     {
       "SystemAsset": "/Game/VFX/NS_Explosion",
       "ParameterScope": "User",
       "ParameterName": "SizeOverLife",
       "DataType": "CurveFloat",
       "CurveKeys": [
         { "Time": 0.0, "Value": 10.0 },
         { "Time": 0.5, "Value": 50.0 },
         { "Time": 1.0, "Value": 0.0 }
       ]
     }
     ```
    
5. **Compilation Verification**:
   Call `compile_niagara_system` to build the script bytecode. Analyze the returned compile warnings and error messages to verify structural correctness.

6. **Temporal Vision Capture loop**:
   Call `capture_niagara_system_isolated` to retrieve a stitched 2x2 grid image representing the start, middle, peak, and dissipation keyframes of the simulation.
   * Examine time labels (`t = 0.5s`) and the visual scale indicator bar (e.g. `1m` or `10cm`) to evaluate size and speed.
   * Make adjustments to parameters using `set_niagara_module_pin` or `set_niagara_parameter` and iterate until target look matches user prompts/reference images.

## Performance Optimization Rules
- **GPU Throttling**: If particle spawn rate $\ge$ 1,000, change the simulation target (`SimTarget`) to GPU Simulation.
- **Fixed Bounds**: Always enable Fixed Bounds (instead of Dynamic Bounds) for finished systems to avoid runtime bounding calculation overhead.
- **Translucency Softness**: For smoke/fire sprites, always assign a material configured with soft-particle `DepthFade` to prevent clipping artifacts.
