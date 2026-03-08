graph TD
    title[<b>Thiya Media Engine</b>]
    style title fill:none,stroke:none,font-size:20px
    
    A[Video File FD] --> B[MediaExtractor Thread]
    B --> C[MediaCodec Video Decoder]
    C --> D[AImageReader]
    D --> E[AHardwareBuffer]
    E --> F[Vulkan Renderer]
    
    F --> G[Compute Pipeline]
    F --> H[Graphics Pipeline]
    
    G --> I[Luma/Chroma Extraction]
    H --> J[Display]