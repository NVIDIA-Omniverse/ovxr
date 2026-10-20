/* Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef OVXR_H
#define OVXR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ovrtx/ovrtx.h>

#if defined(_WIN32) && defined(OVXR_EXPORT_DYNAMIC)
#    define OVXR_API __declspec(dllexport)
#elif defined(__GNUC__) && defined(OVXR_EXPORT_DYNAMIC)
#    define OVXR_API __attribute__((visibility("default")))
#else
#    define OVXR_API
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ovxr_context_t ovxr_context_t;
typedef struct ovxr_frame_t ovxr_frame_t;

typedef enum ovxr_result_t
{
    OVXR_SUCCESS = 0,
    OVXR_TIMEOUT = 1,
    OVXR_NOT_READY = 2,
    OVXR_ERROR = -1,
    OVXR_ERROR_OPENXR = -2,
    OVXR_ERROR_OVRTX = -3,
    OVXR_ERROR_DEVICE_MISMATCH = -4,
} ovxr_result_t;

typedef enum ovxr_stage_ordinal_mode_t
{
    /* OVXR is the sole OVStage writer and allocates its own ordinals. */
    OVXR_STAGE_ORDINAL_PRIVATE = 0,
    /* The application co-writes OVStage and supplies every writing call's ordinal. */
    OVXR_STAGE_ORDINAL_APPLICATION = 1,
} ovxr_stage_ordinal_mode_t;

typedef enum ovxr_feature_policy_t
{
    OVXR_FEATURE_AUTO = 0,
    OVXR_FEATURE_DISABLED = 1,
    OVXR_FEATURE_REQUIRED = 2,
} ovxr_feature_policy_t;

typedef enum ovxr_display_mode_t
{
    /* Prefer primary stereo, falling back to primary mono when stereo is unavailable. */
    OVXR_DISPLAY_MODE_AUTO = 0,
    /* Select XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO (left, right). */
    OVXR_DISPLAY_MODE_STEREO = 1,
    /*
     * Select the four-view foveated-inset configuration (outer left, outer
     * right, inset left, inset right). The runtime must advertise this view
     * configuration. OVXR_LOCAL_FOVEATION_AUTO resolves to disabled in this
     * mode; select OVXR_LOCAL_FOVEATION_WARPED explicitly to warp all four
     * renderer-local views.
     */
    OVXR_DISPLAY_MODE_QUAD_VIEW = 2,
    /* Select XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO (one view). */
    OVXR_DISPLAY_MODE_MONO = 3,
} ovxr_display_mode_t;

typedef enum ovxr_session_mode_t
{
    OVXR_SESSION_MODE_AUTO = 0,
    OVXR_SESSION_MODE_VR = 1,
    OVXR_SESSION_MODE_AR = 2,
} ovxr_session_mode_t;

typedef enum ovxr_environment_blend_mode_t
{
    OVXR_ENVIRONMENT_BLEND_MODE_AUTO = 0,
    OVXR_ENVIRONMENT_BLEND_MODE_OPAQUE = 1,
    OVXR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND = 2,
    OVXR_ENVIRONMENT_BLEND_MODE_ADDITIVE = 3,
} ovxr_environment_blend_mode_t;

typedef enum ovxr_local_foveation_mode_t
{
    /* Leave the decision to the runtime: valid located runtime PQW enables warped
       foveation for OVXR-authored render products, otherwise foveation is off.
       Quad view is opt-in and resolves AUTO to DISABLED. This is the default and
       matches the previous behavior of DISABLED. */
    OVXR_LOCAL_FOVEATION_AUTO = 0,
    /* Do not request local foveation, and do not let runtime PQW enable it. */
    OVXR_LOCAL_FOVEATION_DISABLED = 1,
    /* Always author locally foveated render products with piecewise-quadratic warping,
       including all four products when quad view is selected. */
    OVXR_LOCAL_FOVEATION_WARPED = 2,
} ovxr_local_foveation_mode_t;

typedef enum ovxr_graphics_api_t
{
    /* Select the default graphics API (Vulkan). */
    OVXR_GRAPHICS_API_AUTO = 0,
    OVXR_GRAPHICS_API_VULKAN = 1,
    OVXR_GRAPHICS_API_D3D12 = 2,
} ovxr_graphics_api_t;

typedef enum ovxr_stage_up_axis_t
{
    /* Default; OpenXR's +Y-up tracking poses are used unchanged. */
    OVXR_STAGE_UP_AXIS_Y = 0,
    /* Apply a +90-degree about-X basis change so poses land in a Z-up stage. */
    OVXR_STAGE_UP_AXIS_Z = 1,
} ovxr_stage_up_axis_t;

/* Reference-space kinds OVXR may use for its shared OpenXR space. */
typedef enum ovxr_reference_space_type_t
{
    OVXR_REFERENCE_SPACE_TYPE_VIEW = 1,
    OVXR_REFERENCE_SPACE_TYPE_LOCAL = 2,
    OVXR_REFERENCE_SPACE_TYPE_STAGE = 3,
    OVXR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR = 1000426000,
} ovxr_reference_space_type_t;

typedef enum ovxr_stage_scene_info_policy_t
{
    OVXR_STAGE_SCENE_INFO_AUTO = 0,
    OVXR_STAGE_SCENE_INFO_FROM_DESC = 1,
    OVXR_STAGE_SCENE_INFO_FROM_OVSTAGE = 2,
} ovxr_stage_scene_info_policy_t;

typedef enum ovxr_scene_info_source_t
{
    OVXR_SCENE_INFO_SOURCE_DEFAULT = 0,
    OVXR_SCENE_INFO_SOURCE_DESCRIPTOR = 1,
    OVXR_SCENE_INFO_SOURCE_OVSTAGE = 2,
} ovxr_scene_info_source_t;

typedef enum ovxr_setting_int64_t
{
    OVXR_SETTING_DISPLAY_MODE,
    OVXR_SETTING_LOCAL_FOVEATION_MODE,
    /* Controls XR_NV_foveation_piecewise_quadratic_warp (or legacy XR_NVX1 spelling) enablement.
       The default is OVXR_FEATURE_DISABLED. AUTO or REQUIRED opts in; when the local foveation mode is
       AUTO, valid located runtime PQW then selects warped foveation for OVXR-authored mono or stereo render
       products. Extension transport remains available for every selected view configuration. */
    OVXR_SETTING_RUNTIME_FOVEATED_SUBMISSION_POLICY,
    /* Numeric slot 3 is intentionally unused to preserve the setting-key ABI. */
    OVXR_SETTING_DEPTH_SUBMISSION_POLICY = 4,
    OVXR_SETTING_SESSION_MODE,
    OVXR_SETTING_ENVIRONMENT_BLEND_MODE,
    OVXR_SETTING_AR_BLEND_TRANSLUCENCY_AS_OPACITY,
    OVXR_SETTING_GRAPHICS_API,
    /* Boolean 0/1. Default: 1. See the runtime anchor documentation below. */
    OVXR_SETTING_FLOOR_ALIGNED_ANCHOR,
    /* Force a CPU wait for renderer output completion before OVXR schedules
       the OpenXR transfer. The default value is 1 on Linux and 0 on other platforms.
       Set to 0 to disable this CPU wait, including for cross-GPU transfers;
       GPU-side synchronization still orders the transfer after rendering. */
    OVXR_SETTING_HOST_WAIT_RENDER_OUTPUT,
} ovxr_setting_int64_t;

typedef enum ovxr_setting_key_type_t
{
    OVXR_SETTING_KEY_TYPE_INT64 = 0,
} ovxr_setting_key_type_t;

typedef struct ovxr_setting_entry_t
{
    ovxr_setting_key_type_t key_type;
    union
    {
        ovxr_setting_int64_t int64_key;
    } key;
    union
    {
        int64_t int_value;
    } value;
} ovxr_setting_entry_t;

static inline ovxr_setting_entry_t ovxr_setting_entry_int(ovxr_setting_int64_t key, int64_t value)
{
    ovxr_setting_entry_t entry;
    entry.key_type = OVXR_SETTING_KEY_TYPE_INT64;
    entry.key.int64_key = key;
    entry.value.int_value = value;
    return entry;
}

typedef struct ovxr_settings_t
{
    const ovxr_setting_entry_t* entries;
    size_t entry_count;
} ovxr_settings_t;

