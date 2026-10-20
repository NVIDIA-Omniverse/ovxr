# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

"""High-level, Pythonic wrapper over the ovxr C API.

:class:`Context` is the ergonomic entry point (analogous to ``ovrtx.Renderer``):
build a :class:`ContextDesc`, construct a :class:`Context` from it, then drive
``render_frame`` / ``acquire_frame``. The renderer and stage are borrowed — the
application owns the ``ovrtx.Renderer`` / ``ovstage.Stage`` lifetimes and passes
the objects themselves via :attr:`ContextDesc.ovrtx_renderer` /
:attr:`ContextDesc.ovstage` (raw ctypes pointers and integer addresses are also
accepted for exotic interop).

Callers that need the raw ctypes surface can drop down to
``ovxr._src.bindings.Bindings`` / :func:`ovxr._src.bindings.load`.
"""

from __future__ import annotations

import ctypes
import warnings
from dataclasses import dataclass, field
from threading import Lock
from typing import Optional

from . import bindings as _b
from .bindings import (
    Bindings,
    FrameFormat,
    ReferenceSpaceType,
    Result,
    SceneInfoPolicy,
    SceneInfoSource,
    Setting,
    StageOrdinalMode,
    StageUpAxis,
    check_ordinal,
    ovx_string_t,
    ovxr_anchor_transform_t,
    ovxr_camera_transform_t,
    ovxr_prim_transform_t,
    ovxr_desc_t,
    ovxr_pose_t,
    ovxr_quaternion_t,
    ovxr_render_product_resolution_t,
    ovxr_setting_entry_t,
    ovxr_submit_desc_t,
    ovxr_vector3_t,
    ovxr_settings_t,
    ovx_string_t,
    result_name,
)


def _components(value, names: tuple[str, ...]) -> tuple[float, ...]:
    if all(hasattr(value, name) for name in names):
        return tuple(float(getattr(value, name)) for name in names)
    return tuple(float(component) for component in value)


def _openxr_pose(position, orientation) -> ovxr_pose_t:
    p = _components(position, ("x", "y", "z"))
    q = _components(orientation, ("x", "y", "z", "w"))
    if len(p) != 3:
        raise ValueError(f"OpenXR position requires 3 values, got {len(p)}")
    if len(q) != 4:
        raise ValueError(f"OpenXR orientation requires 4 values, got {len(q)}")
    return ovxr_pose_t(
        orientation=ovxr_quaternion_t(*q),
        position=ovxr_vector3_t(*p),
    )


class OvxrError(RuntimeError):
    """Raised when an ovxr C call returns a non-success :class:`Result`.

    Carries the failing :attr:`result` code and the renderer's ``last_error``
    string (when available).
    """

    def __init__(self, op: str, code: int, detail: str = ""):
        self.result = code
        self.detail = detail
        msg = f"ovxr {op} failed: {result_name(code)}"
        if detail:
            msg += f": {detail}"
        super().__init__(msg)


class FrameExpiredError(RuntimeError):
    """Raised when an expired :class:`Frame` is asked for its CUDA pointer."""


class _FrameLifetime:
    """Thread-safe validity state shared by a context and its frame wrappers."""

    def __init__(self):
        self._lock = Lock()
        self._capture_generation = 0
        self._capture_calls_in_flight = 0
        self._latest_sequence = 0
        self._context_alive = True

    def begin_capture_call(self) -> None:
        """Conservatively consume one pointer-lifetime slot before capture can begin."""
        with self._lock:
            self._capture_generation += 1
            self._capture_calls_in_flight += 1

    def end_capture_call(self) -> None:
        with self._lock:
            self._capture_calls_in_flight -= 1

    def capture_baseline(self) -> int:
        """Return the conservative capture generation for a frame acquired now.

        acquire_frame may overlap a capture-capable call. In that case the call
        might already be overwriting the older of the two buffers, so count
        every in-flight call against the frame being acquired.
        """
        with self._lock:
            return self._capture_generation - self._capture_calls_in_flight

    def register_frame(self, sequence: int, pre_acquire_baseline: int) -> int:
        """Observe a native sequence and return its conservative capture generation.

        ``pre_acquire_baseline`` is the :meth:`capture_baseline` sampled before
        the native acquire began, and is what the frame is stamped with. A
        capture-capable call can begin and end entirely across the acquire, so
        re-sampling the baseline here would only ever be more permissive:
        ``begin_capture_call`` bumps both terms and ``end_capture_call`` only
        the subtracted one, making the baseline monotonically non-decreasing.
        """
        with self._lock:
            self._latest_sequence = max(self._latest_sequence, sequence)
            return pre_acquire_baseline

    def invalidate_context(self) -> None:
        with self._lock:
            self._context_alive = False

    def validity(self, sequence: int, capture_generation: int) -> "tuple[bool, str]":
        with self._lock:
            if not self._context_alive:
                return False, "the owning Context has been destroyed"
            if self._latest_sequence >= sequence + 2:
                return False, f"capture sequence {self._latest_sequence} has been observed"
            if self._capture_generation >= capture_generation + 2:
                return False, "two subsequent capture-capable frame calls have begun"
            return True, ""


