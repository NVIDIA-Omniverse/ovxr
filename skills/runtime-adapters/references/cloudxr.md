# CloudXR service and opaque-channel adapter

CloudXR is a separately licensed OpenXR runtime. Use its matching `cxrServiceAPI.h`, `XR_NV_opaque_data_channel.h` and runtime/client documentation as the authority. This adapter documents the integration seam; it does not make those APIs OVXR exports.

## Service lifecycle

A standalone host may create/configure/start its own service through the supplied `nv_cxr_*` API. Stop requests termination; join completes it; destroy releases it. Follow the vendor's event layout, property mutability and process lifetime rules rather than copying a historical ctypes struct. Configure create-time settings before service creation as the distribution specifies.

CloudXR may expose no HMD system until a client connects. Build expensive renderer state once and distinguish client/system readiness from a failed library load. Backend/profile/port changes belong to the service, and may require client reconnect or process restart.

## Borrowed-session channel

Request `XR_NV_opaque_data_channel` and its required UUID extension before OVXR instance creation. Obtain borrowed OpenXR handles after readiness, then resolve extension functions through `xrGetInstanceProcAddr`. Do not start a second session/frame loop.

Use the supplied header's calling convention and layouts. Channel UUIDs are 16 RFC-4122 bytes; both ends must agree. Distinguish the documented OpaqueDataChannel entry points from historical similarly named prototype transports.

Pump bounded messages on the frame owner; treat not-connected as readiness. Destroy application channels before their borrowed instance/session disappears. Viewer message envelopes, input normalization, byte ceilings and recovery policy are application contracts.

## Client/backend compatibility

Select properties from the installed distribution. Backend names, ports and supported clients belong to the installed CloudXR release. Do not assume historical prerelease pairings describe its current supported profile. Inspect the actual listener and match the supplied client profile. UDP media may appear only after handshake.

Check runtime readiness, effective backend/listener, network reachability, client profile, extension/channel readiness, then application dispatch separately. Preserve unknown and unsupported states; video success does not prove messaging or native hand tracking.