/*
 * OVStage ordinals.
 *
 * OVXR and its applications share one borrowed OVStage. Writes there are
 * ordinal-keyed, and publication is a stage-wide write-floor advance: naive
 * co-writing (each writer deriving "floor + 1" and advancing the floor
 * itself) lets two writers pick the same ordinal and lets either writer's
 * floor advance publish the other's half-written batch. OVXR removes both
 * hazards by never choosing an ordinal when it shares the stage: every entry
 * point that writes the OVStage (ovxr_bind_frame, ovxr_render_frame,
 * ovxr_update_prim_transforms and its siblings,
 * ovxr_set_managed_anchor_transform, ovxr_clear_anchor, ovxr_destroy) takes a
 * stage_ordinal from the caller, writes everything it
 * has to write at that ordinal, and advances the write floor to it exactly
 * once, when the call succeeds. OVXR never interprets the value.
 *
 * Contract for a nonzero stage_ordinal:
 *   - It must be above the current write floor, and exclusively OVXR's to
 *     finish: no other writer may still be writing at it, and every ordinal
 *     below it must already be resolved (published or abandoned), because
 *     sealing it seals everything below it. Physics-driven: pass a fresh
 *     ordinal. Render-driven: pass the ordinal your simulation wrote this step
 *     at, and OVXR's seal publishes both.
 *   - On success the ordinal is sealed; mark it resolved in your allocator.
 *   - On failure, do not assume it was not sealed. ovxr_bind_frame and
 *     ovxr_render_frame seal BEFORE the renderer work, and the floor read, the
 *     OVRTX update, the step enqueue and the step wait can all fail after that;
 *     they report what happened through out_stage_ordinal_sealed, which is why
 *     that argument is mandatory for a nonzero ordinal. Read the
 *     report, and when it is true mark the ordinal resolved and continue from a
 *     fresh one -- the sealed one is at or below the floor and will be rejected.
 *     ovxr_create and ovxr_destroy do not have this problem: ovxr_create writes
 *     nothing to the Stage at all, and ovxr_destroy seals as its last act and
 *     returns exactly that outcome, so a failure from either leaves the ordinal
 *     untouched. The one carve-out is private mode (stage_ordinal 0), where
 *     ovxr_destroy can still fail after its own overlay release sealed; that
 *     ordinal is OVXR's own, so the caller has nothing to reconcile.
 *   - On failure OVXR undoes its writes at the ordinal where it can, but not
 *     everywhere, and it never deletes an attribute it found already authored.
 *     ovxr_destroy seals only if the release succeeded;
 *     ovxr_update_prim_transforms, ovxr_update_camera_transforms and
 *     ovxr_update_render_product_resolutions restore the previous values of the
 *     attributes they touched. The frame path's per-frame camera state is NOT
 *     restored: it is rewritten on the next frame, so a partial batch is left
 *     in place to be superseded rather than repaired -- OVXR does not pay a
 *     read per frame for an undo nothing observes.
 *   - An unpublished write is still readable. Sealing controls ordering, not
 *     visibility, so an abandoned ordinal is not self-cleaning and the rollback
 *     above is what removes a partial batch, not the abandonment.
 *   - Unless it was sealed, the ordinal is yours to retry, finish, or abandon.
 *   - A stage_ordinal of 0 selects OVXR's private ledger instead: OVXR mints
 *     the next ordinal past the floor and seals it itself. That is correct
 *     only while OVXR is the sole OVStage writer. ordinal_mode fixes this
 *     choice for the context lifetime. APPLICATION rejects 0/private writes;
 *     PRIVATE rejects caller-supplied ordinals.
 *
 * Ordinal admission policy. OVStage applies one admission policy per process,
 * selected by OVSTAGE_CONFIG_RUNTIME_DEFAULT_ORDINAL_ADMISSION_POLICY and
 * fixed by the first process reference (ovstage_initialize or the first
 * ovstage_create_instance). OVXR supports NONDECREASING, the default, and
 * FLOOR_ONLY. OVXR 0.1 does not support STRICTLY_INCREASING: OVXR writes some
 * cells more than once at a single ordinal (the first renderable
 * ovxr_bind_frame writes identity eye-camera transforms and then the frame's,
 * and failure undo and a retry rewrite at the failed ordinal), and that policy
 * rejects every such rewrite. Keep the default, or select NONDECREASING
 * explicitly before anything else in the process references OVStage.
 * ovxr_create fails when the stage's process selected STRICTLY_INCREASING.
 * Support for STRICTLY_INCREASING is planned for OVXR 0.2.
 */
