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

## 采集：唯一真正需要记住的规则

### `ACQ fetch <n>` 要求 n 帧已全部缓存，否则 ERR 5

有界 fetch 只有在设备的 n 帧**都已经进了 DDR3 缓存**之后才会被接受，
否则固件回 **ERR 5（device busy）**。这与曝光速率直接相关：1024×64 下 8 秒
只能缓存 13 帧，所以 `burst 20` 会因等不到 20 帧而被拒。

厂商例程 `examples/capture_burst.cpp` 演示的就是正确写法 ——
`StartCapture` 之后轮询 `HK16011_GetFrameNumReady()` 直到 `>= count`，再 `StartFetch(count)`。
厂商自己的回归测试 `test_acq_fetch_insufficient` 也把这条钉死了
（1 帧缓存时 `ACQ fetch 3` → ERR 5）。**这是设计行为，不是缺陷。**

`ACQ fetch 0`（连续）没有帧数前置条件，任何时候都能用。

### 实测：正确的时序下一切正常

| 序列 | 结果 |
|------|------|
| `StartCapture(n)` → 等 `frame_num_ready >= n` → `StartFetch(n)` | N=1,1,2,3,5 全部 5/5 精确命中 |
| 同上但**不做** RESET | 6/6 命中 —— 不需要 RESET |
| 同上，Open 之后的**第一次**采集 | 3/3 命中 —— 不存在冷启动 |
| `StartCapture(0)` + `StartFetch(0)`，收到 N 帧自停 | 3/3 命中 |

> 这些行曾经被误读成「`ACQ burst` 会丢最后一帧」「`fetch 1` 有固件 bug」
> 「首次采集是冷启动」「EP2 FIFO 里有脏数据」。**都不成立。**
> 真正的共同原因只有一个：早发的 `ACQ fetch` 撞上了还在进行中的采集。
> 额外排空 EP2（512 字节为单位、连读 3 次 50 ms 超时）实测每次读到 0 字节，
> 即 FIFO 里本就没有残留，排空这一步没有贡献。

### 驱动为什么仍然选择「实时 + 自停」

不是因为上面的错误结论，而是因为**有界 fetch 需要阻塞等待**：
要发出 `ACQ fetch n` 就必须先等 `frame_num_ready >= n`，而驱动跑在 GUI 线程上，
一个 20 帧的连拍就是十几秒界面卡死。

所以驱动**无论调用方传多少帧，一律 `StartCapture(0)` + `StartFetch(0)`**，
收到 N 帧后自行 `AbortFetch` + `AbortCapture`。好处是 `startCapture()` 永不阻塞、
首帧能尽快送达，且对任何帧数行为一致。

> 另一条真实存在的约束：`StartFetch` 报 `InvalidArg`（-1）时，几乎都是因为
> `image_width` / `image_height` 没被**写过**。SDK 只从 `SetParamValue` 调用里学几何
> 尺寸，单纯读取不填内部字段。驱动因此在每次开始采集前主动回写一次当前几何值（幂等）。

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

`HK16011_Open()` 用来枚举完全可行，实测也**不会**独占 UART
（pyserial 同时占着 `/dev/ttyACM0` 时 SDK 依然能打开）。唯一站得住的理由是成本：

实测分段 —— USB 打开 ~10 ms、串口打开 ~40 ms、**`fetch_list()` ~310 ms**。
`Open` 会预热参数缓存，即 `LISTPARAMS` + 每个参数一次 `GETINFO`，
共 35 次 UART 往返；115200 波特率下每次往返约 8–10 ms，合计就是那 310 ms。
libusb 直接探测约 1 ms。

`enumerate()` 每次扫描都会执行（启动一次，用户每点一次「Scan Plugins」再一次），
而 `AppController` 跑在 `m_controllerThread` 上，**同时还负责把驱动的
`frameReady()` 转发出去**。阻塞它 365 ms/插件会在实时采集时卡住帧转发，
而 SDK 帧队列只有 4 帧深，扫描一次就会丢帧。

> 注：`PluginLoader::scan()` 完全不处理 `enumerate()` 的失败
> （`src/core/PluginLoader.cpp:106` 直接 `e.cameraIds = driver->enumerate();`，
> `g_loadFailedCallback` 只用于插件**加载**失败）。所以枚举失败对所有驱动都是静默的。
> 用 libusb 探测并不能修好这个静默，只是把失败推迟到 `connectToCamera()`，
> 那里会 `emit errorOccurred` 报出具体错误码。

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

两处测试注意事项：

- 帧等待超时给到 15 秒：1024×64 下曝光本身要几百毫秒，且设备偶有秒级延迟，
  紧超时会让测试变脆。
- 参数往返用例会改动设备状态，务必在结束时把原值写回。

覆盖范围：`enumerate` / 连接与重连 / 非法 id / 断开、参数表形状与逐个定义合法性、
13 个已验证可回读参数各一个用例、只读参数与未实测遥测、越界与未知参数拒绝、
批量语义与关键参数整批拒绝、单帧 / 定量 / 实时采集。

---

## 反模式

- **不要**在 `frame_num_ready` 达到目标帧数之前就发有界的 `ACQ fetch` —— 固件回 ERR 5。
- **不要**在帧回调里 emit Qt 信号或访问 QObject。
- **不要**读取联合体里与 `type` 不匹配的成员。
- **不要**在 CMakeLists 里写死 SDK 的绝对路径。
- **不要**为那四个占位遥测断言具体数值。
