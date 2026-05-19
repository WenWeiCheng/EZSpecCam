# src/plugins/hamamatsu — Hamamatsu DCAMSDK4 Hardware Driver

**Generated:** 2026-05-18

Real Hamamatsu camera hardware driver implementing `ICameraDriver`. Communicates with physical camera via Hamamatsu DCAMSDK4 (`dcamapi.dll` / `libdcamapi.so`). Supports all Hamamatsu DCAM-compatible cameras (ORCA series, etc.).

---

## DCAMSDK4 DESIGN OVERVIEW

DCAMSDK4 is a **minimal C API** (~25 functions, 2 headers) that achieves enormous expressiveness through a **flat address-space property system**. Instead of creating one function per camera feature, it maps all features into a 32-bit ID namespace and provides unified `get/set` primitives.

---

## WHERE TO LOOK

| Task | Location |
|------|----------|
| DCAMSDK4 API reference | `driver-sdk/dcamsdk4/doc/api_reference/dcamapi4_en.html` |
| Property reference | `driver-sdk/dcamsdk4/doc/api_reference/property_reference_en.html` |
| C++ samples | `driver-sdk/dcamsdk4/samples/cpp/` (22 examples) |
| Driver interface contract | `src/core/ICameraDriver.h` |
| Parameter type definitions | `src/core/CameraTypes.h` |
| Existing implementation (reference) | `src/plugins/qhyccd/QHYCCDDriver.cpp` |
| Mock implementation (reference) | `src/plugins/mock/MockCameraDriver.cpp` |

## Samples

In `driver-sdk/dcamsdk4/samples/cpp/`:

| Sample | Explanation |
| :--- | :--- |
| init_uninit | **initialize and uninitialize DCAM-API**Initialize DCAM and display device information by using index. Enable directives to set OPTION and GUID. |
| open_show_modelinfo | **open device and show device information**Open device and display device information using DCAM handle. |
| propertylist | **show property list that the device supports**Display name and ID of all supported properties. Enable detectives to show the property details. |
| live_average | **capture image and show average of image**Retrieve images and display the average of each image. Enable directives to change stop timing. |
| access_image | **access image with several function**Capture and access image. Enable directive to change access method. |
| binning_subarray | **set binning and subarray**Set the values of binning and subarray . Each value is defined by the directive. |
| software_trigger | **get image with software trigger**Capture images using software trigger. Fire trigger using key-press. |
| framebundle | **access images and meta data on setting framebundle**Access each image and meta data after setting the values of framebundle. |
| recording | **record image to the harddisk automatically**Record images to the designated file. Enable directive to write meta data for file, session or frame. Support on Windows only. |
| copymetadata | **get image and copy meta data block**Copy and display image meta data(time stamp and frame stamp). |
| attach_imagebuf | **attach buffer to receive image data**Attach a buffer to receive the image data |
| attach_metadatabuf | **attach buffer to receive meta data**Attach a buffer to receive the meta data(time stamp or frame stamp). Enable directive to change the buffer format. |
| splitview | **set parameter and access image on spiltview**Set split view mode and retrieve split image. Not supported by all cameras. |
| setdata_lut | **set look up table**Apply LUT to camera output. Not supported by all cameras; |
| setdata_region | **set region for data reduction**Reduce output data by applying rectangle region or byte mask. Not supported by all cameras. |
| unpack_mono12 | **unpack mono12 data**Retrieve mono 12 packed data from DCAM. Unpack the data. Not supported by all cameras. |
| imageproc_option | **process image on access image**Retrieve high contrast image from DCAM. Not supported by all cameras. |
| control_calibration | **get and store calibration data**Control camera to make calibration data and store in memory. Not supported by all cameras. |
| devicebuffermode | **set parameter related device buffer mode and access image**Set and get parameters related device buffer mode and access all images in the shortest time. Not supported by all cameras. |
| burst_copy | **continuous copying to the user buffer**Copy each image data to local buffer while checking number of transferrd image. |
| control_MAICO | **control MAICO**Set each property of AICO and acquire an image. |