typedef struct ovxr_desc_t
{
    /* Borrowed. The application owns creation, configuration, and destruction. */
    ovrtx_renderer_t* ovrtx_renderer;

    /*
     * Required borrowed OVStage scene representation. OVXR reads scene metadata
     * and anchors from this stage. The caller owns the stage lifetime and is
     * responsible for serializing application-authored OVStage write ordinals
     * with OVXR calls that update or initialize OVStage state. OVXR advances the
     * stage-wide write floor for its own authored state.
     * The stage's process must use the NONDECREASING (default) or FLOOR_ONLY
     * ordinal admission policy; ovxr_create rejects a stage whose process
     * selected STRICTLY_INCREASING (see "OVStage ordinals").
     */
    ovstage_instance_t* ovstage;

    const char* application_name; /* Default: "OVXR" */

    /*
     * Absolute prim path of the runtime namespace. Every prim OVXR invents —
     * default render products, render vars, and generated eye cameras — is
     * authored under this root, and OVXR authors scene description nowhere
     * else. The subtree is library-owned: OVXR may rewrite or
     * schema-complete anything under it, so applications must not author
     * their own prims inside it. Prims whose paths the application supplies
     * (camera_paths, render_products, render_settings) are used where they
     * are and never relocated into the namespace. The stage-global
     * RenderSettings prim is a stage construct at its conventional path:
     * OVXR upserts its prim type and late-binds only its `products`
     * relationship.
     * OVXR also creates <runtime_prim_root>/Anchor, the default tracking
     * anchor prim, which applications may re-author (see the runtime anchor
     * API below).
     * Must name a single absolute top-level prim — for example "/OVXR", not
     * "/OVXR/", "OVXR", or "/World/XR". Anything else is rejected at
     * ovxr_create().
     * Default: "/OVXR"
     */
    const char* runtime_prim_root;

    /* Optional settings overrides. Default: none. */
    const ovxr_settings_t* settings;

    /* Default: 1 frame in flight. */
    uint32_t max_frames_in_flight;
    /*
     * Reserved render-GPU limit; defaults to 1 but currently does not constrain
     * ovRTX placement. Query ovxr_get_device_info() for observed GPU usage.
     */
    uint32_t max_render_gpus;

    /* Default: false. */
    bool enable_debug_labels;

    /*
     * Optional render products. With create_render_products disabled, these
     * paths must name existing prepared RenderProduct prims; OVXR authors only
     * its required per-product eco-mode override on them. With
     * create_render_products enabled, they are the target paths OVXR authors;
     * however, if camera_path_count is zero, OVXR still reads cameras from these
     * products while configuring the context, so the products must already exist. Provide
     * camera_paths when asking OVXR to author new custom product paths.
     * The count must match the selected display mode: one for mono, two for
     * stereo, and four for quad view, in OpenXR view order. AUTO expects stereo
     * ordering and uses the first entry if it must fall back to mono.
     */
    const ovx_string_t* render_products;
    size_t render_product_count;

    /*
     * OpenXR view-driven camera fields. ovxr_begin_frame() waits/begins/
     * locates the OpenXR frame and caches its view state; ovxr_bind_frame()
     * publishes these cameras to the borrowed OVStage at its stage_ordinal,
     * and ovxr_submit_frame() submits using the same views.
     * tracking_anchor_path may name an existing source camera/prim whose
     * latest committed OVStage world transform is used as the tracking anchor
     * at every frame boundary. When omitted or missing, OVXR follows its
     * managed prim <runtime_prim_root>/Anchor instead, initialized from the
     * first configured camera's pre-existing pose. An existing prim with an
     * invalid world transform fails anchor initialization rather than falling
     * back. See the runtime anchor API below.
     * Defaults: <runtime_prim_root>/Cameras/LeftEye and, unless mono,
     * <runtime_prim_root>/Cameras/RightEye. Quad view appends
     * <runtime_prim_root>/Cameras/LeftInset and
     * <runtime_prim_root>/Cameras/RightInset. meters_per_unit = 0.01.
     */
    const ovx_string_t* camera_paths;
    size_t camera_path_count;
    float meters_per_unit;

    /*
     * Runtime render-product authoring. When enabled, OVXR authors the
     * configured/default render products through the borrowed OVStage after
     * querying the selected OpenXR view configuration. Quad view authors each
     * product at that view's recommended size. OVXR clears inherited deviceIds
     * on stereo and quad-view clones so OVRTX can assign them automatically;
     * prepared products retain their authored deviceIds. A mono product is pinned
     * to the OpenXR composition GPU, which must be active in OVRTX. Disabled local
     * foveation overrides any foveation inherited from source_render_product.
     *
     * Current contract:
     * - Runtime RenderProduct authoring is initialization-only; changing
     *   RenderProducts or renderer settings through OVStage after this step is
     *   not a supported invalidation path.
     * - OVXR does not change the process-global RTX eco-mode setting. OVXR
     *   disables eco mode directly on every configured RenderProduct before it
     *   is used for submission. For prepared products supplied with
     *   create_render_products disabled, this write occurs during the first
     *   renderable ovxr_bind_frame().
     *   Required color and depth AOVs therefore remain available on every frame.
     * - AR passthrough compositing (omni:rtx:post:compositing:* and the related
     *   refraction/cutout-opacity settings) is authored only on RenderProducts
     *   OVXR creates. With create_render_products disabled, an application
     *   supplying its own prepared products in an AR environment blend mode is
     *   responsible for authoring these settings itself; OVXR's prepared-product
     *   path only writes the eco-mode override above.
     * - Authoring is transactional at the attribute-value level: a failed
     *   first bind rolls back the attribute values it wrote on prims it created
     *   (keeping an "ovxr:runtimeAuthored" ownership marker), restores a
     *   pre-existing RenderSettings products relationship it overwrote, and
     *   never publishes the failed ordinal — so no partially customized
     *   values become readable. Complete artifacts a failed run may leave
     *   behind (fallback-camera prims, cloned product copies, empty prim
     *   entries carrying only the marker) are adopted by a retry: calling
     *   binding again with the same context succeeds without
     *   recreating or manually cleaning the stage and never duplicates
     *   references, prims, relationships, or ordinal records. An existing
     *   target WITHOUT the ownership marker remains a hard conflict.
     * - Every write lands at the ordinal the caller passes (see
     *   "OVStage ordinals" above): initialization and per-frame state both use
     *   the ordinal passed to ovxr_bind_frame. OVXR never derives an ordinal from the write floor
     *   when the caller supplies one, so an application that also writes the
     *   OVStage after ovxr_create writes concurrently without ordinal
     *   collisions and without either side publishing the other's incomplete
     *   writes. Writes made before ovxr_create are the application's to
     *   serialize, as before.
     *
     * Defaults:
     * - render products: <runtime_prim_root>/Render/Products/Left for mono,
     *   {Left,Right} for stereo, plus {LeftInset,RightInset} for quad view
     * - cameras: the corresponding one, two, or four default camera paths
     * - render_vars: <runtime_prim_root>/Render/Vars/LdrColor, plus
     *   <runtime_prim_root>/Render/Vars/XRDepth when depth submission is
     *   supported by OpenXR and not disabled. Foveated products are always
     *   rendered warped; OVXR unwarps colour and depth in CUDA when the OpenXR
     *   runtime expects an unwarped submission, and unwarps the readback
     *   capture when readback_foveation_unwarp is set.
     * - render_settings: /Render/OmniverseGlobalRenderSettings
     * - source_render_product: empty, so products are defined directly
     * - foveation_warped_resolution_multiplier: 0.5
     * - foveation_warped_inset_size: 0.4
     */
    bool create_render_products;
    const ovx_string_t* render_vars;
    size_t render_var_count;
    ovx_string_t source_render_product;
    ovx_string_t render_settings;
    /* Legacy-fallback foveation for OVXR-authored products: submit unwarped images (OVXR
       unwarps the warped render in CUDA at submit). Runtime PQW overrides this choice. */
    bool foveation_unwarp;
    int32_t foveation_unwarped_width;
    int32_t foveation_unwarped_height;
    float foveation_warped_resolution_multiplier;
    float foveation_warped_inset_size;

    /*
     * Optional source camera/prim for the tracking-to-world anchor. The path
     * must be absolute, contain no embedded NUL, and must not name an eye
     * camera driven by OVXR.
     */
    ovx_string_t tracking_anchor_path;

    /* Stage up axis for mapping OpenXR's +Y-up tracking poses into the USD stage. */
    ovxr_stage_up_axis_t up_axis;

    /*
     * Additional OpenXR instance extensions the application wants enabled.
     *
     * OVXR enables only the extensions it needs itself (Vulkan, and optionally
     * composition-layer-depth). It deliberately does not assume which
     * extensions a consuming application requires. When an application shares
     * OVXR's OpenXR handles with another stack -- e.g. IsaacTeleop's DeviceIO,
     * which resolves input/timing entry points (hand tracking, timespec
     * conversion, CloudXR action/xdev/data-channel functions) via
     * xrGetInstanceProcAddr against this instance -- those extensions must be
     * enabled at xrCreateInstance or the lookups fail with
     * XR_ERROR_FUNCTION_UNSUPPORTED. The application declares them here; OVXR
     * does not call these entry points itself, it only enables them. Each name
     * is enabled when the runtime advertises it and skipped (with a warning)
     * otherwise. This mirrors how the Kit XR path lets an application supply
     * its required extensions when the session is created.
     */
    const ovx_string_t* extra_openxr_extensions;
    size_t extra_openxr_extension_count;

    /*
     * Eye-camera near/far clip planes in meters. These are the single source of
     * truth for both the eye-camera render clip range and the
     * XrCompositionLayerDepthInfoKHR planes declared to CloudXR, so the two always
     * agree. They clip render geometry: anything nearer than depth_near_z or
     * farther than depth_far_z is clipped out of the color/depth render.
     *
     * These planes do NOT govern streamed-depth precision. CloudXR reconstructs
     * metric distance from the (reverse, 32-bit float) XRDepth and re-encodes it
     * into its own client-fixed depth range, so a wider range here does not crush
     * the transmitted depth -- it only widens what geometry is rendered.
     *
     * Leave both fields 0 (the zero-initialized state) to keep the wide VR-sane
     * default: near = 0.01 m, far = 10000 m,
     * which avoids clipping large stages. To override, set BOTH to a positive,
     * finite pair with near < far. Any other non-zero input -- a value <= 0, a
     * non-finite value, near >= far, or only one of the two set -- causes
     * ovxr_create to fail with OVXR_ERROR rather than silently falling back to the
     * default. Tighten the range for room/workcell-scale scenes (e.g. teleop) if
     * you want closer/farther geometry clipped explicitly.
     */
    float depth_near_z;
    float depth_far_z;

    /*
     * Controls where OVXR resolves scene scale and up-axis from.
     * AUTO: OVStage metadata when available, then descriptor, then defaults.
     *       OVStage metadata therefore overrides desc.meters_per_unit/up_axis.
     * FROM_DESC: descriptor/default metadata; OVStage remains required.
     * FROM_OVSTAGE: missing OVStage metadata is an error.
     */
    ovxr_stage_scene_info_policy_t scene_info_policy;

    /*
     * Optional render-product color capture. When true, OVXR copies the color
     * output selected by readback_render_product_index into an internal CUDA
     * buffer after each successfully submitted frame, on both the high-level
     * ovxr_render_frame() path and the split ovxr_begin_frame()/
     * ovxr_submit_frame() path. The application retrieves the latest capture
     * with ovxr_acquire_frame() and owns any subsequent processing or
     * transport. Capture is best-effort and never changes the frame result.
     * By default the captured image is the color selected for OpenXR submission.
     * Set readback_foveation_unwarp to request an unwarped capture independently
     * when OpenXR submission remains warped.
     */
    bool enable_frame_readback;

    /*
     * Zero-based index into the configured render_products array whose color
     * output is captured. Defaults to 0 and must identify a configured product.
     * AUTO uses index 0 after falling back to mono.
     */
    uint32_t readback_render_product_index;

    /*
     * When true with enable_frame_readback and runtime-created foveated render
     * products, OVXR unwarps the captured colour in CUDA to the unwarped
     * resolution even when OpenXR submission uses the warped image, so the
     * acquired frame has the unwarped size. This does not change the OpenXR
     * submit mode or the render products. When foveation is disabled, the
     * capture is the submitted colour at its native size.
     */
    bool readback_foveation_unwarp;

    /* Selects who owns OVStage ordinals for every Stage-writing call. */
    ovxr_stage_ordinal_mode_t ordinal_mode;

    /*
     * Default false: ovxr_bind_frame() runs ovrtx_update_from_stage() and
     * ovrtx_step_with_stage() itself and OVXR owns the step result.
     *
     * True selects application-stepped mode. ovxr_bind_frame() still publishes
     * the frame's Stage writes at the caller's ordinal, seals it and reads the
     * live floor into plan->render_ordinal, but does not touch the renderer.
     * The application then runs ovrtx_update_from_stage() and
     * ovrtx_step_with_stage() at plan->render_ordinal over
     * plan->render_products, under its own publication lock, and passes the
     * step result in ovxr_submit_desc_t::step_result; OVXR reads it for
     * submission and never destroys it. The mode requires an explicit
     * ovxr_bind_frame() (the implicit bind in ovxr_submit_frame() and
     * ovxr_render_frame() are rejected) and gives up bind's guarantee that the
     * renderer's borrowed Stage reads completed inside its call. Intended for
     * applications that must observe or own the OVRTX step directly.
     */
    bool application_steps_renderer;
} ovxr_desc_t;

typedef struct ovxr_scene_info_t
{
    float meters_per_unit;
    ovxr_scene_info_source_t meters_per_unit_source;
    ovxr_stage_up_axis_t up_axis;
    ovxr_scene_info_source_t up_axis_source;
} ovxr_scene_info_t;

typedef struct ovxr_runtime_features_t
{
    ovxr_display_mode_t display_mode;
    ovxr_graphics_api_t graphics_api;
    ovxr_session_mode_t session_mode;
    ovxr_environment_blend_mode_t environment_blend_mode;
    /* Effective local mode. When configured as AUTO, this can change from DISABLED
       to WARPED after a frame locates valid runtime PQW data; query again after that
       frame. Quad-view AUTO remains DISABLED. */
    ovxr_local_foveation_mode_t local_foveation_mode;

    bool ar_blend_enabled;
    bool environment_blend_opaque_supported;
    bool environment_blend_alpha_supported;
    bool environment_blend_additive_supported;

    bool depth_submission_enabled;
    bool foveated_submission_enabled;
    bool local_foveated_rendering_enabled;
} ovxr_runtime_features_t;

typedef struct ovxr_gpu_identity_t
{
    bool has_device_luid;
    uint8_t device_luid[8];
    uint32_t device_node_mask;
    uint8_t device_uuid[16];
    uint8_t driver_uuid[16];

    int32_t cuda_device_id;
} ovxr_gpu_identity_t;

typedef struct ovxr_device_info_t
{
    /* CUDA device used by the OpenXR compositor. */
    ovxr_gpu_identity_t openxr_composition_gpu;
    /* CUDA device backing the first configured render product's color output. */
    ovxr_gpu_identity_t primary_ovrtx_render_gpu;

    /* True when every observed render output is on the composition GPU. */
    bool same_gpu_fast_path;
    /* True when at least one observed render output requires a cross-GPU transfer. */
    bool cross_gpu_required;
    /*
     * True when peer access is available for every required cross-GPU transfer.
     * False is not meaningful when cross_gpu_required is false.
     */
    bool cuda_peer_transfer_available;
    /* True when configured render products' color outputs span multiple GPUs. */
    bool stereo_split_enabled;
    /* Unique GPUs observed across submitted color and depth outputs. */
    uint32_t render_gpu_count;

    ovxr_runtime_features_t features;
} ovxr_device_info_t;

