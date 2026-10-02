# src/plugins/hk16011 — HK16011 CCD 光谱相机驱动

面向 HK16011 CCD 光谱相机的硬件驱动，实现 `ICameraDriver`。通过厂商 C SDK
（`libHK16011`）通信：UART 走参数协议，FX2 USB 批量端点走图像数据。

---

## SDK 概况

| 项目 | 说明 |
|------|------|
| 公开头文件 | `include/hk16011.h` —— **仅此一个**，其余 `src/` 下的头文件属 SDK 私有，外部不得 include |
| 产物 | `lib/libHK16011.so.1.0.0` + `.so.1` / `.so` 符号链接，SONAME 为 `libHK16011.so.1` |
| 语言 | 纯 C99，`extern "C"`；句柄 `HK16011_DeviceHandle` 不透明 |
| 传递依赖 | `libusb-1.0`、`libserialport`（`.so` 自身的 `DT_NEEDED`，链接时只需 `-lHK16011`） |
| pkg-config | **没有**，需自行传 `-I` / `-L` / `-l` |
| 设备 | Cypress FX2（VID `0x04B4` / PID `0x00F1`）+ CH340 UART 桥（VID `0x1A86`） |

构建 SDK 本身：`mkdir -p lib && make lib`（`lib/` 目录不存在时 `make` 会直接失败）。

**参数总量 34 个**，由 `LISTPARAMS` + 逐个 `GETINFO` 在 `HK16011_Open` 时探测并缓存。

---

## 三个必须知道的设备行为

这三条是实测结论，不是文档抄的。**改动采集路径前务必先读这一节。**

### 1. 有界的 fetch 收不到帧

`ACQ fetch <n>` 在 `n == 1` 时**一帧都收不到**（4/4 次，3 秒与 5 秒超时均失败）；
`n >= 2` 正常；`ACQ fetch 0`（连续）始终正常。

### 2. 任何「会结束」的采集都丢最后一帧

`ACQ burst <n>` 稳定只送 n−1 帧（15/15 次，n = 1/2/3/5 全部正好少一帧），且
`frame_num_ready` 停在 1 —— 最后一帧卡在 DDR3 缓存里没被推上 EP2。

原因：帧数据没有帧头，SDK reader 一旦被残留数据打乱字节流就无法重新同步，
只能靠 `HK16011_SoftReset` 清空。

### 3. 实时采集是唯一可靠路径

`ACQ live`（`StartCapture(0)`）没有「终止帧」：下一次曝光会把待发的那帧顶出去，
所以缓存始终排空到 0、不丢帧。实测 1024×64 下 2 秒 13 帧，
「实时 + 收到 N 帧自停」在 N = 1..5 共 20 次试验中 19 次精确命中。

### 驱动因此采用的策略

**无论调用方传多少帧，一律 `HK16011_StartCapture(dev, 0)` + `HK16011_StartFetch(dev, 0)`，
收到 N 帧后自行 `AbortFetch` + `AbortCapture`。** 这样 `startCapture(1)` 与
`startCapture(n)` 行为一致。

> 注：`StartFetch` 失败时若报 `InvalidArg`（-1），多半是 `image_width` / `image_height`
> 没被**写过**。SDK 只从 `SetParamValue` 调用里学几何尺寸，单纯读取不填内部字段。
> 驱动因此在每次开始采集前主动回写一次当前几何值（幂等）。

---

## 线程模型

| 线程 | 做什么 |
|------|--------|
| Qt 线程（持有本对象） | 所有 SDK 调用；drain 帧队列并 `emit frameReady()` |
| SDK 内部 reader 线程 | 通过 C 回调交付原始帧 |

