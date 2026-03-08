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

## Thread Architecture

- Extractor Thread
- Video Decoder Thread
- Audio Decoder Thread
- Surface Listener Thread
- Audio Stream Thread
- Vulkan Renderer Thread

- CPU ↔ CPU : mutex + condition variables
- CPU ↔ GPU : fences
- GPU ↔ GPU : semaphores

## Performance

Device: Moto G86
GPU: Mali G615 MP2

4K playback results:

YCbCr display pipeline: ~3 ms
Compute pipeline enabled: ~8 ms

Stable playback for 10+ minutes
No thermal throttling observed