typedef struct ovxr_frame_timing_t
{
    uint64_t frame_index;
    int64_t predicted_display_time_ns;
    /*
     * OpenXR predictedDisplayPeriod converted to seconds when OpenXR is active;
     * otherwise a conservative fallback timestep.
     */
    double suggested_delta_time_seconds;
    bool should_render;
} ovxr_frame_timing_t;

typedef struct ovxr_frame_plan_t
{
    ovxr_frame_t* frame;
    ovxr_frame_timing_t timing;

    /*
     * The RenderProducts the frame's OVRTX step renders: OVXR's eye products
     * followed by the application products selected with
     * ovxr_set_frame_render_products(). Owned by OVXR. The array remains valid
     * until ovxr_submit_frame() or ovxr_cancel_frame() consumes this plan. The
     * consuming call clears this view before returning.
     */
    ovrtx_render_product_set_t render_products;

    /*
     * The OVRTX step result of the bound step. Zero until a renderable
     * ovxr_bind_frame() succeeds, and for frames the runtime asked not to
     * render. Borrowed: OVXR owns it and destroys it in ovxr_submit_frame() /
     * ovxr_cancel_frame(), which also clear this field. The step operation has
     * completed when bind returns, so ovrtx_fetch_results() and
     * ovrtx_map_render_var_output() may be used on it directly -- this is how an
     * application reads the outputs of its own render products. Every mapping
     * must be unmapped before the consuming call. Never pass it to
     * ovrtx_destroy_results(). Always zero in application-stepped mode
     * (ovxr_desc_t::application_steps_renderer), where the application owns
     * the step result it created.
     */
    ovrtx_step_result_handle_t step_result;

    /*
     * The committed OVStage ordinal the frame renders at: the write floor
     * ovxr_bind_frame() observed after sealing the frame's own ordinal. Zero
     * until a renderable bind succeeds. In application-stepped mode this is
     * the ordinal to pass to ovrtx_update_from_stage() and
     * ovrtx_step_with_stage(); otherwise it is informational.
     */
    ovstage_ordinal_t render_ordinal;
} ovxr_frame_plan_t;

/* Forward declaration keeps the public C header independent of the OpenXR SDK. */
struct XrCompositionLayerBaseHeader;

/*
 * Optional per-submit inputs for ovxr_submit_frame(). Zero-initialize before
 * assigning fields; passing NULL submits the OVRTX scene only.
 */
typedef struct ovxr_submit_desc_t
{
    /*
     * Application-stepped mode only (ovxr_desc_t::application_steps_renderer):
     * the ovrtx_step_with_stage() result for this frame, rendered at
     * plan->render_ordinal over plan->render_products. Required for a
     * renderable frame in that mode; OVXR reads it and the application waits
     * for and destroys it after submit returns. Must be 0 otherwise -- OVXR
     * owns the bound step result and a nonzero value is rejected.
     */
    ovrtx_step_result_handle_t step_result;
    /*
     * Borrowed application-owned OpenXR composition layers (projection, quad,
     * or enabled extension layer types), appended after the OVXR scene layer in
     * array order, so later entries composite above earlier ones.
     */
    const struct XrCompositionLayerBaseHeader* const* additional_layers;
    /* Zero submits only the scene. A nonzero count requires additional_layers. */
    uint32_t additional_layer_count;
} ovxr_submit_desc_t;

typedef struct ovxr_frame_result_t
{
    uint64_t frame_index;
    bool submitted;
    bool used_cross_gpu_transfer;
    uint32_t source_gpu_count;
    ovxr_environment_blend_mode_t environment_blend_mode;
    char openxr_submit_message[256];
} ovxr_frame_result_t;

typedef struct ovxr_camera_transform_t
{
    const char* camera_path;
    /*
     * Row-major 4x4 WORLD transform. Translation is matrix[12..14].
     *
     * World-space, not parent-relative: OVXR authors the camera so it opts out of
     * its ancestors' transform stack, so a non-identity parent does not offset the
     * result. This is required for the pose to survive the renderer's per-frame
     * world-transform resolution, which otherwise recomputes it.
     */
    double matrix[16];
} ovxr_camera_transform_t;

/*
 * One prim's world transform for ovxr_update_prim_transforms. Identical
 * semantics to ovxr_camera_transform_t (row-major 4x4 WORLD transform,
 * translation in matrix[12..14], authored to opt out of the ancestors'
 * transform stack); the separate type exists because the general API is not
 * camera-specific.
 */
typedef struct ovxr_prim_transform_t
{
    const char* prim_path;
    double matrix[16];
} ovxr_prim_transform_t;

/*
 * Absolute tracking anchor transform in scene space.
 *
 * `matrix` is row-major and uses row vectors; translation is matrix[12..14]
 * in stage units. It may contain translation, arbitrary rotation, and a
 * positive uniform scale. OVXR rejects NaN/Inf, perspective, shear,
 * reflection, non-uniform scale, and zero scale.
 *
 * Let U convert meters to stage units, B convert OpenXR's +Y-up basis to the
 * configured stage basis, and A be this transform. An OpenXR pose P maps as:
 *
 *     P_scene = P * U * B * A
 *
 * The anchor is always a prim. A supplies its latest committed OVStage world
 * transform at every frame boundary. When no application prim is selected the
 * source is the OVXR-managed prim <runtime_prim_root>/Anchor, created with the
 * runtime RenderProducts (or at the first frame boundary when OVXR does not
 * author them) and initialized from the first eye camera's pre-existing pose
 * (identity for runtime-authored cameras). Calling
 * ovxr_set_managed_anchor_transform(), or re-authoring that prim through
 * OVStage, places or teleports the user without configuring another anchor
 * prim.
 *
 * By default OVXR floor-aligns A: translation and heading about the stage up
 * axis are preserved while pitch, roll, and authored scale are discarded. The
 * Camera prims derive heading from local -Z (USD camera forward), while
 * stage-space Xforms use the stage-space forward axis. OVXR selects this
 * convention from stable prim metadata when the source is selected and keeps
 * it while that prim animates, avoiding orientation-dependent heading snaps.
 *
 * Set OVXR_SETTING_FLOOR_ALIGNED_ANCHOR to 0 to apply A verbatim, preserving
 * its complete validated rotation and positive uniform scale. A is then a
 * stage-space transform composed after the basis change B, so it must be
 * authored in that convention: the managed prim and application Xform rigs
 * qualify, but a Camera prim in a Z-up stage carries its own rotateX(90) and
 * would pitch the view 90 degrees. Use floor alignment with Camera anchors.
 */
typedef struct ovxr_anchor_transform_t
{
    double matrix[16];
} ovxr_anchor_transform_t;

/* OVXR-owned equivalents of the position, orientation, and pose fields used by OpenXR. */
typedef struct ovxr_quaternion_t
{
    float x;
    float y;
    float z;
    float w;
} ovxr_quaternion_t;

typedef struct ovxr_vector3_t
{
    float x;
    float y;
    float z;
} ovxr_vector3_t;

typedef struct ovxr_pose_t
{
    ovxr_quaternion_t orientation;
    ovxr_vector3_t position;
} ovxr_pose_t;

typedef struct ovxr_render_product_resolution_t
{
    const char* render_product_path;
    int32_t width;
    int32_t height;
} ovxr_render_product_resolution_t;

typedef struct ovxr_openxr_probe_result_t
{
    bool instance_created;
    bool system_found;
    bool view_config_enumerated;
    bool vulkan_enable_available;
    bool vulkan_enable2_available;
    bool vulkan_requirements_queried;
    bool d3d12_enable_available;
    bool d3d12_requirements_queried;
    bool composition_gpu_queried;
    bool session_created;

    uint32_t view_count;
    uint32_t recommended_swapchain_width;
    uint32_t recommended_swapchain_height;
    uint32_t recommended_swapchain_sample_count;
    uint32_t max_swapchain_width;
    uint32_t max_swapchain_height;
    bool recommended_swapchain_size_uniform;
    bool environment_blend_opaque_supported;
    bool environment_blend_alpha_supported;
    bool environment_blend_additive_supported;
    int32_t last_xr_result;
    ovxr_environment_blend_mode_t environment_blend_mode;
    uint64_t system_id;
    uint64_t runtime_version;
    uint64_t min_vulkan_api_version_supported;
    uint64_t max_vulkan_api_version_supported;
    uint32_t d3d12_min_feature_level;
    ovxr_graphics_api_t graphics_api;

    ovxr_gpu_identity_t composition_gpu;

    char runtime_name[128];
    char system_name[256];
    char vulkan_physical_device_name[256];
    char d3d12_adapter_name[256];
    char message[256];
} ovxr_openxr_probe_result_t;

typedef struct ovxr_openxr_frame_result_t
{
    bool session_created;
    bool session_running;
    bool spaces_created;
    bool swapchains_created;
    bool frame_waited;
    bool frame_begun;
    bool views_located;
    bool swapchains_cleared;
    bool frame_ended;
    bool submitted_projection_layer;
    bool should_render;

    uint32_t view_count;
    uint32_t swapchain_count;
    uint32_t swapchain_width;
    uint32_t swapchain_height;
    int32_t session_state;
    int32_t last_xr_result;
    int64_t swapchain_format;
    int64_t predicted_display_time;
    ovxr_environment_blend_mode_t environment_blend_mode;

    char message[256];
} ovxr_openxr_frame_result_t;