回调里只做最小工作：把裸缓冲区包成 `QImage`、推进互斥保护的队列，**不碰任何 QObject、
不 emit 任何信号**。需要唤醒 Qt 线程时用 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`，
并且用 `m_deliveryScheduled` 做「空→非空」跳变判断，一批只唤醒一次。

帧是**成批**到达的，所以 `deliverQueuedFrames()` 里必须逐帧判上限，否则
`startCapture(2)` 可能发出 3 帧 —— 契约要求的是「恰好 N 帧」。

---

## 参数映射

| SDK `HK16011_ValueType` | EZSpecCam `ParameterType` | 备注 |
|-------------------------|---------------------------|------|
| `Int` | `IntRange` | 读 `min/max/step` 的 `data.i` |
| `Float` | `FloatRange` | 读 `data.d` |
| `Bool` | `Boolean` | |
| `Enumeration` | `StringCollection` | **`validValues` 放线上 token**（如 `line_binning`），因为那才是 `SETPARAM` 认的东西；友好的 label 写进 `description` |
| `String` | `String` | |

### 两个固件侧的坑

**默认值永远是空的。** 固件的 `GETINFO` 只回 `"<min>:<max>:<step>"`，从不报默认值，
所以 SDK 交回来的 `default_value` 恒为 `Invalid`。而 `ParameterDefinition::isValid()`
对可写参数会校验 `defaultValue`，空值直接判为非法。驱动因此回退到「设备当前值」，
再退到量程下限。

**联合体只能按 `type` 读。** `HK16011_ValueStruct` 是 tagged union，
对 `Int`/`Float` 读 `data.s.set` 会把数值位当成指针解引用 —— 直接段错误。
`HK16011_FreeValue` 有类型保护（标量时是 no-op），只有 `String`/`Enumeration`
才持有堆内存。

### 未实测的遥测

以下四个参数固件没有接真实传感器，返回的是**落在自身声明量程之外**的占位值：

| 参数 | 读数 | 声明量程 |
|------|------|----------|
| `sensor_temp` | -50.0 | [0, 80] |
| `environment_temp` | -50.0 | [0, 80] |
| `tec_voltage` | -5.001 | [-3.3, 3.3] |
| `tec_current` | -2.381 | [-1.1, 1.1] |

整条 TEC 温控环没有接实际硬件，`tec_set_temp` / `tec_kp` / `tec_ki` / `tec_kd`
可以写可以读回，但控制不了任何物理量。

驱动用**「只读数值参数的读数落在自身声明量程之外」**这一通用特征识别占位值
（而不是硬编码参数名，这样以后固件新增占位项也能自动识别），
并在其 `description` 追加 `[not implemented: ...]` 注记，避免 GUI 把它当真实数据显示。

---

## 采集关键参数（几何一致性）

`image_width` / `image_height` / `read_mode` 属于**关键参数**（见
`hk16011CriticalParameters()`）。SDK reader 每一帧都按当前 w/h 重新推导
`expected_bytes`，因此批量下发时只要其中一个失败，就**整批拒绝**，
避免出现「设备已改尺寸、reader 仍按旧尺寸解析」的错位。

---

## enumerate() 为什么不用 HK16011_Open

`PluginLoader::scan()` 在每次启动时会对每个已加载插件调用 `enumerate()`，
而 `HK16011_Open()` 会 claim USB 接口并独占 UART —— 启动扫描绝不能做这件事。
驱动改为直接用 libusb 探测 VID/PID，不碰任何接口。

---

## 构建

```bash
export HK16011_ROOT=/path/to/hk16011/sdk/tree
cmake --preset linux-debug
```

`HK16011_ROOT` 指向的目录需含 `include/hk16011.h` 与 `lib/libHK16011.so`。
也可在 configure 阶段用 `-DHK16011_SDK_ROOT=<path>` 覆盖，或放进
`CMakeUserPresets.json`。**未设置时插件带 `WARNING` 自行跳过**，不阻塞其余构建；
需要硬失败时传 `-DEZSPECCAM_REQUIRE_HK16011=ON`。

**注意：仓库内任何被 Git 跟踪的文件都不得写死该路径**（见根 `AGENTS.md` 的「构建可移植性」）。

---

## 测试

`tests/test_hk16011_driver/`，编译期直接编入驱动源码（不通过 QPluginLoader）。
已注册进 ctest；**没接硬件时全部用例 `QSKIP`**，因此 CI 上是安全的。

两处设备行为直接影响测试写法：

- `HK16011_Open` 之后的**第一次**采集是冷启动，不出帧 → 每个采集用例先跑一次
  `warmUpCapture()` 预热。
- 复位后首帧偶尔要等数秒 → 帧等待超时给到 15 秒，不用紧超时。

覆盖范围：`enumerate` / 连接与重连 / 非法 id / 断开、参数表形状与逐个定义合法性、
13 个已验证可回读参数各一个用例、只读参数与未实测遥测、越界与未知参数拒绝、
批量语义与关键参数整批拒绝、单帧 / 定量 / 实时采集。

---

## 反模式

- **不要**把调用方给的帧数传给 `HK16011_StartCapture` —— 会稳定丢最后一帧。
- **不要**用有界的 `HK16011_StartFetch` —— `n == 1` 完全不工作。
- **不要**在帧回调里 emit Qt 信号或访问 QObject。
- **不要**读取联合体里与 `type` 不匹配的成员。
- **不要**在 CMakeLists 里写死 SDK 的绝对路径。
- **不要**为那四个占位遥测断言具体数值。
