# 🌀 pensieve-dx 🌀
A 3D model viewer built on D3D12 using
- Mesh Shaders
- Shader Model 6.6 Dynamic Resources
- Enhanced Barriers

You have to use the included meshlet generator to create a meshletized version of your 3D model. Then you can feed the generated binary file into pensieve.

The following third party libraries are used:
- Assimp for model loading
- stb_image for texture loading
- DirectXMesh for meshlet generation
- D3D12 Memory Allocator for GPU memory management

![A screenshot of 400 000 cubes](screenshots/cubes.jpg)
![A screenshot of a community model of the Mark XVII Iron Man Armor](screenshots/mark17.jpg)
![A screenshot of the Stanford Lucy scan](screenshots/lucy.jpg)