/*
 * External-runtime handle handoff. Surfaces the OpenXR handles OVXR created so a
 * separate consumer (e.g. IsaacTeleop) can drive input on the SAME OpenXR session
 * instead of creating its own. Values are raw handle/function-pointer bits as
 * uint64 (cast back to XrInstance/XrSession/XrSpace/PFN_xrGetInstanceProcAddr by
 * the consumer). Valid only after the OpenXR session has been created (i.e. after
 * the first ovxr_begin_frame / ovxr_submit_openxr_blank_frame); returns
 * OVXR_NOT_READY before then.
 *
 * Ownership contract: OVXR remains the OWNER of the shared session. It alone
 * drives the frame loop (xrWaitFrame/xrBeginFrame/xrEndFrame via
 * ovxr_begin_frame/ovxr_submit_frame) and owns the session lifecycle; the
 * handles stay valid only until ovxr_destroy. The consumer may create its own
 * action sets, spaces, and swapchains (destroy them before ovxr_destroy), read input,
 * locate spaces, and resolve entry points against the shared instance, but must
 * NOT begin/end frames, change the session state, or destroy these handles.
 * The handles are not synchronized for concurrent use across threads; treat
 * cross-stack access as belonging to OVXR's render-thread cadence.
 *
 * xr_reference_space_type identifies the reference-space kind xr_space was created
 * with. OVXR prefers STAGE, then LOCAL_FLOOR, then LOCAL. For STAGE and
 * LOCAL_FLOOR, tracking y = 0 is the physical floor, which an application
 * needs when deriving a standing position from a head pose; for LOCAL the
 * origin is the runtime's start pose and no floor is implied.
 */
typedef struct ovxr_openxr_handles_t
{
    uint64_t xr_instance;
    uint64_t xr_session;
    uint64_t xr_space;
    uint64_t xr_get_instance_proc_addr;
    ovxr_reference_space_type_t xr_reference_space_type;
} ovxr_openxr_handles_t;

/*
 * Borrowed Vulkan graphics binding of the OpenXR SESSION (not an OVRTX device).
 * Cast the uint64 handle bits to VkInstance/VkPhysicalDevice/VkDevice/VkQueue.
 * The application may create its own resources on this device, including
 * importing external images from a separate Vulkan context on the same GPU.
 * Never destroy the borrowed objects. Destroy application-owned imports,
 * command pools, and OpenXR swapchains before ovxr_destroy().
 *
 * All access to vk_queue, including submit/wait/present, must be serialized
 * with OVXR calls and other consumers of the shared queue. OVXR does not lock
 * the queue on the application's behalf. Use the returned queue family/index;
 * do not assume additional queues or device features were enabled.
 *
 * The handle-type masks use VkExternalMemoryHandleTypeFlagBits and
 * VkExternalSemaphoreHandleTypeFlagBits. A nonzero mask means the relevant
 * platform extension is enabled, not that every format/usage is supported.
 * Query external image/semaphore properties before importing. The application
 * owns GPU synchronization, external queue-family ownership transfers, and OS
 * handle lifetime; it must finish writes before releasing a swapchain image.
 */
typedef struct ovxr_vulkan_handles_t
{
    uint64_t vk_instance;
    uint64_t vk_physical_device;
    uint64_t vk_device;
    uint64_t vk_queue;
    uint32_t queue_family_index;
    uint32_t queue_index;
    uint32_t external_memory_handle_types;
    uint32_t external_semaphore_handle_types;
} ovxr_vulkan_handles_t;

/*
 * Pixel format of a frame returned by ovxr_acquire_frame(). OVRTX emits the
 * selected color output as RGBA8, so OVXR hands it back unchanged.
 */
typedef enum ovxr_frame_format_t
{
    /* 4x8-bit unsigned, R,G,B,A byte order. */
    OVXR_FRAME_FORMAT_RGBA8_UNORM = 0,
} ovxr_frame_format_t;

/*
 * A view onto the most recently captured render-product color frame (see
 * ovxr_desc_t::enable_frame_readback and ovxr_acquire_frame).
 */
typedef struct ovxr_frame_view_t
{
    /*
     * Device pointer to a tightly packed (pitch_bytes = width*4) image on
     * cuda_device_id, owned by OVXR. The copy is complete and host-synchronized
     * before this view is published, so the contents are safe to read from any
     * thread or stream. The application must not free or write the buffer. OVXR
     * double-buffers it, so the pointer remains valid until capture of sequence
     * + 2 begins, and only while the context lives. Copy or consume it before
     * allowing two more frames to be submitted.
     */
    const void* device_ptr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch_bytes;
    int32_t cuda_device_id;
    ovxr_frame_format_t format;

    /*
     * Monotonic capture counter (1-based). A repeated value means capture failed
     * or no newer frame is available; the last completed view remains usable.
     */
    uint64_t sequence;

    /*
     * OpenXR predicted display time of the frame this capture came from (the
     * ovxr_frame_timing_t::predicted_display_time_ns of the producing
     * ovxr_render_frame() call), for correlating the image with poses or other
     * per-frame data. 0 when the frame had no OpenXR timing.
     */
    int64_t predicted_display_time_ns;
} ovxr_frame_view_t;

/*
 * Threading contract:
 * - Calls using the same ovxr_context_t must not overlap unless an entry point
 *   below explicitly permits it. They may run on different threads when the
 *   caller serializes them.
 * - ovxr_acquire_frame() may overlap other calls on the same context.
 * - ovxr_request_exit() may overlap other calls on the same context except
 *   ovxr_destroy(). It only requests the runtime transition; session-state
 *   pumping, xrEndSession, and destruction remain on the owning frame thread.
 * - ovxr_destroy() is exclusive: no call may be in progress and no new call
 *   may begin once destruction starts.
 * - Different contexts are independent, but callers remain responsible for
 *   synchronizing any borrowed renderer or OVStage shared between them.
 */

/*
 * Creates an OVXR context over the borrowed renderer and required OVStage.
 * Threading: may be called from any thread; the caller must exclusively own the
 * borrowed renderer and OVStage for the duration of the call.
 * Creation performs no OVStage writes. Render-product/settings authoring and
 * tracking-anchor initialization are deferred to the first renderable
 * ovxr_bind_frame(), which is the first publication boundary.
 * Fails with OVXR_ERROR when the stage's process selected the unsupported
 * STRICTLY_INCREASING ordinal admission policy (see "OVStage ordinals").
 */
OVXR_API ovxr_result_t ovxr_create(const ovxr_desc_t* desc, ovxr_context_t** out_context);

/*
 * Asynchronously asks the OpenXR runtime to exit a running session. This is a
 * cooperative wake for an owner thread blocked in the frame lifecycle; it does
 * not end or destroy the session. A missing, already-stopped, or previously
 * requested session is a successful no-op. ovxr_destroy() subsequently pumps
 * session events to STOPPING and calls xrEndSession on the owning thread before
 * destroying native resources.
 *
 * Threading: may overlap other calls on the same context except ovxr_destroy().
 * Calls using the same context are idempotent and internally serialized.
 */
OVXR_API ovxr_result_t ovxr_request_exit(ovxr_context_t* context);

/*
 * Destroys the context. OVXR's runtime overlay is released from the surviving
 * stage at stage_ordinal (see "OVStage ordinals"; 0 = private ledger). The
 * context is always destroyed.
 *
 * The return value describes the Stage and nothing else: OVXR_SUCCESS means the
 * final overlay release was published and stage_ordinal was sealed, so mark it
 * resolved. A failure means the release did not publish, stage_ordinal was NOT
 * sealed, and ovxr_get_last_error() describes that release failure. Native
 * teardown problems -- an OpenXR session that never reached STOPPING, for
 * instance -- are logged by ovxr_destroy rather than returned, because they say
 * nothing about the ordinal the caller has to reconcile. Destruction of the
 * context, its OpenXR objects and its GPU resources always completes either
 * way. If the diagnostic reports unresolved Stage writes, the borrowed Stage
 * must not be published or reused: an earlier rollback failed, so advancing its
 * floor could expose a partial transaction.
 * Threading: exclusive; see the threading contract above.
 */
OVXR_API ovxr_result_t ovxr_destroy(ovxr_context_t* context, ovstage_ordinal_t stage_ordinal);

/*
 * Fill out_handles with the OpenXR instance/session/reference-space handles
 * and the xrGetInstanceProcAddr entrypoint OVXR is using, as uint64 values.
 * Returns OVXR_NOT_READY if the session has not been created yet.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_get_openxr_handles(ovxr_context_t* context, ovxr_openxr_handles_t* out_handles);

/*
 * Returns OVXR_NOT_READY until the Vulkan OpenXR session exists, or OVXR_ERROR
 * when the context uses another graphics API. Clears out_handles on failure.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_get_vulkan_handles(ovxr_context_t* context, ovxr_vulkan_handles_t* out_handles);

/*
 * Returns OVXR_NOT_READY until the OpenXR and ovRTX render GPU topology has
 * been observed. On OVXR_NOT_READY, features is valid, both CUDA device ids are
 * -1, and the remaining topology fields are zero. After the first successful
 * topology query, OVXR returns the last successfully resolved topology until a
 * changed GPU set is resolved successfully.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_get_device_info(ovxr_context_t* context, ovxr_device_info_t* out_info);

/* Threading: serialize with all non-acquire calls on the same context. */
OVXR_API ovxr_result_t ovxr_get_scene_info(ovxr_context_t* context, ovxr_scene_info_t* out_info);