---

## C16091-10 PROPERTY REFERENCE

Using c++ sample code(`propertylist`) it has confirmed that C16091-10 camera has properties shown in `driver-sdk\dcamsdk4\samples\cpp\propertylist\c16091-output.txt`. And Consider only C16091-10's supported properties at this stage.

### Property Classification

C16091-10 is a 1D linear camera (512 pixels horizontal, 1 pixel vertical, B/W, 16-bit).

#### Excluded Properties (per requirements)

Only these 12 properties are excluded:

| Property | Reason |
|----------|--------|
| `SUBARRAY MODE` | Excluded per requirement |
| `SUBARRAY HPOS` | Excluded per requirement |
| `SUBARRAY HSIZE` | Excluded per requirement |
| `SUBARRAY VPOS` | Excluded per requirement |
| `SUBARRAY VSIZE` | Excluded per requirement |
| `TIMING READOUT TIME` | Excluded per requirement |
| `TIMING CYCLIC TRIGGER PERIOD` | Excluded per requirement |
| `TIMING MIN TRIGGER BLANKING` | Excluded per requirement |
| `TIMING MIN TRIGGER INTERVAL` | Excluded per requirement |
| `RECORD FIXED BYTES PER FILE` | Excluded per requirement |
| `RECORD FIXED BYTES PER SESSION` | Excluded per requirement |
| `RECORD FIXED BYTES PER FRAME` | Excluded per requirement |

**All other properties (including Info category) are included.**

### C16091 Parameter Mapping

#### Category: Info (Read-only informational, 17 parameters)

| ICameraDriver Name | DCAM Property | Type | Notes |
|-------------------|---------------|------|-------|
| `vendor` | `DCAM_IDSTR_VENDOR` | String | Read-only |
| `model` | `DCAM_IDSTR_MODEL` | String | Read-only |
| `camera_id` | `DCAM_IDSTR_CAMERAID` | String | Read-only |
| `bus` | `DCAM_IDSTR_BUS` | String | Read-only |
| `camera_version` | `DCAM_IDSTR_CAMERAVERSION` | String | Read-only |
| `driver_version` | `DCAM_IDSTR_DRIVERVERSION` | String | Read-only |
| `module_version` | `DCAM_IDSTR_MODULEVERSION` | String | Read-only |
| `dcamapi_version` | `DCAM_IDSTR_DCAMAPIVERSION` | String | Read-only |
| `color_type` | `COLORTYPE` | StringCollection | Fixed: B/W |
| `bit_depth` | `BIT PER CHANNEL` | IntRange | Fixed: 16-bit |
| `detector_pixels_horz` | `IMAGE DETECTOR PIXEL NUM HORZ` | IntRange | Fixed: 512 |
| `detector_pixels_vert` | `IMAGE DETECTOR PIXEL NUM VERT` | IntRange | Fixed: 1 |
| `image_width` | `IMAGE WIDTH` | IntRange | Read-only, max 512 |
| `image_height` | `IMAGE HEIGHT` | IntRange | Read-only, max 1024 |
| `image_pixel_type` | `IMAGE PIXEL TYPE` | StringCollection | Fixed: MONO16 |
| `buffer_pixel_type` | `BUFFER PIXEL TYPE` | StringCollection | Fixed: MONO16 |
| `system_alive` | `SYSTEM ALIVE` | StringCollection | Volatile: OFFLINE/ONLINE |

#### Category: Core (User-direct controllable, 7 parameters)

