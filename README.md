## ThiyaPlayer — Vulkan Based Hardware Video Engine for Android
ThiyaMediaEngine is a high-performance Android Media SDK written in C++/Vulkan. It implements a zero-copy architecture that bypasses standard memcpy bottlenecks by bridging AHardwareBuffer directly into a Vulkan Compute/Graphics pipeline. Designed for low-latency AI filtering, HDR tone mapping, and 4K+ playback without thermal throttling.

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

## Technical Challenges

- Vulkan external memory import for AHardwareBuffer
- Correct synchronization between MediaCodec and Vulkan pipelines
- Correct synchronization between audio codec and video codec
- YCbCr sampler conversion setup
- Compute pipeline integration
- Vulkan memory leak debugging
- Surface Listener for Hardware buffer extraction leakage prevention.(Gralloc leakage prevention for Images).
- GPU timestamp profiling.

## Future work

- HDR support
- Video filters with compute shaders for real time filters.
- Ai Model integration with zero copy direct gpu integration with luma extracted images.