/*
 * Select application-owned RenderProducts that OVXR includes, after its own
 * eye products and in array order, in every bound OVRTX step until replaced.
 * Rendering them in the frame's single step keeps their sensor history and
 * simulation clock aligned with the eye products; a separate application step
 * would discard the history of every product not in its set. Read their
 * outputs from plan->step_result between ovxr_bind_frame() and
 * ovxr_submit_frame(); OVXR never submits or interprets them.
 *
 * Replaces the previous selection; a count of zero clears it. Each entry must
 * be an absolute prim path that names a RenderProduct on the borrowed OVStage
 * by the time the next frame binds (OVRTX rejects an unknown product at that
 * step), and must not repeat an OVXR eye product or another entry. OVXR copies
 * the strings. This call writes nothing to the Stage, so it takes no ordinal,
 * and OVXR authors nothing on these prims: an application that needs an output
 * every frame disables eco mode on its product itself
 * (`omni:rtx:rt:ecoMode:enabled`). Rejected with OVXR_ERROR while any frame
 * plan is active, because bind hands the composed set to the plan as a
 * borrowed view. A product that leaves the set loses its accumulated render
 * history (OVRTX contract); products that stay keep it.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_set_frame_render_products(
    ovxr_context_t* context,
    const ovx_string_t* render_products,
    size_t render_product_count);

/*
 * Begin a frame and return its plan. This waits/begins/locates OpenXR but does
 * not write or read mutable OVStage state. out_plan is cleared before any
 * return. On success, it owns an active frame that must next be passed to
 * ovxr_bind_frame() inside the application's Stage-publication rendezvous,
 * then to ovxr_submit_frame(), or be passed to ovxr_cancel_frame().
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_begin_frame(ovxr_context_t* context, ovxr_frame_plan_t* out_plan);

/*
 * Publish the OpenXR view state cached by ovxr_begin_frame() at stage_ordinal,
 * seal that ordinal, and render it with the borrowed renderer. Every OVStage
 * write the frame needs (camera state, any still-deferred runtime
 * render-product authoring) lands at stage_ordinal, so after this call the
 * write floor is at it and the renderer has consumed it. With stock OVRTX the
 * call waits for the whole render operation, which is its only public
 * guarantee that all borrowed Stage/Fabric reads have completed.
 * Tracking-anchor initialization also occurs here on the first renderable
 * frame, after its source camera is visible at the sealed floor.
 *
 * The step renders plan->render_products: OVXR's eye products followed by the
 * application products selected with ovxr_set_frame_render_products(). On a
 * renderable success plan->step_result borrows the resulting OVRTX step result
 * until ovxr_submit_frame() or ovxr_cancel_frame() consumes the plan; see
 * ovxr_frame_plan_t for what the application may do with it. In
 * application-stepped mode (ovxr_desc_t::application_steps_renderer) the call
 * stops after sealing and reading the floor into plan->render_ordinal: the
 * application performs the update and step at that ordinal before submit.
 *
 * out_stage_ordinal_sealed reports whether stage_ordinal was sealed, and is
 * written on every return path. It must be non-null whenever stage_ordinal is
 * nonzero, and may be null only for stage_ordinal 0, where OVXR owns the
 * ordinal and the caller has nothing to reconcile. This is not advisory: the
 * ordinal is sealed BEFORE the renderer work, and the floor read,
 * ovrtx_update_from_stage, the step enqueue and the step wait can all fail
 * afterwards. Those come back as ordinary errors, so a caller that only
 * inspected the result would mark a published ordinal unresolved and stall its
 * own allocator. Read it on failure and, when true, mark the ordinal resolved
 * and continue from a fresh one.
 *
 * OVXR_NOT_READY leaves the plan active without consuming stage_ordinal: OpenXR
 * located-view data is still pending for runtime PQW authoring, so nothing was
 * written or sealed (out_stage_ordinal_sealed is false). A plan's located view
 * data is fixed by the ovxr_begin_frame() that produced it, so binding the same
 * plan again cannot clear this: cancel the plan, call ovxr_begin_frame() again,
 * and bind the new plan with the same, still unconsumed, stage_ordinal.
 *
 * The first renderable bind of a context does more than the rest: it authors
 * the runtime render products, eye cameras and the managed anchor prim, seeds
 * the anchor mapping, writes this frame's camera state, and seals -- all at
 * this one stage_ordinal. It never asks for a second ordinal, because the
 * anchor is seeded from the value it just authored rather than read back.
 * Cancel the plan after any other bind failure so
 * OVXR can drain renderer work; cancelling ends the OpenXR frame and does not
 * unseal anything. A co-writing application calls this while holding its
 * Stage-publication rendezvous, with an ordinal it owns and every earlier
 * ordinal already resolved (see "OVStage ordinals").
 *
 * Calling this explicitly is only required by applications that co-write the
 * OVStage and need the stage-touching work bounded by their own lock and
 * ordinal. A sole writer may go straight from ovxr_begin_frame() to
 * ovxr_submit_frame(), which binds an unbound plan itself at
 * plan->timing.suggested_delta_time_seconds and a private ordinal; pass a
 * delta_time_seconds of 0, a stage_ordinal of 0 and a null
 * out_stage_ordinal_sealed here for the same effect.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_bind_frame(
    ovxr_context_t* context,
    ovxr_frame_plan_t* plan,
    double delta_time_seconds,
    ovstage_ordinal_t stage_ordinal,
    bool* out_stage_ordinal_sealed);

/*
 * Submit and consume a plan returned by ovxr_begin_frame(). A plan the caller
 * has not bound is bound here first, exactly as ovxr_bind_frame(context, plan,
 * plan->timing.suggested_delta_time_seconds, 0, NULL) would: that implicit bind
 * publishes the frame's OVStage writes at a private ordinal and steps the
 * renderer on the calling thread, so it is meant for applications that are the
 * sole OVStage writer. Co-writers bind explicitly, under their publication
 * lock and at their own ordinal. An implicit bind that fails leaves the plan
 * active with the same retry (OVXR_NOT_READY) or cancel obligations as an
 * explicit one.
 *
 * OVXR owns the bound OVRTX result and destroys it after OpenXR submission.
 * Only in application-stepped mode (ovxr_desc_t::application_steps_renderer)
 * does the caller supply the result, through submit_desc->step_result, and
 * then it also owns its lifetime; that mode requires an explicit
 * ovxr_bind_frame() first, so the implicit bind below is rejected. Validation
 * errors leave the plan active so the caller can correct the input or cancel it. Once OVXR accepts
 * ownership, the frame is consumed on every return path, including errors, and
 * plan->frame, plan->render_products and plan->step_result are cleared.
 *
 * submit_desc may be NULL. submit_desc->additional_layers optionally appends
 * application-owned OpenXR composition layers (projection, quad, or enabled
 * extension layer types) after the OVXR scene layer; a count of zero submits
 * only the scene. The array, the layers, their views and next chains must
 * remain valid until this call returns. Their spaces and swapchains must
 * belong to the shared session (see ovxr_get_openxr_handles() and
 * ovxr_get_vulkan_handles()). The caller creates, acquires, waits, renders and
 * releases its swapchains and completes the required graphics submission
 * before this call; OVXR neither touches their images nor synchronizes the
 * application's GPU work, and remains the only caller of
 * xrWaitFrame/xrBeginFrame/xrEndFrame. The runtime's maxLayerCount includes
 * the OVXR scene layer. Invalid arrays and counts are rejected before the plan
 * is consumed (and before an implicit bind); OpenXR validates layer contents
 * during submission. If should_render is false or OVXR's views are invalid,
 * all layers are omitted and the frame is ended without rendering. Readback
 * still captures only the OVRTX render product, not the compositor's output.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_submit_frame(
    ovxr_context_t* context,
    ovxr_frame_plan_t* plan,
    const ovxr_submit_desc_t* submit_desc,
    ovxr_frame_result_t* out_result);

/*
 * Cancel and consume a plan returned by ovxr_begin_frame(), draining any OVRTX
 * operation started by a failed bind. A valid plan has plan->frame,
 * plan->render_products and plan->step_result cleared before this call
 * returns. Invalid arguments leave the plan unchanged.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API void ovxr_cancel_frame(ovxr_context_t* context, ovxr_frame_plan_t* plan);

/*
 * Convenience frame path. The renderer supplied in the context descriptor
 * must be attached to desc.ovstage. The frame's OVStage writes land at
 * stage_ordinal (see "OVStage ordinals"; 0 = private ledger), which is sealed
 * before the renderer updates from and steps against it. At either kind of
 * ordinal this is exactly begin -> bind(stage_ordinal) -> submit, so it
 * behaves as the split loop does -- including bind's rejection of a negative
 * or non-finite delta_time_seconds. Unavailable (OVXR_ERROR) in
 * application-stepped mode, which needs the application's step between bind
 * and submit.
 * If OpenXR reports should_render=false or invalid views, this call ends the
 * OpenXR frame without stepping ovRTX or publishing per-frame OVStage camera
 * writes.
 *
 * out_stage_ordinal_sealed carries bind's report through unchanged and follows
 * the same rule: non-null whenever stage_ordinal is nonzero, optional only for
 * the private ledger. Because this form composes bind, it inherits bind's
 * seal-then-fail window exactly - a failure here may also have published the
 * ordinal, and this is the only way to tell.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_render_frame(
    ovxr_context_t* context,
    double delta_time_seconds,
    ovstage_ordinal_t stage_ordinal,
    ovxr_frame_result_t* out_result,
    bool* out_stage_ordinal_sealed);

/*
 * Fill out_view with the most recently captured render-product color frame.
 * Returns OVXR_NOT_READY when capture is disabled or no frame has been captured,
 * and OVXR_ERROR for invalid arguments. The referenced device buffer is owned by
 * OVXR; see ovxr_frame_view_t::device_ptr for its lifetime. Safe to call from
 * any thread after ovxr_create returns and before ovxr_destroy begins.
 * Threading: may overlap any call on the same context except ovxr_destroy().
 */
