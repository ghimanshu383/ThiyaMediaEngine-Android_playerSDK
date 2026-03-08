## ThiyaPlayer — Vulkan Based Hardware Video Engine for Android
A high-performance Android video playback engine built with Vulkan, MediaCodec,
and zero-copy AHardwareBuffer integration. Supports 4K playback with compute
pipeline processing and GPU timing instrumentation.

## Features
--------

- Vulkan-based rendering pipeline
- Zero-copy MediaCodec → AHardwareBuffer → Vulkan integration
- YCbCr sampler conversion using VK_ANDROID_external_memory
- Compute pipeline for luma/chroma extraction (to own the ownership of frames to be used for filters or ai model integrations)
- Multi-threaded decode architecture
- GPU timestamp profiling
- ImGui debug overlay
- 4K playback tested on Mali G615 MP2
- No memory leaks (validated with Vulkan validation layers)

## Architecture Diagram