@dataclass(frozen=True)
class Frame:
    """Python-owned metadata and a guarded view of an OVXR-owned CUDA frame.

    The metadata is copied from the native ``ovxr_frame_view_t`` and remains
    readable for the lifetime of this object. :attr:`device_ptr`, however, is a
    borrowed, read-only CUDA device pointer owned by OVXR. It is valid only
    while :attr:`is_valid` is true and must never be written to or freed.

    OVXR double-buffers readback. A frame from capture sequence ``N`` expires
    when capture ``N + 2`` can begin, or when the owning :class:`Context` is
    destroyed. The high-level context conservatively treats each
    :meth:`Context.render_frame` or :meth:`Context.submit_frame` call as a
    possible capture. A call overlapping the acquisition of this frame counts
    as one of those captures, whether it was already in flight or began and
    finished while the frame was being acquired. A call that captured nothing
    still counts, so a frame can expire while OVXR's own pointer is still
    live. Reading :attr:`device_ptr` after expiry raises
    :class:`FrameExpiredError`.

    An integer pointer obtained while the frame is valid cannot later be
    revoked by Python. Do not cache it: finish and synchronize any CUDA work
    that uses it before allowing two more capture-capable calls to begin.

    Attributes:
        width: Image width in pixels.
        height: Image height in pixels.
        pitch_bytes: Row pitch in bytes (``width * 4`` for RGBA8).
        cuda_device_id: CUDA device that owns :attr:`device_ptr`.
        format: Pixel format; currently :attr:`FrameFormat.RGBA8_UNORM`. A
            format a newer runtime adds decodes to a synthetic
            ``FrameFormat.UNKNOWN_<value>`` member rather than raising.
        sequence: 1-based monotonic native capture sequence.
        predicted_display_time_ns: Producing frame's OpenXR predicted display
            time, or 0 when no OpenXR timing was available.
    """

    width: int
    height: int
    pitch_bytes: int
    cuda_device_id: int
    format: FrameFormat
    sequence: int
    predicted_display_time_ns: int
    _device_ptr: int = field(repr=False, compare=False)
    _lifetime: _FrameLifetime = field(repr=False, compare=False)
    _capture_generation: int = field(repr=False, compare=False)

    @classmethod
    def _from_c(cls, view, lifetime: _FrameLifetime, pre_acquire_baseline: int) -> "Frame":
        sequence = int(view.sequence)
        capture_generation = lifetime.register_frame(sequence, pre_acquire_baseline)
        return cls(
            width=int(view.width),
            height=int(view.height),
            pitch_bytes=int(view.pitch_bytes),
            cuda_device_id=int(view.cuda_device_id),
            format=FrameFormat(view.format),
            sequence=sequence,
            predicted_display_time_ns=int(view.predicted_display_time_ns),
            _device_ptr=int(view.device_ptr or 0),
            _lifetime=lifetime,
            _capture_generation=capture_generation,
        )

    @property
    def is_valid(self) -> bool:
        """Whether :attr:`device_ptr` may still be retrieved.

        This is a point-in-time check. When another thread drives the frame
        loop, coordinate CUDA consumption with that thread rather than relying
        on an earlier ``is_valid`` result.
        """
        valid, _ = self._lifetime.validity(self.sequence, self._capture_generation)
        return valid

    @property
    def device_ptr(self) -> int:
        """Read-only CUDA device-pointer value, raising after this frame expires."""
        valid, reason = self._lifetime.validity(self.sequence, self._capture_generation)
        if not valid:
            raise FrameExpiredError(f"OVXR frame sequence {self.sequence} has expired: {reason}")
        return self._device_ptr


@dataclass(frozen=True)
class OpenXrProbe:
    """Decoded OpenXR/Vulkan discovery snapshot from :meth:`Context.probe_openxr`.

    Attributes:
        result: :attr:`Result.SUCCESS` when the probe completed; other codes for
            partial discovery (inspect :attr:`raw` and ``Context.last_error``).
        runtime_name: Active OpenXR runtime, e.g. the CloudXR runtime.
        system_name: OpenXR system (headset / simulator) name.
        view_count: Views the runtime composites (one for mono, two for stereo,
            or four for quad view).
        recommended_swapchain_width: Runtime-recommended per-view width [px].
        recommended_swapchain_height: Runtime-recommended per-view height [px].
        session_created: Whether the probe brought a session up.
        raw: The underlying ``ovxr_openxr_probe_result_t`` with every field.
    """

    result: Result
    runtime_name: str
    system_name: str
    view_count: int
    recommended_swapchain_width: int
    recommended_swapchain_height: int
    session_created: bool
    raw: object

    @staticmethod
    def _decode(buffer: bytes) -> str:
        return bytes(buffer).split(b"\0", 1)[0].decode("utf-8", "replace")

    @classmethod
    def _from_c(cls, result: Result, probe) -> "OpenXrProbe":
        return cls(
            result=result,
            runtime_name=cls._decode(probe.runtime_name),
            system_name=cls._decode(probe.system_name),
            view_count=int(probe.view_count),
            recommended_swapchain_width=int(probe.recommended_swapchain_width),
            recommended_swapchain_height=int(probe.recommended_swapchain_height),
            session_created=bool(probe.session_created),
            raw=probe,
        )


@dataclass(frozen=True)
class OpenXrHandles:
    """Handles for the OpenXR session ovxr owns, from :meth:`Context.get_openxr_handles`.

    Raw 64-bit handle and entry-point values, so a separate consumer can drive
    input on the SAME session. ovxr remains the owner: read input, locate
    spaces and resolve entry points, but do not drive frames, change session
    state, or destroy anything. They are valid only until the owning
    :class:`Context` is destroyed.

    Attributes:
        xr_instance: ``XrInstance`` as an integer.
        xr_session: ``XrSession`` as an integer.
        xr_space: Reference ``XrSpace`` ovxr locates views against.
        xr_get_instance_proc_addr: ``PFN_xrGetInstanceProcAddr`` as an integer.
        xr_reference_space_type: :class:`ReferenceSpaceType` ``xr_space`` was created
            with (``STAGE`` preferred, then ``LOCAL_FLOOR``, then ``LOCAL``). For
            ``STAGE`` and ``LOCAL_FLOOR`` tracking ``y = 0`` is the physical floor.
        raw: The underlying ``ovxr_openxr_handles_t``.
    """

    xr_instance: int
    xr_session: int
    xr_space: int
    xr_get_instance_proc_addr: int
    xr_reference_space_type: int
    raw: object

    @classmethod
    def _from_c(cls, handles) -> "OpenXrHandles":
        space_type = int(handles.xr_reference_space_type)
        try:
            space_type = ReferenceSpaceType(space_type)
        except ValueError:
            pass
        return cls(
            xr_instance=int(handles.xr_instance),
            xr_session=int(handles.xr_session),
            xr_space=int(handles.xr_space),
            xr_get_instance_proc_addr=int(handles.xr_get_instance_proc_addr),
            xr_reference_space_type=space_type,
            raw=handles,
        )


@dataclass(frozen=True)
class VulkanHandles:
    """Borrowed session Vulkan binding from :meth:`Context.get_vulkan_handles`.

    Handles and external handle-type masks are integer values. Serialize queue
    access with OVXR calls and destroy application resources before the context.
    The context owns the instance, device, and queue; never destroy these handles.
    """

    vk_instance: int
    vk_physical_device: int
    vk_device: int
    vk_queue: int
    queue_family_index: int
    queue_index: int
    external_memory_handle_types: int
    external_semaphore_handle_types: int

    @classmethod
    def _from_c(cls, handles) -> "VulkanHandles":
        return cls(
            vk_instance=int(handles.vk_instance),
            vk_physical_device=int(handles.vk_physical_device),
            vk_device=int(handles.vk_device),
            vk_queue=int(handles.vk_queue),
            queue_family_index=int(handles.queue_family_index),
            queue_index=int(handles.queue_index),
            external_memory_handle_types=int(handles.external_memory_handle_types),
            external_semaphore_handle_types=int(handles.external_semaphore_handle_types),
        )