OVXR_API ovxr_result_t ovxr_acquire_frame(ovxr_context_t* context, ovxr_frame_view_t* out_view);

/*
 * Attribute update helper. Writes camera prim transforms through the borrowed
 * OVStage. This uses the same upsert-style authoring path as the frame update
 * and does not validate that every prim already exists. Callers that need typo
 * detection or strict must-exist behavior must prevalidate through OVStage.
 * The writes land at stage_ordinal (see "OVStage ordinals"; 0 = private
 * ledger), which is sealed before this call returns so the next renderer step
 * sees the new transforms.
 *
 * The camera-named historical entry point: ovxr_update_prim_transforms is the
 * same write for any prim, and this call is equivalent to it for cameras.
 * Threading: serialize with all non-acquire calls on the same context. The
 * ordinal is what makes a batch atomic, not what makes calls concurrent -
 * sealing one seals every ordinal below it, so a second writer holding a lower
 * ordinal would have its remaining writes rejected.
 *
 * When that second writer is another OVXR call, OVXR sees the open ordinal and
 * refuses the seal rather than tearing the batch. It CANNOT do that for writes
 * the application makes to the stage itself: those never enter OVXR's ledger,
 * so OVXR has no way to know an application batch is open at a lower ordinal.
 * Serializing OVXR's calls against your own stage writes - and resolving every
 * ordinal below the one you hand OVXR - remains yours to enforce.
 */
OVXR_API ovxr_result_t ovxr_update_camera_transforms(
    ovxr_context_t* context,
    const ovxr_camera_transform_t* cameras,
    size_t camera_count,
    ovstage_ordinal_t stage_ordinal);

/*
 * Attribute update helper. Writes WORLD transforms for arbitrary prims
 * through the borrowed OVStage — the general form of
 * ovxr_update_camera_transforms, for applications moving their own scene
 * prims (UI panels, cursors, tracked-device geometry) at frame rate.
 * Upsert-style: prims are not validated to exist, and each prim is authored
 * to opt out of its ancestors' transform stack (see ovxr_prim_transform_t).
 * The batch lands at stage_ordinal (see "OVStage ordinals"; 0 = private
 * ledger), which is sealed before this call returns so the next renderer step
 * sees the new transforms.
 * Threading: serialize with all non-acquire calls on the same context. The
 * ordinal is what makes a batch atomic, not what makes calls concurrent -
 * sealing one seals every ordinal below it, so a second writer holding a lower
 * ordinal would have its remaining writes rejected.
 *
 * When that second writer is another OVXR call, OVXR sees the open ordinal and
 * refuses the seal rather than tearing the batch. It CANNOT do that for writes
 * the application makes to the stage itself: those never enter OVXR's ledger,
 * so OVXR has no way to know an application batch is open at a lower ordinal.
 * Serializing OVXR's calls against your own stage writes - and resolving every
 * ordinal below the one you hand OVXR - remains yours to enforce.
 *
 * Cost: these prims are the application's, so a partial batch would persist
 * indefinitely and OVXR snapshots the transform attributes it is about to
 * overwrite in order to restore them on failure. That snapshot is a synchronous
 * read per call, not per prim, so pass every prim moving this frame in one call
 * rather than calling once per prim.
 */
OVXR_API ovxr_result_t ovxr_update_prim_transforms(
    ovxr_context_t* context,
    const ovxr_prim_transform_t* prims,
    size_t prim_count,
    ovstage_ordinal_t stage_ordinal);

/*
 * Attribute update helper. Writes render product `resolution` attributes
 * through the borrowed OVStage. This uses upsert-style authoring and does not
 * validate that every RenderProduct prim already exists. Callers that need typo
 * detection or strict must-exist behavior must prevalidate through OVStage.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_update_render_product_resolutions(
    ovxr_context_t* context,
    const ovxr_render_product_resolution_t* render_products,
    size_t render_product_count,
    ovstage_ordinal_t stage_ordinal);

/*
 * The last anchor prim selected before a frame boundary wins. A change is
 * published only after both eye cameras are authored successfully (or at a
 * no-render boundary), without recreating the context, OpenXR session/reference
 * space, swapchains, or renderer attachment. Threading: serialize these
 * operations with all non-acquire calls on the same context.
 */

/*
 * Queue a prim as the tracking anchor. `prim_path` must be a non-empty absolute
 * path and is copied before return. The prim must be readable from OVStage and
 * carry a valid transform now, so a typo fails here rather than at the next
 * frame boundary. Its latest committed OVStage world transform is then sampled
 * once at each frame boundary and drives both eyes. Configured OVXR eye-camera
 * paths are rejected because following a camera that OVXR itself drives would
 * create a feedback loop. The sampled transform is resolved according to
 * OVXR_SETTING_FLOOR_ALIGNED_ANCHOR.
 *
 * If the prim later becomes unreadable (deleted, or resolved as an OVXR eye
 * camera), the frame loop keeps the last committed anchor, logs a warning once
 * for that path, and resumes following the prim when it is readable again.
 *
 * The anchor is always prim-sourced. To place or teleport the user, call
 * ovxr_set_managed_anchor_transform() or author an application-owned anchor
 * prim's world transform through OVStage; the next frame boundary picks it up.
 * OVXR does not prescribe how that transform is chosen; the mapping documented
 * at ovxr_anchor_transform_t and the inputs exposed by
 * ovxr_get_anchor_transform(), ovxr_get_scene_info(), and
 * ovxr_get_openxr_handles() are sufficient to solve it. The managed prim
 * <runtime_prim_root>/Anchor may be selected like any other, including before
 * the first frame boundary has created it: the selection is queued and the prim
 * is authored with the rest of the runtime prims.
 *
 * This call writes nothing to the OVStage and therefore takes no ordinal. Its
 * validation read is a typed OVStage read, so it sees SEALED state only: a prim
 * you authored at an ordinal you have not sealed yet is not selectable yet.
 * The same rule governs a prim you MOVE, and it is stronger than "the update
 * is late". A typed read of a prim carrying an unsealed change FAILS; it does
 * not fall back to the previous sealed value. Every frame boundary re-reads
 * the active anchor prim, so a prim that is moved and left unsealed is
 * unreadable at every boundary, not just the first: the anchor stops advancing
 * and holds its last committed value for the rest of the session, reported
 * once as a warning and then silent.
 *
 * That makes one shape a trap. Moving an application anchor prim at ordinal N
 * and then calling ovxr_bind_frame(..., N) never advances the anchor: bind
 * reads the prim at an ordinal it is itself about to seal, so the read fails
 * every time. Following an application-owned prim therefore requires one of:
 *   - sealing the prim's ordinal BEFORE the boundary that must see it (the
 *     physics-driven shape: simulation seals its step, then OVXR binds), or
 *   - moving the prim through the ordinal-taking OVXR writers --
 *     ovxr_set_managed_anchor_transform(..., stage_ordinal) or
 *     ovxr_update_prim_transforms(..., stage_ordinal) -- and letting OVXR's
 *     own seal publish it.
 */
OVXR_API ovxr_result_t ovxr_set_anchor_prim(ovxr_context_t* context, ovx_string_t prim_path);

/*
 * Write the world transform of the OVXR-managed anchor prim
 * <runtime_prim_root>/Anchor and queue that prim as the anchor. The transform
 * uses the layout and validation rules of ovxr_anchor_transform_t. OVXR
 * publishes the USD, derived-world, and canonical live transform channels in
 * one OVStage batch and completes backend work before returning, matching
 * ovxr_update_camera_transforms(). The write lands at stage_ordinal (see
 * "OVStage ordinals"; 0 = OVXR's private ledger) and the ordinal is sealed on
 * success, which is what makes the new value visible to the next frame
 * boundary. The value becomes active at that boundary and is ordered with
 * ovxr_set_anchor_prim() and ovxr_clear_anchor(): the last request before a
 * boundary wins.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_set_managed_anchor_transform(
    ovxr_context_t* context,
    const ovxr_anchor_transform_t* transform,
    ovstage_ordinal_t stage_ordinal);

/*
 * Reset the managed anchor prim <runtime_prim_root>/Anchor to the identity
 * transform (tracking origin at the stage origin) and queue it as the anchor.
 * The prim is written at stage_ordinal (see "OVStage ordinals"; 0 = OVXR's
 * private ledger), which is sealed on success; the selection takes effect at
 * the next frame boundary and is ordered with ovxr_set_anchor_prim() and
 * ovxr_set_managed_anchor_transform(): the last request before a boundary wins.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_clear_anchor(ovxr_context_t* context, ovstage_ordinal_t stage_ordinal);

/*
 * Read the path of the prim driving the active anchor. This is never empty:
 * when no application prim is selected it is <runtime_prim_root>/Anchor.
 * `out_length` (optional) receives the path length excluding the terminator;
 * pass out_path = NULL and capacity = 0 to query the length. Fails when
 * capacity cannot hold the path plus terminator. Returns OVXR_NOT_READY until
 * the initial anchor is resolved.
 */
