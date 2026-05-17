# src/plugins/qhyccd — QHYCCD Hardware Driver

**Generated:** 2026-05-17

## OVERVIEW
Real QHYCCD camera hardware driver implementing `ICameraDriver`. Communicates with physical camera via QHYCCD SDK. Plugin metadata: `Keys: ["qhyccd"]`.

## DRIVER LOCATION
`C:\Program Files\QHYCCD\AllInOne\sdk\x64`

## STRUCTURE
```
qhyccd/
├── QHYCCDDriver.cpp/h      # Main driver implementation
├── qhyccd.json             # Plugin metadata (Keys: ["qhyccd"])
├── sdk/                    # QHYCCD vendor SDK
│   ├── include/            # SDK headers (DO NOT MODIFY)
│   ├── lib/                # Pre-built SDK libraries
│   └── manual.md           # SDK API reference
└── CMakeLists.txt          # Plugin build config
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Driver implementation | `QHYCCDDriver.cpp` |
| Plugin metadata | `qhyccd.json` |
| SDK headers | `sdk/include/` |
| SDK manual | `sdk/manual.md` |

## CONVENTIONS
- Follow existing qhyccd test patterns for signal/event structure
- Use async signals for frame delivery (never block main thread)
- Stream mode support via `stream_mode` parameter
- Cross-mode capture supported

## ANTI-PATTERNS
- **DO NOT** modify SDK files in `sdk/include/` — vendor files
- **DO NOT** block the main thread — use signals for all async events
- **DO NOT** use the same plugin name as another driver