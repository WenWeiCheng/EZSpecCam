# EZSpecCam CLI

Headless camera control via command-line interface. Stateless — each run connects, configures, captures, and disconnects.

## Quick Start

```bash
# List available cameras
ezspeccam_cli --list

# Show all parameters for a camera
ezspeccam_cli --camera mock-001 --list-params

# Capture 3 frames as TIFF to ./output/
ezspeccam_cli --camera mock-001 --frames 3 --output ./output

# Capture with custom parameters
ezspeccam_cli --camera mock-001 --set exposure=500 --set gain=5.5 --frames 5
```

## Options

### Camera

| Option | Description |
|--------|-------------|
| `--list` | Enumerate all available cameras and exit |
| `--camera <id>` | Camera ID to connect to (required for all other operations) |
| `--list-params` | List all parameters with current values, types, and flags. Exit after listing. |

### Parameters

| Option | Description |
|--------|-------------|
| `--set <name>=<value>` | Set a camera parameter. Repeatable. Values are auto-parsed as double, integer, or string. |

### Capture

| Option | Default | Description |
|--------|---------|-------------|
| `--frames <n>` | `1` | Number of frames to capture |
| `--output <dir>` | `.` | Output directory (created if missing) |
| `--format <fmt>` | `tiff` | Output format: `tiff` or `csv` |
| `--prefix <str>` | `""` | Filename prefix |
| `--suffix <str>` | `""` | Filename suffix |
| `--no-metadata` | off | Skip saving `_metadata.json` sidecar files |

### Sequence

| Option | Description |
|--------|-------------|
| `--sequence <file>` | Run an event sequence from a JSON file (see below) |

### General

| Option | Description |
|--------|-------------|
| `--help`, `-h` | Print help |
| `--version`, `-v` | Print version |

## Output Files

Filenames follow the convention (underscores auto-inserted):

```
[{prefix}_]img_{yyyyMMdd_hhmmss_zzz}[_{suffix}].{ext}
```

| `--prefix` | `--suffix` | Result |
|------------|------------|--------|
| (none) | (none) | `img_20260602_143021_550.tiff` |
| `test` | (none) | `test_img_20260602_143021_550.tiff` |
| (none) | `cooled` | `img_20260602_143021_550_cooled.tiff` |
| `test` | `cooled` | `test_img_20260602_143021_550_cooled.tiff`

### Metadata JSON

```json
{
    "cameraId": "mock-001",
    "timestamp": 1780368436772000,
    "frameNumber": 1,
    "parameters": {
        "exposure": 100,
        "gain": 1,
        "cooling_sensor_temp": 25,
        ...
    }
}
```

## Event Sequences

For workflows requiring multiple configurations or stabilized conditions, use `--sequence` with a JSON file.

### Format

```json
{
    "settings": {
        "output": "./data",
        "format": "tiff",
        "prefix": "run1_",
        "suffix": "",
        "save_metadata": true
    },
    "steps": [
        {
            "configure": {
                "exposure": 100,
                "gain": 1.0
            }
        },
        {
            "capture": {
                "frames": 10,
                "output": "./data/room_temp"
            }
        },
        {
            "configure": {
                "cooling_enabled": true,
                "cooling_target_temp": -20
            }
        },
        {
            "wait_stable": {
                "parameter": "cooling_sensor_temp",
                "target": -20.0,
                "tolerance": 0.1,
                "window_sec": 30,
                "timeout_sec": 600
            }
        },
        {
            "capture": {
                "frames": 100,
                "prefix": "cooled_"
            }
        }
    ]
}
```

### Step Types

**`configure`** — Set camera parameters before the next step.

```json
{ "configure": { "exposure": 200, "gain": 5.0 } }
```

**`capture`** — Acquire frames. All fields are optional and inherit from `settings`.

```json
{
    "capture": {
        "frames": 10,
        "output": "./subdir",
        "format": "csv",
        "prefix": "scan_",
        "save_metadata": false
    }
}
```

**`wait_stable`** — Poll an extrinsic parameter until it stabilizes.

| Field | Default | Description |
|-------|---------|-------------|
| `parameter` | (required) | Parameter name (e.g. `cooling_sensor_temp`) |
| `target` | (required) | Target value |
| `tolerance` | `0.1` | Allowed deviation from target |
| `window_sec` | `10` | Time window over which stability must hold |
| `timeout_sec` | `300` | Maximum wait before aborting |

The parameter is polled every 500ms. Stability is reached when all readings in the time window are within `tolerance` of `target`. Progress is printed continuously so the user can monitor the approach.

### CLI Overrides

When using `--sequence`, CLI arguments for `--output`, `--format`, `--prefix`, `--suffix`, and `--no-metadata` act as fallbacks for any `settings` fields not specified in the JSON.

## Parameter Flags

`--list-params` annotates parameters with flags:

| Flag | Meaning |
|------|---------|
| `[readonly]` | Cannot be modified |
| `[extrinsic]` | Value depends on environment/time; user cannot set it directly |
| `[dynamic]` | Value may change with other parameters or conditions |

Extrinsic parameters (e.g. sensor temperature) are ideal targets for `wait_stable`. Read-only + extrinsic parameters change on their own and should be polled rather than set.

## Build

```bash
.\build_preset.bat debug
```

The CLI binary is at `build/msvc-debug/bin/Debug/ezspeccam_cli.exe`. Mock plugins are deployed alongside it automatically.

Enable the CLI build in `CMakePresets.json` by setting `EZSPECCAM_BUILD_CLI` to `"ON"` in the desired configure preset.
