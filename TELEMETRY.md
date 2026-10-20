# Data Collection & Use

## Overview

OVXR collects anonymous initialization data when the library initializes to help improve OVXR. Collection is **enabled by default** and can be turned off without affecting OVXR functionality.

## When collection happens

After the first successful OVXR context initialization in a process, OVXR schedules one background send attempt. Installing the wheel, importing `ovxr`, or an unsuccessful initialization does not itself trigger a report. Creating additional contexts in that process does not trigger additional reports. Delivery is best effort: there are no retries, and a short-lived process may exit before sending.

## What data is sent

The `LibraryStartup` event has two parameters: **library name and version**. The surrounding request also contains:

- A UTC timestamp, repeated in the event and request fields.
- The library version repeated in `clientVer`.
- A library build variant: `Release` or `iBeta` for a debug build. This describes the library build, not your application's build configuration.
- Fixed project and protocol values, including a project-wide `clientId`. That value identifies NVIDIA's telemetry project, not an individual user, device, or installation.
- Literal `undefined` values in unused identity, device, browser, and host-product fields, and literal `None` values in the protocol's consent fields. These placeholders are not values read from your account or device.

### Fields and expected values

The following tables list every value in the JSON request. Field paths such as `events[0].name` refer to the first and only event in the request. All values shown here are JSON strings. Version and time examples are illustrative, not fixed values.

#### Fixed and variable values

- **Built into the library:** the project/protocol fields, placeholder fields, and event name. `clientVariant` is set when the telemetry component is built (`Release` or `iBeta`), rather than detected from your application at runtime.
- **Library name and version:** the name is `ovxr`; the version reflects the OVXR library you are using.
- **Generated for each report:** the UTC timestamp, repeated in `events[0].ts` and `sentTs`.

#### Event and request information

| Field | Expected value | Meaning |
| --- | --- | --- |
| `events[0].name` | `"LibraryStartup"` | The event type; always the same. |
| `events[0].parameters.library` | `"ovxr"`  | Library name. |
| `events[0].parameters.version` | Your OVXR version, e.g. `"0.1.0"` | Version of the library being initialized. |
| `events[0].ts` | UTC time, e.g. `"2026-09-30T14:53:00.000Z"` | Time recorded when the report is prepared, with millisecond precision. |
| `sentTs` | Same value as `events[0].ts` | The same timestamp repeated for the telemetry protocol. |
| `clientVer` | Same value as `events[0].parameters.version` | Library version repeated for the telemetry protocol. |
| `clientId` | `"6697362192350625"` | Shared NVIDIA telemetry project ID; not an ID assigned to you or your device. |
| `clientType` | `"Native"` | Fixed client category. |
| `clientVariant` | `"Release"`; `"iBeta"` for a debug library build | Library build variant; not your application's build configuration. |
| `eventSchemaVer` | `"1.0"` | Fixed event-schema version. |
| `eventProtocol` | `"1.6"` | Fixed telemetry-protocol version. |
| `eventSysVer` | `"1.0"` | Fixed event-system version. |

#### Unused identity and device fields

The protocol includes the following fields, but OVXR does not populate them with information about you or your system. Each contains the literal string `"undefined"`—not a hidden value or an identifier.

| Field | Expected value |
| --- | --- |
| `integrationId` | `"undefined"` |
| `browserType` | `"undefined"` |
| `userId` | `"undefined"` |
| `externalUserId` | `"undefined"` |
| `idpId` | `"undefined"` |
| `sessionId` | `"undefined"` |
| `deviceId` | `"undefined"` |
| `cpuArchitecture` | `"undefined"` |
| `deviceOS` | `"undefined"` |
| `deviceOSVersion` | `"undefined"` |
| `deviceType` | `"undefined"` |
| `deviceMake` | `"undefined"` |
| `deviceModel` | `"undefined"` |
| `productName` | `"undefined"` |
| `productVersion` | `"undefined"` |

#### Protocol consent fields

These fields contain the literal string `"None"`. They do not reflect consent settings read from your account or device.

| Field | Expected value |
| --- | --- |
| `gdprTechOptIn` | `"None"` |
| `gdprBehOptIn` | `"None"` |
| `gdprFuncOptIn` | `"None"` |
| `deviceGdprTechOptIn` | `"None"` |
| `deviceGdprBehOptIn` | `"None"` |
| `deviceGdprFuncOptIn` | `"None"` |