| ICameraDriver Name | DCAM Property | Type | Access | Notes |
|-------------------|---------------|------|--------|-------|
| `exposure` | `EXPOSURE TIME` | FloatRange | RW | 8μs – 30s, step 4μs |
| `contrast_gain` | `CONTRAST GAIN` | IntRange | RW | 0 – 3, step 1 |
| `binning` | `BINNING` | StringCollection | RO | Fixed: 1x1 (no binning) |
| `trigger_source` | `TRIGGER SOURCE` | StringCollection | RW | INTERNAL, EXTERNAL |
| `trigger_mode` | `TRIGGER MODE` | StringCollection | RO | Fixed: NORMAL |
| `trigger_active` | `TRIGGER ACTIVE` | StringCollection | RO | Fixed: EDGE |
| `trigger_polarity` | `TRIGGER POLARITY` | StringCollection | RW | NEGATIVE, POSITIVE |
| `trigger_connector` | `TRIGGER CONNECTOR` | StringCollection | RO | Fixed: BNC |

#### Category: Cooling (4 parameters)

| ICameraDriver Name | DCAM Property | Type | Access | Notes |
|-------------------|---------------|------|--------|-------|
| `sensor_temperature` | `SENSOR TEMPERATURE` | FloatRange | RO | -50°C – 100°C, step 0.1°C |
| `sensor_cooler` | `SENSOR COOLER` | StringCollection | RW | OFF, ON |
| `sensor_temperature_target` | `SENSOR TEMPERATURE TARGET` | FloatRange | RW | -20°C – 20°C, step 1°C |
| `sensor_cooler_status` | `SENSOR COOLER STATUS` | StringCollection | RO | READY |

#### Category: Advanced (5 parameters)

| ICameraDriver Name | DCAM Property | Type | Access | Notes |
|-------------------|---------------|------|--------|-------|
| `readout_frequency` | `READOUT FREQUENCY` | FloatRange | RW | 1.25MHz – 5MHz, step 1.25MHz |
| `sensor_mode` | `SENSOR MODE` | StringCollection | RO | Fixed: LINE |
| `line_bundle_height` | `SENSOR MODE LINE BUNDLE HEIGHT` | IntRange | RW | 8 – 1024, step 8 |
| `capture_mode` | `CAPTURE MODE` | StringCollection | RO | Fixed: NORMAL DATA |

### TYPE MAPPING RULES

| DCAM Type | ParameterType | Condition |
|-----------|--------------|-----------|
| `DCAMPROP_TYPE_REAL` | `FloatRange` | Always |
| `DCAMPROP_TYPE_LONG` | `IntRange` | Always |
| `DCAMPROP_TYPE_MODE` | `StringCollection` | Has valuetext (use text labels) |
| `DCAMPROP_TYPE_MODE` | `IntCollection` | No valuetext (fallback) |

For **Info category** (per requirement): all properties use `String` or `StringCollection` type regardless of DCAM type.

### Implementation Notes

1. **MODE → StringCollection**: When a MODE property has valuetext (e.g., TRIGGER SOURCE has "INTERNAL"/"EXTERNAL"), store the text strings in `constraint.validValues` and use `StringCollection` type.

2. **Dynamic Discovery**: Properties should be discovered at runtime via `dcam_getnextpropertyid()`, not hardcoded. Use the exclusion list above to filter.

3. **C16091 Limitations**:
   - No pixel binning support (BINNING fixed at 1x1)
   - No vertical dimension control (VPOS/VSIZE not applicable for 1D sensor)
   - Only INTERNAL and EXTERNAL trigger sources supported

---

## ANTI-PATTERNS

- **DO NOT** hardcode parameter lists — DCAM properties vary per camera model; use runtime enumeration via `dcamprop_getnextid()`
- **DO NOT** assume all properties exist — always check `dcamprop_getattr()` return value
- **DO NOT** block the main thread — use QThread for the dcamwait_start loop
- **DO NOT** assume rowbytes = width × bpp — DCAM may add alignment padding
- **DO NOT** modify SDK headers in `driver-sdk/dcamsdk4/inc/` — vendor files, copy to local `sdk/incude/` if needed. And copy `driver-sdk\dcamsdk4\lib` to local `sdk/include` if needed. Look at qhyccd driver directory for reference.