@dataclass(frozen=True)
class SceneInfo:
    """Resolved stage scale and up-axis from :meth:`Context.get_scene_info`.

    The values are resolved once during :class:`Context` initialization according
    to :attr:`ContextDesc.scene_info_policy`. They are a value copy, not a live
    view of OVStage metadata, and remain valid after the context is destroyed. The
    raw C struct is deliberately not retained so that lifetime remains unambiguous.

    Attributes:
        meters_per_unit: Resolved linear stage unit in meters.
        meters_per_unit_source: Whether the scale came from the default,
            descriptor, or OVStage metadata.
        up_axis: Resolved stage up-axis.
        up_axis_source: Whether the up-axis came from the default, descriptor,
            or OVStage metadata.
    """

    meters_per_unit: float
    meters_per_unit_source: SceneInfoSource
    up_axis: StageUpAxis
    up_axis_source: SceneInfoSource

    @classmethod
    def _from_c(cls, info) -> "SceneInfo":
        return cls(
            meters_per_unit=float(info.meters_per_unit),
            meters_per_unit_source=SceneInfoSource(info.meters_per_unit_source),
            up_axis=StageUpAxis(info.up_axis),
            up_axis_source=SceneInfoSource(info.up_axis_source),
        )


def _renderer_handle(renderer) -> int:
    """Return the raw ``ovrtx_renderer_t*`` value for :attr:`ContextDesc.ovrtx_renderer`.

    ovxr borrows a renderer that the application creates with the ovrtx Python
    package, but ``ovrtx.Renderer`` does not expose its native handle publicly, so
    reading it means touching a private attribute. That access is centralised here
    rather than repeated in every application: if ovrtx later publishes an
    accessor, only this function changes.

    Accepts an ``ovrtx.Renderer``, a raw ctypes pointer, or an int.

    Raises:
        TypeError: if no handle can be derived from ``renderer``.
    """
    for candidate in (renderer, getattr(renderer, "handle", None), getattr(renderer, "_handle", None)):
        address = _pointer_address(candidate)
        if address:
            return address
    raise TypeError(
        f"cannot derive an ovrtx renderer handle from {type(renderer).__name__}; "
        "pass a live ovrtx.Renderer, a ctypes pointer, or an int"
    )


def _ovstage_handle(stage) -> int:
    """Return the raw ``ovstage_instance_t*`` value for :attr:`ContextDesc.ovstage`.

    The counterpart of :func:`renderer_handle` for the mandatory borrowed OVStage:
    ``ovstage.Stage`` keeps its native instance in a private attribute, so the
    access is centralised here instead of repeated in every application.

    Accepts an ``ovstage.Stage``, a raw ctypes pointer, or an int.

    Raises:
        TypeError: if no handle can be derived from ``stage``.
    """
    for candidate in (stage, getattr(stage, "handle", None), getattr(stage, "_inst", None)):
        address = _pointer_address(candidate)
        if address:
            return address
    raise TypeError(
        f"cannot derive an ovstage instance handle from {type(stage).__name__}; "
        "pass a live ovstage.Stage, a ctypes pointer, or an int"
    )


def _pointer_address(obj) -> Optional[int]:
    """Address held by ``obj``, or None when it is not a usable pointer.

    ovrtx hands out ``POINTER(ovrtx_renderer_t)``, which carries its address in
    ``contents`` rather than ``value``, so casting is the only form that covers
    plain ints, ``c_void_p`` and typed struct pointers alike.
    """
    if isinstance(obj, int):
        return obj
    if obj is None:
        return None
    value = getattr(obj, "value", None)
    if isinstance(value, int):
        return value
    try:
        return ctypes.cast(obj, ctypes.c_void_p).value
    except (ctypes.ArgumentError, TypeError):
        return None


def _string_array_ptr(values, keepalive: list):
    """Build a ``const ovx_string_t[]`` from ``values`` and return it as a ``c_void_p``.

    Appends the encoded bytes + the ctypes array to ``keepalive`` so the C side's
    borrowed pointers stay valid until ``ovxr_create`` has copied them.
    """
    if not values:
        return ctypes.c_void_p(None)
    raw = [str(v).encode("utf-8") for v in values]
    arr = (ovx_string_t * len(raw))()
    for i, b in enumerate(raw):
        arr[i].ptr = b
        arr[i].length = len(b)
    keepalive.append(raw)
    keepalive.append(arr)
    return ctypes.cast(arr, ctypes.c_void_p)


