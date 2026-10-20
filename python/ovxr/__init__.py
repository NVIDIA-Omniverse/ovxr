# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

"""Python bindings for ovxr — the OVRTX-to-OpenXR bridge.

ovxr drives a borrowed ``ovrtx.Renderer`` to render and submit mono, stereo, or
quad-view frames over OpenXR. Typical use mirrors ``ovrtx``::

    import ovrtx, ovstage, ovxr

    renderer = ovrtx.Renderer(ovrtx.RendererConfig(...))
    # OVXR 0.1 requires the default NONDECREASING (or FLOOR_ONLY) OVStage
    # ordinal admission policy; STRICTLY_INCREASING is not supported.
    stage = ovstage.Stage("my-app")
    renderer.attach_ovstage(stage)
    # ... populate the stage (ovstage.population.open_usd) and commit ...
    ctx = ovxr.Context(ovxr.ContextDesc(
        ovrtx_renderer=renderer,
        ovstage=stage,
        display_mode=ovxr.DisplayMode.STEREO,
        create_render_products=True,
    ))
    while running:
        ctx.render_frame(dt)            # renders + submits to OpenXR

The raw ctypes surface (``Bindings``, ``load``, the ``ovxr_*_t`` structs) is
available under ``ovxr._src.bindings`` for callers that need it.
"""

from ._src.bindings import (
    OVXR_LIBRARY_PATH_HINT,
    OVXR_LOADER_LIB_NAME,
    Bindings,
    DisplayMode,
    EnvironmentBlendMode,
    FeaturePolicy,
    FrameFormat,
    GraphicsApi,
    LocalFoveationMode,
    ReferenceSpaceType,
    Result,
    SceneInfoPolicy,
    SceneInfoSource,
    SessionMode,
    StageOrdinalMode,
    StageUpAxis,
    last_error,
    library_telemetry_opt_out,
    load,
    result_name,
)
from ._src.context import (
    Context,
    ContextDesc,
    Frame,
    FrameExpiredError,
    OpenXrHandles,
    OpenXrProbe,
    OvxrError,
    SceneInfo,
    VulkanHandles,
)
from ._src.settings import apply_renderer_settings

__version__ = "0.1.0.5842"

__all__ = [
    "__version__",
    # global config
    "OVXR_LIBRARY_PATH_HINT",
    "OVXR_LOADER_LIB_NAME",
    "library_telemetry_opt_out",
    # enums
    "Result",
    "FeaturePolicy",
    "DisplayMode",
    "SessionMode",
    "StageOrdinalMode",
    "EnvironmentBlendMode",
    "GraphicsApi",
    "LocalFoveationMode",
    "ReferenceSpaceType",
    "SceneInfoPolicy",
    "SceneInfoSource",
    "StageUpAxis",
    "FrameFormat",
    # high-level API
    "Context",
    "ContextDesc",
    "Frame",
    "FrameExpiredError",
    "SceneInfo",
    "OvxrError",
    "OpenXrHandles",
    "VulkanHandles",
    "OpenXrProbe",
    # renderer-global settings
    "apply_renderer_settings",
    # low-level binding surface
    "Bindings",
    "load",
    "last_error",
    "result_name",
]
