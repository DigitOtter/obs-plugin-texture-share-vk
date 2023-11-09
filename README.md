# OBS Plugin for Texture Sharing between Vulkan and OpenGL instances

OBS Plugin to exchange textures with other plugins. Can be used to exchange images without performing a CPU roundtrip. 

## Build

### Linux

- Install the [texture-share-vk](https://github.com/DigitOtter/texture-share-vk) library
- Download repository
- Execute inside the repository directory: 
  ```bash
  git submodule update --init --recursive
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -GNinja
  cmake --build build
  sudo cmake --install build
  ```

### Windows

- Currently not supported (I'd recommend using the Spout2 OBS plugin on Windows)

## Installation

### Linux

#### Arch Linux

Available via the `obs-plugin-texture-share-vk-git` AUR package:

```bash
pikaur -S obs-plugin-texture-share-vk-git
```

## Usage

The plugin offers an additional scene and filter. The `texture-share-vk-filter-plugin` filter can be used to send an image, and the `texture-share-vk-source-plugin` source can be used to receive an image.

For the filter:
- Add the filter to a source
- Open the new filter's properties
- Set the name under which the image should be made available

For the source:
- Add the source to a scene
- In the source properties, set the name under which to look for external images

## Todos

- [ ] Fix problem with filter only working if it's the first one in the scene filter chain
- [ ] Add localization