@dataclass
class ContextDesc:
    """Pythonic mirror of ``ovxr_desc_t`` (see ``ovxr.h``) for :class:`Context`.

    Args:
        ovrtx_renderer: Native ``ovrtx_renderer_t*`` of the borrowed renderer, as
            an integer address (e.g. ``ovrtx.Renderer._handle`` cast to an int).
        application_name: OpenXR application name.
        runtime_prim_root: Stage root prim ovxr authors under.
        display_mode: A :class:`DisplayMode` value. ``AUTO`` prefers primary
            stereo and falls back to primary mono. ``QUAD_VIEW`` selects
            outer-left, outer-right, inset-left, and inset-right views and
            requires runtime support.
        session_mode: A :class:`SessionMode` value (VR / AR / AUTO).
        environment_blend_mode: An :class:`EnvironmentBlendMode` value.
        local_foveation_mode: A :class:`LocalFoveationMode` value. ``None``
            keeps the ``AUTO`` default: valid located runtime PQW enables warped
            foveation for OVXR-authored render products, otherwise foveation is
            off. Quad view resolves ``AUTO`` to disabled and requires explicit
            ``WARPED`` to enable local foveation on all four products.
        runtime_foveated_submission_policy: A :class:`FeaturePolicy` value. When
            ``None``, runtime PQW is disabled by the ovxr default. ``AUTO`` or
            ``REQUIRED`` opts in; when ``local_foveation_mode`` is ``AUTO``,
            valid located runtime PQW then selects warped foveation for
            OVXR-authored mono or stereo products. Quad view requires explicit
            ``WARPED`` for local foveation independently of runtime PQW submission.
        depth_submission_policy: A :class:`FeaturePolicy` value.
        ar_blend_translucency_as_opacity: 1 to treat translucency as opacity in
            AR blending, 0 to disable (None keeps the ovxr default).
        graphics_api: A :class:`GraphicsApi` value.
        host_wait_render_output: Force a CPU wait for renderer output completion
            before OVXR schedules its OpenXR transfer. None keeps the OVXR
            default, which is enabled on Linux and disabled on other platforms.
            Set to 0 to disable this CPU wait, including for cross-GPU transfers;
            GPU-side synchronization still orders the transfer after rendering.
        max_frames_in_flight: Frames ovxr may keep in flight.
        max_render_gpus: Reserved render-GPU limit. OVRTX currently chooses
            render-product placement.
        enable_debug_labels: Emit Vulkan/OpenXR debug labels.
        render_products: Render-product prim paths used for submission. With
            ``create_render_products`` disabled, these must already be prepared;
            OVXR authors only its required per-product eco-mode override on them.
            Supply one path per selected OpenXR view in runtime order (one for
            mono, two for stereo, four for
            quad view). ``AUTO`` uses the first of two stereo-ordered entries
            when it falls back to mono.
        camera_paths: Camera prim paths driven from the corresponding OpenXR
            views, with the same count and order as ``render_products``.
        meters_per_unit: Stage linear unit [m].
        create_render_products: Author render products from OpenXR view data.
            OVXR disables eco mode directly on every configured render product
            before submission; for prepared products, this happens during
            context creation when this field is false.
        render_vars: Render-var prim paths used when authoring products.
        source_render_product: Source product to derive authored products from.
        render_settings: Render-settings prim path.
        foveation_unwarp: Produce an unwarped readback/mirror frame.
        foveation_unwarped_width: Unwarped frame width [px] (0 = derive).
        foveation_unwarped_height: Unwarped frame height [px] (0 = derive).
        foveation_warped_resolution_multiplier: Warped product resolution scale.
        foveation_warped_inset_size: Warped foveation inset fraction.
        tracking_anchor_path: Camera/prim whose latest committed world transform
            is used as the anchor at every frame boundary. When omitted or
            missing, OVXR follows its managed ``<runtime_prim_root>/Anchor`` prim,
            initialized from the first eye camera's pre-existing pose.
        floor_aligned_anchor: Preserve only anchor translation and heading about
            the stage up axis. Pitch, roll, and authored scale are discarded.
            None keeps the enabled OVXR default; False applies the complete
            validated prim transform verbatim as a stage-space transform, which
            suits the managed prim and Xform rigs but not a Camera prim in a
            Z-up stage.
        up_axis: Stage up axis for mapping OpenXR +Y-up poses.
        extra_openxr_extensions: Additional OpenXR instance extensions to enable at
            ``xrCreateInstance``. ovxr enables only what it needs itself, so an
            application sharing ovxr's OpenXR handles with another stack (hand
            tracking, CloudXR action/data-channel entry points) must declare those
            extensions here or ``xrGetInstanceProcAddr`` fails with
            ``XR_ERROR_FUNCTION_UNSUPPORTED``. Each name is enabled when the runtime
            advertises it and skipped with a warning otherwise.
        depth_near_z: Eye-camera near clip plane [m]. Leave both planes 0 to keep the
            wide VR-sane defaults; a supplied pair must be positive, finite and
            near < far, or context creation fails.
        depth_far_z: Eye-camera far clip plane [m]. See :attr:`depth_near_z`.
        ovstage: The required borrowed OVStage scene representation — pass the
            ``ovstage.Stage`` object itself (a raw ctypes pointer or integer
            address is also accepted). OVXR reads scene metadata and anchors
            from this stage; the caller owns the stage lifetime. The stage's
            process must use the ``NONDECREASING`` (default) or ``FLOOR_ONLY``
            ``ovstage.OrdinalAdmissionPolicy``; ``STRICTLY_INCREASING`` is not
            supported in OVXR 0.1 and makes :class:`Context` creation fail.
        scene_info_policy: Where OVXR resolves scene scale and up-axis from (a
            :class:`SceneInfoPolicy` value; OVStage metadata wins under ``AUTO``).
        enable_frame_readback: Capture one render product's color output for
            :meth:`Context.acquire_frame`.
        readback_render_product_index: Zero-based configured render-product index.
            ``AUTO`` uses index zero after falling back to mono.
        readback_foveation_unwarp: Request an unwarped capture independently of
            OpenXR submission for runtime-created foveated render products.
        ordinal_mode: Selects whether OVXR privately allocates Stage ordinals
            or the embedding application supplies every Stage-writing call's
            ordinal. Creation itself performs no Stage writes.
        application_steps_renderer: Default ``False`` (OVXR steps the renderer
            in :meth:`Context.bind_frame`). ``True`` makes bind stop after
            sealing and reading ``plan.render_ordinal``; the application runs
            the ``ovrtx`` update and step at that ordinal itself and passes the
            result to :meth:`Context.submit_frame` as ``step_result``. Requires
            an explicit :meth:`Context.bind_frame`; :meth:`Context.render_frame`
            is unavailable in this mode.
    """

    ovrtx_renderer: object
    application_name: str = ""
    runtime_prim_root: str = "/OVXR"
    display_mode: Optional[int] = None
    session_mode: Optional[int] = None
    environment_blend_mode: Optional[int] = None
    local_foveation_mode: Optional[int] = None
    runtime_foveated_submission_policy: Optional[int] = None
    depth_submission_policy: Optional[int] = None
    ar_blend_translucency_as_opacity: Optional[int] = None
    graphics_api: Optional[int] = None
    host_wait_render_output: Optional[int] = None
    max_frames_in_flight: int = 1
    max_render_gpus: int = 1
    enable_debug_labels: bool = False
    render_products: list[str] = field(default_factory=list)
    camera_paths: list[str] = field(default_factory=list)
    meters_per_unit: float = 0.01
    create_render_products: bool = False
    render_vars: list[str] = field(default_factory=list)
    source_render_product: str = ""
    render_settings: str = ""
    foveation_unwarp: bool = False
    foveation_unwarped_width: int = 0
    foveation_unwarped_height: int = 0
    foveation_warped_resolution_multiplier: float = 0.5
    foveation_warped_inset_size: float = 0.4
    tracking_anchor_path: str = ""
    floor_aligned_anchor: Optional[bool] = None
    up_axis: int = StageUpAxis.Y
    extra_openxr_extensions: list[str] = field(default_factory=list)
    depth_near_z: float = 0.0
    depth_far_z: float = 0.0
    ovstage: object = None
    scene_info_policy: int = SceneInfoPolicy.AUTO
    enable_frame_readback: bool = False
    readback_render_product_index: int = 0
    readback_foveation_unwarp: bool = False
    ordinal_mode: StageOrdinalMode = StageOrdinalMode.PRIVATE
    application_steps_renderer: bool = False

    # Named setting fields and the ovxr_setting_int64_t key each one carries.
    _SETTING_FIELDS = (
        ("display_mode", Setting.DISPLAY_MODE),
        ("local_foveation_mode", Setting.LOCAL_FOVEATION_MODE),
        ("runtime_foveated_submission_policy", Setting.RUNTIME_FOVEATED_SUBMISSION_POLICY),
        ("depth_submission_policy", Setting.DEPTH_SUBMISSION_POLICY),
        ("session_mode", Setting.SESSION_MODE),
        ("environment_blend_mode", Setting.ENVIRONMENT_BLEND_MODE),
        ("ar_blend_translucency_as_opacity", Setting.AR_BLEND_TRANSLUCENCY_AS_OPACITY),
        ("graphics_api", Setting.GRAPHICS_API),
        ("floor_aligned_anchor", Setting.FLOOR_ALIGNED_ANCHOR),
        ("host_wait_render_output", Setting.HOST_WAIT_RENDER_OUTPUT),
    )

    def _to_c(self):
        """Build the ``ovxr_desc_t`` plus a keepalive list backing its borrowed pointers."""
        keepalive: list = []
        desc = ovxr_desc_t()
        # An explicitly falsy renderer (0 / None) marshals as NULL so the C-side
        # validation rejects it with its own message; anything else must coerce.
        desc.ovrtx_renderer = ctypes.c_void_p(_renderer_handle(self.ovrtx_renderer)) if self.ovrtx_renderer else None
        desc.application_name = self.application_name.encode("utf-8")
        desc.runtime_prim_root = self.runtime_prim_root.encode("utf-8")

        # Build the ovxr_settings_t block from the named setting fields (None =
        # unset, keeping the ovxr default for that key).
        setting_pairs = [
            (key, int(getattr(self, name))) for name, key in self._SETTING_FIELDS if getattr(self, name) is not None
        ]
        if setting_pairs:
            entries = (ovxr_setting_entry_t * len(setting_pairs))(
                *[ovxr_setting_entry_t.int_entry(k, v) for k, v in setting_pairs]
            )
            settings = ovxr_settings_t(entries, len(setting_pairs))
            keepalive.append(entries)
            keepalive.append(settings)
            desc.settings = ctypes.pointer(settings)

        desc.max_frames_in_flight = self.max_frames_in_flight
        desc.max_render_gpus = self.max_render_gpus
        desc.enable_debug_labels = self.enable_debug_labels

        desc.render_products = _string_array_ptr(self.render_products, keepalive)
        desc.render_product_count = len(self.render_products)
        desc.camera_paths = _string_array_ptr(self.camera_paths, keepalive)
        desc.camera_path_count = len(self.camera_paths)
        desc.meters_per_unit = self.meters_per_unit
        desc.create_render_products = self.create_render_products
        desc.render_vars = _string_array_ptr(self.render_vars, keepalive)
        desc.render_var_count = len(self.render_vars)
        desc.source_render_product = ovx_string_t.make(self.source_render_product or None)
        desc.render_settings = ovx_string_t.make(self.render_settings or None)
        desc.foveation_unwarp = self.foveation_unwarp
        desc.foveation_unwarped_width = self.foveation_unwarped_width
        desc.foveation_unwarped_height = self.foveation_unwarped_height
        desc.foveation_warped_resolution_multiplier = self.foveation_warped_resolution_multiplier
        desc.foveation_warped_inset_size = self.foveation_warped_inset_size
        desc.tracking_anchor_path = ovx_string_t.make(self.tracking_anchor_path or None)
        desc.up_axis = int(self.up_axis)
        desc.extra_openxr_extensions = _string_array_ptr(self.extra_openxr_extensions, keepalive)
        desc.extra_openxr_extension_count = len(self.extra_openxr_extensions)
        desc.depth_near_z = self.depth_near_z
        desc.depth_far_z = self.depth_far_z
        desc.ovstage = ctypes.c_void_p(_ovstage_handle(self.ovstage)) if self.ovstage else None
        desc.scene_info_policy = int(self.scene_info_policy)
        desc.enable_frame_readback = self.enable_frame_readback
        desc.readback_render_product_index = self.readback_render_product_index
        desc.readback_foveation_unwarp = self.readback_foveation_unwarp
        desc.ordinal_mode = int(StageOrdinalMode(self.ordinal_mode))
        desc.application_steps_renderer = bool(self.application_steps_renderer)
        return desc, keepalive