OVXR_API ovxr_result_t ovxr_get_anchor_prim(
    ovxr_context_t* context,
    char* out_path,
    size_t capacity,
    size_t* out_length);

/*
 * Read the exact active absolute anchor transform used by the last completed
 * frame boundary. Returns OVXR_NOT_READY until the initial anchor is resolved.
 */
OVXR_API ovxr_result_t ovxr_get_anchor_transform(
    ovxr_context_t* context,
    ovxr_anchor_transform_t* out_transform);

/*
 * Convert one OpenXR tracking pose with the active frame-latched anchor. pose
 * is expressed in meters relative to the XrSpace returned by
 * ovxr_get_openxr_handles(). out_scene_matrix is a row-major, row-vector rigid
 * scene pose. Call this after ovxr_begin_frame() to map controllers, hands,
 * rays, or spatial UI with the same transform as the eye cameras for that
 * frame.
 *
 * The orientation must be a unit quaternion. An unlocated OpenXR space location
 * commonly carries an all-zero pose, so check its locationFlags before mapping.
 */
OVXR_API ovxr_result_t ovxr_transform_openxr_pose(
    ovxr_context_t* context,
    const ovxr_pose_t* pose,
    double out_scene_matrix[16]);

/*
 * OpenXR/Vulkan discovery helper. Creates an OpenXR instance, finds the HMD
 * system, enumerates the selected mono, stereo, or quad-view configuration, and
 * queries graphics requirements and composition GPU identity where the runtime
 * supports it. Session and swapchain creation are handled by the frame path or
 * blank-frame helper.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_probe_openxr(ovxr_context_t* context, ovxr_openxr_probe_result_t* out_result);

/*
 * OpenXR blank-frame helper. Creates the standalone OpenXR Vulkan session path,
 * creates color swapchains, clears one selected-view-configuration frame,
 * locates views, and submits an OpenXR projection layer. This does not consume
 * ovRTX images.
 * Threading: serialize with all non-acquire calls on the same context.
 */
OVXR_API ovxr_result_t ovxr_submit_openxr_blank_frame(
    ovxr_context_t* context,
    ovxr_openxr_frame_result_t* out_result);

/*
 * Returns the error recorded by the most recent OVXR call on the calling
 * thread. Storage is thread-local: calls on other threads do not change it.
 * The pointer remains valid until the next OVXR call on this thread.
 * Threading: safe to call from any thread without synchronization.
 */
OVXR_API const char* ovxr_get_last_error(void);

/*
 * Logging
 *
 * OVXR emits its diagnostics on the "omni.ovxr" channel. In a build that hosts
 * Carbonite (e.g. an application linking ovxr-static) those messages already
 * reach the host's log. The packaged ovxr.dylib payload has no host to inherit,
 * so ovxr_set_log_callback is the only way for a consumer of the shipped
 * library to observe them. Mirrors ovrtx_set_log_callback / ovstage_set_log_callback.
 */

/* Severity levels. Values follow the underlying log-level ordering. */
typedef enum ovxr_log_severity_t
{
    OVXR_LOG_VERBOSE = -2, /* Most verbose (debug/trace). */
    OVXR_LOG_INFO = -1, /* Informational. */
    OVXR_LOG_WARNING = 0, /* Warning; operation continues. */
    OVXR_LOG_ERROR = 1, /* Error; operation may have failed (fatal is reported here too). */
    OVXR_LOG_NONE = 3, /* Threshold sentinel: as a filter level, disables all logging.
                          Never delivered to the callback. */
} ovxr_log_severity_t;

/*
 * Callback receiving OVXR log messages.
 *
 * Process-global (see ovxr_set_log_callback) and may be invoked from any
 * thread, not necessarily the one that emitted the message; invocations are
 * serialized, so the callback body needs no mutex of its own for its own
 * state. message is valid only for the duration of the call -- copy it to
 * retain.
 */
typedef void (*ovxr_log_callback_t)(ovxr_log_severity_t severity,
    double timestamp,
    ovx_string_t message,
    void* user_data);

/*
 * Install a process-global log callback. Process-global rather than per-context:
 * one callback for the whole process, replacing any previous one. Pass a null
 * callback to disable delivery.
 *
 * Scope: OVXR's own "omni.ovxr" channel, and nothing else. Messages from ovrtx,
 * ovstage, or anything else sharing the same Carbonite are not delivered here.
 * Installing an OVXR callback does not displace an ovrtx or ovstage one; the
 * three are independent sinks.
 *
 * severity is both the delivery threshold and the level the "omni.ovxr" channel
 * is raised to, because a channel's level gates a message before any sink can
 * see it. OVXR_LOG_INFO therefore really does produce informational messages.
 * OVXR_LOG_NONE installs the callback but delivers nothing. Clearing the
 * callback restores OVXR's own channel level to what it was before the first
 * install, and sets the shared log system's "omni.ovxr" threshold back to
 * inheriting the global threshold. The log system offers no way to read a
 * per-source threshold, so if the host application had set one for
 * "omni.ovxr" itself, clearing does not restore it; re-apply it after
 * clearing.
 *
 * Delivery thread and timing. Invocations are always serialized, so the
 * callback body needs no mutex for its own state. By default a message is
 * delivered on the thread that emitted it, before the emitting call returns.
 * That is not guaranteed: if the host has turned on asynchronous logging in
 * the Carbonite instance OVXR shares (ILogging::setLogAsync), or Carbonite
 * defers a message because it holds an internal lock, the message is
 * delivered later on another thread. Do not rely on the emitting thread's
 * thread-local state, and do not assume a message has been delivered when
 * the OVXR call that produced it returns. OVXR has no flush of its own; in
 * async mode the host can drain pending messages with ILogging::flushLogs.
 *
 * Replacing or clearing from another thread: this function waits for any
 * invocation already running to return. Once it returns, the previous
 * callback is not running and will not be called again, so its code and
 * user_data may be freed immediately. A language binding needs no
 * retire-and-defer scheme for trampolines.
 *
 * Replacing or clearing from inside the callback: allowed, and the change
 * applies to every later message. The invocation that made the call is still
 * running, though, so it must not free its own code (unload its module,
 * release its trampoline) or its user_data until it has returned.
 *
 * Install before ovxr_create() to observe context-creation diagnostics. This
 * requires ovrtx_initialize() to have been called: the packaged ovxr library
 * links no Carbonite of its own and borrows the one ovrtx already started.
 * Calling earlier returns OVXR_ERROR and explains so via ovxr_get_last_error().
 *
 * Returns OVXR_SUCCESS, or OVXR_ERROR if severity is invalid or the log hook
 * could not be brought up (see ovxr_get_last_error()).
 */
OVXR_API ovxr_result_t ovxr_set_log_callback(ovxr_log_severity_t severity,
    ovxr_log_callback_t callback,
    void* user_data);

/**
 * Programmatically opts out of the OVXR library startup reporting functionality for this session.
 *
 * OVXR sends one anonymous report that the library was initialized, from a background thread, when
 * ovxr_create() first succeeds. It reports at most once per process: an application that destroys
 * every context and then creates another one does not report a second time. This call disables that
 * report, and to take effect it must be called BEFORE the ovxr_create() that creates the first
 * context. Called afterwards it cannot take the report back, because this process has already used
 * up its one reporting opportunity; at best it abandons a report that has not gone out yet, since
 * the reporter waits briefly before assembling anything. Note that it loads the OVXR implementation
 * library if that has not already happened, since the opt-out state lives there; calling it therefore
 * brings the implementation's dependency graph forward from the first ovxr_create() to this call.
 *
 * Note that if the `OVX_PHONE_HOME_OPT_OUT` environment variable is defined and set to `1` for this
 * process, this call will be unnecessary. The environment variable based opt-out will always
 * override the programmatic opt-out for the process: once observed it stays in effect for the
 * lifetime of the process, and no OVXR call can clear it. When used, this environment variable is
 * expected to be defined for the process at launch time. Defining it programmatically with a call
 * such as `setenv()` is strongly discouraged since that can lead to a race condition crash on Linux
 * due to a known bug in the environment variable functions in glibc in versions earlier than 2.41.
 *
 * The startup event parameters contain the OVXR library name and version. The surrounding request
 * also includes a UTC timestamp, the library build variant (`clientVariant`: `"Release"`, or
 * `"iBeta"` for a debug library), and fixed protocol and placeholder fields. See TELEMETRY.md for
 * the full request fields.
 *
 * The message is sent as plain-text JSON over an HTTP connection, with no encryption or obfuscation
 * of the body or the headers, and nothing is written to disk.
 *
 * Note that this phone-home message will *never* contain any of the following information:
 *   * a user name, user ID, email address, real name, or geographical location.
 *   * any information about the process including process ID, host app name, module names, etc.
 *   * any personal information about a user.
 *   * any intellectual property information related to the host app or its developer.
 *   * any kind of session identifier or even an anonymous user ID.
 *
 * Destroying the last context tears the reporter down and joins its worker thread, so ovxr_destroy()
 * can block there. Ordinarily that is a join on a thread that finished long ago. The one case that
 * is not bounded is a teardown that lands while the worker sits in the platform DNS resolver, which
 * takes no timeout and cannot be cancelled; against a black-holing DNS server that can be tens of
 * seconds. An application that needs a hard bound on its own shutdown should opt out, which avoids
 * the worker thread entirely.
 *
 * Threading: safe to call from any thread without synchronization, with or without a live context.
 */
OVXR_API void ovxr_library_telemetry_opt_out(void);

#ifdef __cplusplus
}
#endif

#endif /* OVXR_H */