### Complete example request body

This illustrative release-build example uses library name `ovxr`, version `0.1.0`, and a sample timestamp. Version and timestamp values vary with the actual library and invocation.

```json
{
  "clientId": "6697362192350625",
  "eventSchemaVer": "1.0",
  "eventProtocol": "1.6",
  "eventSysVer": "1.0",
  "integrationId": "undefined",
  "clientType": "Native",
  "clientVariant": "Release",
  "browserType": "undefined",
  "userId": "undefined",
  "externalUserId": "undefined",
  "idpId": "undefined",
  "sessionId": "undefined",
  "deviceId": "undefined",
  "cpuArchitecture": "undefined",
  "deviceOS": "undefined",
  "deviceOSVersion": "undefined",
  "deviceType": "undefined",
  "deviceMake": "undefined",
  "deviceModel": "undefined",
  "productName": "undefined",
  "productVersion": "undefined",
  "gdprTechOptIn": "None",
  "gdprBehOptIn": "None",
  "gdprFuncOptIn": "None",
  "deviceGdprTechOptIn": "None",
  "deviceGdprBehOptIn": "None",
  "deviceGdprFuncOptIn": "None",
  "events": [
    {
      "name": "LibraryStartup",
      "ts": "2026-09-30T14:53:00.000Z",
      "parameters": {
        "version": "0.1.0",
        "library": "ovxr"
      }
    }
  ],
  "sentTs": "2026-09-30T14:53:00.000Z",
  "clientVer": "0.1.0"
}
```

### Destination and transport

The report is an HTTP POST to:

```text
http://6697362192350625.events.telemetry.data.nvidia.com/v1.1/events/json
```

**The connection uses unencrypted HTTP, not HTTPS.** The request can therefore be read by parties able to observe the network traffic. It does not use user credentials or an embedded secret.

The request also includes these HTTP headers. The user-agent is a fixed library string, not your browser's user-agent.

| Header | Expected value |
| --- | --- |
| `Host` | `6697362192350625.events.telemetry.data.nvidia.com` |
| `User-Agent` | `ovx-phone-home/1` |
| `Accept` | `application/json` |
| `X-Event-Protocol` | `1.6` |
| `Content-Type` | `application/json;charset=utf-8` |
| `Content-Length` | Byte length of the JSON body; varies with the actual values. |
| `Connection` | `close` |

### Information added by the receiving service

As with any network request, the receiving service can see the connection's source IP address. It uses that address to derive approximate geographic and network information and whether the request originates from an NVIDIA network. The raw client IP address is not retained by the ingestion service.

Derived metadata can include country, region, city, timezone, estimated latitude/longitude, and network-provider information. These are IP-based estimates, not GPS coordinates read from your device; their accuracy varies. The service also adds processing metadata such as receipt time and processing datacenter. These fields are separate from the JSON sent by OVXR.

For information about NVIDIA's handling of data, see the [NVIDIA Privacy Policy](https://www.nvidia.com/en-us/about-nvidia/privacy-policy/).

## Turn off data collection

Either of the following controls prevents the telemetry request without disabling OVXR functionality.

### Environment variable

Set `OVX_PHONE_HOME_OPT_OUT` to exactly `1` **before starting the application process**.

Linux / macOS shell:

```bash
export OVX_PHONE_HOME_OPT_OUT=1
# Launch your application from this shell.
```

Windows PowerShell:

```powershell
$env:OVX_PHONE_HOME_OPT_OUT = "1"
# Launch your application from this shell.
```

Changing the environment variable from inside an already-running application is not a supported way to opt out. Use the programmatic control below instead, before the first context initialization.

### Programmatic control

Python:

```python
import ovxr
ovxr.library_telemetry_opt_out()
# Create your ovxr.Context only after this call.
```

C/C++:

```c
ovxr_library_telemetry_opt_out();
/* Call before the first ovxr_create(). */
```

Opt out before initialization to prevent the first report. An opt-out cannot retract a request that has already been sent.

## Offline use and shutdown

Report delivery happens in the background; initialization does not wait for delivery. Offline or blocked delivery does not fail OVXR initialization and does not produce a telemetry failure log.

If a platform DNS lookup is still in progress when the final OVXR context is destroyed, cleanup can wait for the platform's resolver timeout. Disabling telemetry before initialization avoids this telemetry-related lookup and wait.