class Context:
    """A live ovxr context wrapping the borrowed OVRTX renderer and the OpenXR session.

    Construct from a :class:`ContextDesc`; call :meth:`destroy` to release
    it. The context manager is available only in ``StageOrdinalMode.PRIVATE``;
    application-ordinal contexts must be destroyed explicitly with another
    caller-owned ordinal. The OpenXR session is created on the first
    :meth:`render_frame` / :meth:`submit_openxr_blank_frame`.

    Creation raises :class:`OvxrError` when the borrowed stage's process
    selected the ``STRICTLY_INCREASING`` OVStage ordinal admission policy,
    which OVXR 0.1 does not support. Keep the ``NONDECREASING`` default or
    select ``NONDECREASING`` / ``FLOOR_ONLY`` through ``ovstage.StageConfig``
    before anything else in the process creates an ``ovstage.Stage``.

    Methods using one context must be externally serialized, even when called
    from different Python threads, except :meth:`acquire_frame`. Destruction is
    exclusive and must not overlap any method. The wrapper adds no per-context
    lock.

    Args:
        desc: The context description.
        bindings: An existing :class:`Bindings` to use; when omitted, the library
            is loaded lazily via :data:`ovxr.OVXR_LIBRARY_PATH_HINT` / the loader
            search path.
    """

    def __init__(self, desc: ContextDesc, bindings: Optional[Bindings] = None):
        # Establish a fully destructible Python object before the native call:
        # allocation or conversion failures after ovxr_create must not strand
        # the returned native context.
        self._handle = None
        self._uses_explicit_stage_ordinals = int(desc.ordinal_mode) == StageOrdinalMode.APPLICATION
        self._bindings = bindings if bindings is not None else _b._ovxr_loader.get_bindings()
        c_desc, self._desc_keepalive = desc._to_c()
        rc, handle = self._bindings.create(c_desc)
        if rc != Result.SUCCESS or handle is None:
            raise OvxrError("create", rc, self._bindings.get_last_error())
        self._handle = handle
        self._frame_lifetime = _FrameLifetime()

    # -- lifetime -----------------------------------------------------------
    def request_exit(self) -> None:
        """Cooperatively wake the OpenXR frame lifecycle for owner-thread teardown.

        This may overlap a frame call, but must not overlap :meth:`destroy`.
        The owning frame thread remains responsible for pumping session events,
        ending the session in ``STOPPING``, and destroying the context.
        """
        if getattr(self, "_handle", None) is not None:
            self._check("request_exit", self._bindings.request_exit(self._handle))

    def destroy(self, stage_ordinal: Optional[int] = None) -> None:
        """Destroy the context (idempotent); must not overlap another method.

        ``stage_ordinal`` is where the runtime overlay release is written. It
        may be omitted only for a context created in private-ledger mode. A
        context created in application-ordinal mode must be destroyed explicitly
        with another caller-owned ordinal; automatic teardown cannot safely mint
        from the application's allocator.
        """
        if getattr(self, "_handle", None) is not None:
            ordinal = self._resolve_stage_ordinal(stage_ordinal)
            # Destroy natively first, then expire the frames. A rejected ordinal
            # or a Python/FFI exception raises before anything was destroyed, so
            # invalidating first would expire every live frame of a context that
            # still exists; the handle is also kept so cleanup can be retried.
            rc = self._bindings.destroy(self._handle, ordinal)
            # Native destroy consumes the context even when its final Stage
            # release fails.
            self._frame_lifetime.invalidate_context()
            self._handle = None
            self._check("destroy", rc)

    def __enter__(self) -> "Context":
        if self._uses_explicit_stage_ordinals:
            raise RuntimeError(
                "the Context context-manager is only available in private-ledger mode; "
                "explicit Stage-ordinal owners must call destroy(stage_ordinal=...)"
            )
        return self

    def __exit__(self, *exc) -> None:
        self.destroy()

    def __del__(self):
        if getattr(self, "_handle", None) is not None and getattr(
            self, "_uses_explicit_stage_ordinals", False
        ):
            try:
                # Native mode validation rejects the private Stage release,
                # but ovxr_destroy still drains and consumes the context.
                self._bindings.destroy(self._handle, 0)
            except Exception:
                pass
            finally:
                # The context is gone either way, so its frames must expire with
                # it exactly as destroy() expires them; a frame left valid here
                # would hand out a device_ptr into released native resources.
                self._frame_lifetime.invalidate_context()
                self._handle = None
            # Warned only after the cleanup above: warnings.warn() RAISES when
            # ResourceWarning is configured as an error (-W
            # error::ResourceWarning), and warning first meant that raise
            # skipped the native destroy, the frame invalidation and the handle
            # clearing -- silently, since Python swallows __del__ exceptions.
            warnings.warn(
                "an explicit-ordinal OVXR Context was not destroyed; draining native resources "
                "without releasing its Stage overlay",
                ResourceWarning,
                stacklevel=2,
            )
            return
        try:
            self.destroy()
        except Exception:
            pass

    @property
    def last_error(self) -> str:
        """This thread's last ovxr error; safe to read without synchronization."""
        return self._bindings.get_last_error()

    def _check(self, op: str, rc: int) -> None:
        if rc != Result.SUCCESS:
            raise OvxrError(op, rc, self._bindings.get_last_error())

    def _resolve_stage_ordinal(self, stage_ordinal: Optional[int]) -> int:
        ordinal = check_ordinal(0 if stage_ordinal is None else stage_ordinal)
        if self._uses_explicit_stage_ordinals:
            if ordinal == 0:
                raise RuntimeError(
                    "an OVXR context created in application-ordinal mode requires "
                    "a caller-owned stage_ordinal for every Stage-writing call"
                )
        elif ordinal != 0:
            raise RuntimeError(
                "an OVXR context created in private-ledger mode cannot switch to "
                "application-owned Stage ordinals"
            )
        return ordinal

    # -- frame loop ---------------------------------------------------------
    def render_frame(
        self, delta_time_seconds: float, stage_ordinal: Optional[int] = None
    ) -> "tuple[Result, ovxr_frame_result_t, bool]":
        """``ovxr_render_frame`` — render + submit one frame.

        Returns ``(result, frame_result, ordinal_sealed)``. ``ordinal_sealed``
        carries :meth:`bind_frame`'s report through unchanged, and matters for
        the same reason: this call can publish ``stage_ordinal`` and then fail.

        The frame's OVStage writes land at ``stage_ordinal`` (0 = private ledger;
        see ``ovxr.h`` "OVStage ordinals"). Never raises: ``TIMEOUT`` (no client
        ready) and ``NOT_READY`` (session not up yet) are normal frame-loop states,
        and a transient hard error should not kill the loop — inspect ``result`` and
        :attr:`last_error`. ``delta_time_seconds == 0`` uses the runtime-suggested
        frame delta; a negative or non-finite value is rejected.

        Serialize with non-acquire methods on this context.
        """
        ordinal = self._resolve_stage_ordinal(stage_ordinal)
        self._frame_lifetime.begin_capture_call()
        try:
            rc, out, sealed = self._bindings.render_frame(self._handle, delta_time_seconds, ordinal)
            return Result(rc), out, sealed
        finally:
            self._frame_lifetime.end_capture_call()

    def begin_frame(self):
        """Begin a split frame loop and return ``(result, frame_plan)``.

        Serialize with non-acquire methods on this context.
        """
        rc, plan = self._bindings.begin_frame(self._handle)
        return Result(rc), plan

    def bind_frame(
        self, plan, delta_time_seconds: float, stage_ordinal: Optional[int] = None
    ) -> "tuple[Result, bool]":
        """Publish cached XR state at ``stage_ordinal``, seal it, and render it.

        Returns ``(result, ordinal_sealed)``. ``stage_ordinal`` is sealed before
        the renderer work and the renderer steps can still fail afterwards, so a
        failure does not mean the ordinal is untouched. Read ``ordinal_sealed``
        on failure: when true, mark the ordinal resolved in your allocator and
        continue from a fresh one — reusing it is rejected as at or below the
        write floor.

        Only co-writers need this; a sole writer may go straight from
        :meth:`begin_frame` to :meth:`submit_frame`, which binds at a private
        ordinal itself. Serialize with non-acquire methods on this context.

        ``NOT_READY`` leaves an application ordinal unconsumed; cancel the
        plan, begin again and bind the new plan with the same ordinal. The
        first renderable bind of a context authors the runtime prims, seeds the
        anchor, writes camera state and seals, all at this one ordinal -- it
        never asks for a second one.
        """
        ordinal = self._resolve_stage_ordinal(stage_ordinal)
        rc, sealed = self._bindings.bind_frame(self._handle, plan, delta_time_seconds, ordinal)
        return Result(rc), sealed

    def submit_frame(self, plan, *, step_result: int = 0, additional_layers=None):
        """Submit a plan from :meth:`begin_frame` (binding it first if the caller did not).

        ``step_result`` is only for application-stepped contexts
        (``ContextDesc.application_steps_renderer``): the ``ovrtx`` step result
        the application produced at ``plan.render_ordinal``; OVXR reads it and
        the application destroys it afterwards. It must stay 0 otherwise.
        ``additional_layers`` optionally holds ctypes pointers to native OpenXR
        composition layers (``XrCompositionLayerBaseHeader``-compatible structs),
        appended after the OVRTX scene in order. Keep their views, next chains,
        spaces and swapchains alive and finish their graphics submissions before
        calling; this method retains the pointers and the array through the
        native call. A malformed descriptor is rejected before the plan is
        consumed or implicitly bound. Serialize with non-acquire methods on this context.
        """
        owners = tuple(additional_layers) if additional_layers is not None else ()
        if len(owners) > 0xFFFFFFFF:
            raise ValueError("Too many composition layers")
        desc = None
        if owners or step_result:
            desc = _b.ovxr_submit_desc_t()
            desc.step_result = int(step_result)
        if owners:
            desc.additional_layers = (ctypes.c_void_p * len(owners))(
                *(ctypes.cast(layer, ctypes.c_void_p) for layer in owners)
            )
            desc.additional_layer_count = len(owners)
        self._frame_lifetime.begin_capture_call()
        try:
            rc, result = self._bindings.submit_frame(self._handle, plan, desc)
            return Result(rc), result
        finally:
            self._frame_lifetime.end_capture_call()

    def set_frame_render_products(self, render_products) -> None:
        """``ovxr_set_frame_render_products`` — application products in every bound step.

        ``render_products`` is a sequence of absolute RenderProduct prim paths
        rendered after OVXR's eye products in the frame's single OVRTX step, so
        they share its history and clock; an empty sequence clears the
        selection. Read their outputs from ``plan.step_result`` between
        :meth:`bind_frame` and :meth:`submit_frame` with the ``ovrtx`` package.
        Writes nothing to the Stage. Call between frames; serialize with
        non-acquire methods on this context.
        """
        self._check(
            "set_frame_render_products",
            self._bindings.set_frame_render_products(self._handle, list(render_products)),
        )

    def cancel_frame(self, plan) -> None:
        """Cancel a plan; serialize with non-acquire methods on this context."""
        self._bindings.cancel_frame(self._handle, plan)

    def acquire_frame(self) -> Optional[Frame]:
        """Return the latest configured render-product :class:`Frame`, or ``None``.

        ``None`` means readback is disabled, no frame has been captured yet, or
        another best-effort capture error occurred. On success, native metadata
        is copied into a Python object. Its guarded :attr:`Frame.device_ptr`
        remains borrowed from OVXR and expires at capture sequence ``N + 2`` or
        context destruction; see :class:`Frame` for the complete contract.

        May overlap methods on this context except :meth:`destroy`.
        """
        baseline = self._frame_lifetime.capture_baseline()
        rc, out = self._bindings.acquire_frame(self._handle)
        return Frame._from_c(out, self._frame_lifetime, baseline) if rc == Result.SUCCESS else None

    def get_device_info(self):
        """Return composition/render GPU identity and feature state.

        Runtime features are available before GPU topology is ready. In that
        state the returned GPU ids are ``-1`` and the remaining topology fields
        are zero; callers can check ``device_info.topology_ready`` explicitly.
        Other failures raise :class:`OvxrError`.

        Serialize with non-acquire methods on this context.
        """
        rc, out = self._bindings.get_device_info(self._handle)
        if rc == Result.NOT_READY:
            return out
        self._check("get_device_info", rc)
        return out

    def get_scene_info(self) -> SceneInfo:
        """Return the scene scale and up-axis resolved during initialization.

        This is available immediately after :class:`Context` creation, including
        when :attr:`ContextDesc.scene_info_policy` is
        :attr:`SceneInfoPolicy.AUTO`. The result is a copied snapshot and does
        not change when OVStage metadata is edited later. Call this while the
        context is alive; the returned :class:`SceneInfo` remains valid after
        :meth:`destroy`.

        Serialize with non-acquire methods on this context.
        """
        rc, out = self._bindings.get_scene_info(self._handle)
        self._check("get_scene_info", rc)
        return SceneInfo._from_c(out)

    def get_openxr_handles(self):
        """``ovxr_get_openxr_handles`` — :class:`OpenXrHandles` for the session ovxr owns, or ``None``.

        Returns ``None`` while the session does not exist yet (``OVXR_NOT_READY``,
        i.e. before the first :meth:`begin_frame` / :meth:`submit_openxr_blank_frame`),
        so a consumer waiting to share the session can poll with backoff.

        ovxr remains the owner: a consumer may read input, locate spaces, and
        resolve entry points against the shared instance, but must not drive
        frames, change session state, or destroy the handles, and they stay valid
        only until this context is destroyed.

        Serialize with non-acquire methods on this context.
        """
        rc, out = self._bindings.get_openxr_handles(self._handle)
        if rc == Result.NOT_READY:
            return None
        self._check("get_openxr_handles", rc)
        return OpenXrHandles._from_c(out)

    def get_vulkan_handles(self) -> Optional[VulkanHandles]:
        """Return the session's borrowed Vulkan binding, or None before creation.

        The returned :class:`VulkanHandles` contains integer handles and external
        handle-type masks. Serialize all access to its queue with OVXR calls.
        Destroy application-owned imports before destroying this context; never
        destroy the borrowed device, instance, or queue. Non-Vulkan contexts fail.
        """
        rc, out = self._bindings.get_vulkan_handles(self._handle)
        if rc == Result.NOT_READY:
            return None
        self._check("get_vulkan_handles", rc)
        return VulkanHandles._from_c(out)

    # -- runtime anchor -----------------------------------------------------
    def set_anchor_prim(self, prim_path: str) -> None:
        """Queue a prim whose committed world transform is tracked each frame.

        ``prim_path`` must be a non-empty absolute USD path naming a prim that
        is readable now; a missing prim raises here rather than at the next
        frame boundary. The path is copied, and the last anchor prim selected
        before a frame boundary wins. If the prim later disappears, frames keep
        the last committed anchor and a warning is logged once.

        This call writes nothing to the OVStage and therefore takes no ordinal.
        Its validation read sees SEALED state only, and so does the per-frame
        anchor read: a prim moved at an ordinal you have not sealed is not
        visible to the next :meth:`bind_frame`. Move it with
        :meth:`set_managed_anchor_transform` or :meth:`update_prim_transforms`
        and let OVXR's seal publish it.
        Serialize with non-acquire methods on this context.
        """
        self._check("set_anchor_prim", self._bindings.set_anchor_prim(self._handle, prim_path))

    def clear_anchor(self, stage_ordinal: Optional[int] = None) -> None:
        """Reset the managed anchor prim to identity and select it.

        The prim ``<runtime_prim_root>/Anchor`` is written at ``stage_ordinal``
        (``None``/0 = OVXR's private ledger), which is sealed on success; the
        selection takes effect at the next frame boundary and is ordered with
        :meth:`set_anchor_prim` and :meth:`set_managed_anchor_transform` (the
        last request before a boundary wins).
        Serialize with non-acquire methods on this context.
        """
        self._check(
            "clear_anchor",
            self._bindings.clear_anchor(self._handle, self._resolve_stage_ordinal(stage_ordinal)),
        )

    def set_managed_anchor_transform(self, transform, stage_ordinal: Optional[int] = None) -> None:
        """Publish a world transform on the managed anchor and select it.

        ``transform`` contains 16 row-major, row-vector values in stage units;
        translation is in elements 12..14. The write lands at ``stage_ordinal``
        (``None``/0 = OVXR's private ledger) and that ordinal is sealed on
        success, which is what makes the value visible to the next frame
        boundary. The managed anchor becomes active at that boundary, ordered
        with :meth:`set_anchor_prim` and :meth:`clear_anchor`; the last request
        before that boundary wins. Serialize with non-acquire methods on this
        context.
        """
        raw = ovxr_anchor_transform_t()
        raw.matrix = (ctypes.c_double * 16)(*transform)
        self._check(
            "set_managed_anchor_transform",
            self._bindings.set_managed_anchor_transform(
                self._handle, raw, self._resolve_stage_ordinal(stage_ordinal)
            ),
        )

    def get_anchor_prim(self) -> str:
        """Return the prim driving the active anchor (``<runtime_prim_root>/Anchor`` by default)."""
        rc, path = self._bindings.get_anchor_prim(self._handle)
        self._check("get_anchor_prim", rc)
        return path

    def get_anchor_transform(self) -> tuple[float, ...]:
        """Return the active anchor; serialize with non-acquire methods."""
        rc, transform = self._bindings.get_anchor_transform(self._handle)
        self._check("get_anchor_transform", rc)
        return tuple(transform.matrix)

    def transform_openxr_pose(
        self,
        position,
        orientation=(0.0, 0.0, 0.0, 1.0),
    ) -> tuple[float, ...]:
        """Map an OpenXR pose with the active frame-latched anchor.

        Position is in meters and orientation is an x/y/z/w unit quaternion
        (note the argument order is position first, unlike ``ovxr_pose_t``). Check
        the location's ``locationFlags`` first: an unlocated space commonly
        reports an all-zero orientation, which is rejected. Call after
        :meth:`begin_frame` to map application-owned controller, hand, ray, or
        spatial-UI poses exactly like the eye cameras for that frame. Returns 16
        row-major, row-vector values in stage units. Serialize with non-acquire
        methods on this context.
        """
        pose = _openxr_pose(position, orientation)
        rc, matrix = self._bindings.transform_openxr_pose(self._handle, pose)
        self._check("transform_openxr_pose", rc)
        return matrix

    def probe_openxr(self) -> "OpenXrProbe":
        """``ovxr_probe_openxr`` — OpenXR/Vulkan discovery snapshot.

        Diagnostic: carries the result code in :attr:`OpenXrProbe.result` rather
        than raising, so callers can log a partial probe and continue. The raw
        ctypes struct remains available as :attr:`OpenXrProbe.raw` for fields the
        wrapper does not surface.

        Serialize with non-acquire methods on this context.
        """
        rc, out = self._bindings.probe_openxr(self._handle)
        return OpenXrProbe._from_c(Result(rc), out)

    def submit_openxr_blank_frame(self) -> "tuple[Result, ovxr_openxr_frame_result_t]":
        """``ovxr_submit_openxr_blank_frame`` — bring up the session + submit one blank frame.

        Returns ``(result, frame_result)`` (does not raise — session bring-up is
        driven/observed by the caller).

        Serialize with non-acquire methods on this context.
        """
        rc, out = self._bindings.submit_openxr_blank_frame(self._handle)
        return Result(rc), out

    def update_camera_transforms(
        self, transforms: dict[str, "list[float]"], stage_ordinal: Optional[int] = None
    ) -> None:
        """``ovxr_update_camera_transforms`` — set camera prim transforms.

        Args:
            transforms: Maps a camera prim path to its row-major 4x4 transform
                (16 floats; translation in elements 12..14).
            stage_ordinal: OVStage ordinal the batch lands at (0 = private ledger).

        Serialize with non-acquire methods on this context.
        """
        items = list(transforms.items())
        arr = (ovxr_camera_transform_t * len(items))()
        for i, (path, matrix) in enumerate(items):
            raw = path.encode("utf-8")
            arr[i].camera_path = raw
            arr[i].matrix = (ctypes.c_double * 16)(*matrix)
        self._check(
            "update_camera_transforms",
            self._bindings.update_camera_transforms(self._handle, arr, self._resolve_stage_ordinal(stage_ordinal)),
        )

    def update_prim_transforms(
        self, transforms: dict[str, "list[float]"], stage_ordinal: Optional[int] = None
    ) -> None:
        """``ovxr_update_prim_transforms`` — set WORLD transforms for any prims.

        The general form of :meth:`update_camera_transforms` (same authoring:
        each prim opts out of its ancestors' transform stack). Args map a prim
        path to its row-major 4x4 transform (16 floats; translation in
        elements 12..14); ``stage_ordinal`` is where the batch lands (0 =
        private ledger).
        """
        items = list(transforms.items())
        arr = (ovxr_prim_transform_t * len(items))()
        for i, (path, matrix) in enumerate(items):
            raw = path.encode("utf-8")
            arr[i].prim_path = raw
            arr[i].matrix = (ctypes.c_double * 16)(*matrix)
        self._check(
            "update_prim_transforms",
            self._bindings.update_prim_transforms(self._handle, arr, self._resolve_stage_ordinal(stage_ordinal)),
        )

    def update_render_product_resolutions(
        self, resolutions: dict[str, "tuple[int, int]"], stage_ordinal: Optional[int] = None
    ) -> None:
        """``ovxr_update_render_product_resolutions`` — set render-product ``resolution``.

        Args:
            resolutions: Maps a render-product prim path to its ``(width, height)`` [px].
            stage_ordinal: OVStage ordinal the batch lands at (0 = private ledger).

        Serialize with non-acquire methods on this context.
        """
        items = list(resolutions.items())
        arr = (ovxr_render_product_resolution_t * len(items))()
        for i, (path, (w, h)) in enumerate(items):
            raw = path.encode("utf-8")
            arr[i].render_product_path = raw
            arr[i].width = w
            arr[i].height = h
        self._check(
            "update_render_product_resolutions",
            self._bindings.update_render_product_resolutions(
                self._handle, arr, self._resolve_stage_ordinal(stage_ordinal)
            ),
        )
