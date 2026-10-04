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

**参数总量 35 个**，由 `LISTPARAMS` + 逐个 `GETINFO` 在 `HK16011_Open` 时探测并缓存。

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
>
> 上表是**有界 fetch** 的实测。它成立过，也曾因此成为驱动唯一的可用路径；
> 固件修好连续 fetch 之后（见下）驱动不再走这条路，这张表只作为对照。

### 驱动为什么一律用连续 fetch

因为**有界 fetch 需要阻塞等待**：要发出 `ACQ fetch n` 就必须先等
`frame_num_ready >= n`，而驱动跑在 GUI 线程上，一个 20 帧的连拍就是十几秒界面卡死。

驱动沿用设备自己的采集模式（0 = live、1 = single、≥2 = burst），
但**不论哪种模式一律 `StartFetch(0)`**，在第一帧曝光之前就发起；
有界帧数由 `deliverQueuedFrames()` 发满 N 帧后自行
`AbortFetch` + `AbortCapture` 收尾。`startCapture()` 永不阻塞，首帧能尽快送达。

> **连续 fetch 对 single / burst 的丢帧已经修好**（固件 `ccd.c` v1.13）。
> 曾经的症状是 single 一帧都不回、burst 恰好丢最后一帧，规律是「发起 fetch 时
> 采集是否已结束」。根因：`FRAME_WRITTEN` 里「有新帧就发」的触发块原本挂在采集环
> 继续推进的路径末尾，single 与 burst 收尾的**提前 `return` 从上方绕过了它**。
> 修法是用 `keep_capturing` 标志消掉提前 `return`，让触发块收在唯一出口，
> 发送与采集环是否收尾彻底解耦。
> 所以现在「有帧即读」是安全的，也是三种模式唯一共用的策略。

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
| `Enumeration` | `StringCollection` | `validValues` 放 **label**，设备只认 token —— 见下 |
| `String` | `String` | |

### 枚举：label 是对外的值，token 是对上设备的值

`validValues` 填的是 `data.s.label`（`Image`、`Idle`…），不是 `data.s.set`
（`image`、`idle`…）。原因不是好看：`validValues` 同时是**下拉框的数据源**、
**校验集合**和**参数当前值**的取值域，三者必须是同一个东西。

这一点如果只改 `validValues` 会立刻出问题：`def.defaultValue` 在固件不报默认值时
回退到「设备当前值」，而设备返回的是 token；`ParameterWidgetFactory::createEnumWidget()`
用 `findData(defaultValue)` 选中项，token 在 label 列表里查不到，
`setWidgetValue()` 里的 `if (index >= 0)` 于是**静默不生效** —— 下拉框停在第 0 项，
用户点确定后 `getWidgetValue()` 把错误值写回去，相机自己换了工作模式。

所以驱动维护两张映射（`m_enumLabelByToken` / `m_enumTokenByLabel`），把值统一到 label：

| 方向 | 位置 |
|------|------|
| 读设备 → 对外 | `readValue()` 经 `toShownValue()` 把 token 换成 label |
| 写入 → 暂存 | `setParameter()` / `setParameters()` 先 `toShownValue()` 再校验、暂存 |
| 暂存 → 上设备 | `fromVariant()` 把 label 换回 token；认不出来的字符串按原样下发 |

**token 仍然可以传进来。** 改之前存下的 `.ini` 里是 `string:image`，照着线上名字写的
CLI 脚本同理；这些都要继续能用，所以 `setParameter` 收 token、只是归一化后再暂存。
`fromVariant` 对认不出的字符串按原样当 token 处理，保证 token 绝不会原样进 GUI、
也绝不会以 label 的形态进设备。

没有 label 的枚举项（`freq_sel` 的 `100k` / `500k`）直接显示 token 本身。

### 描述与分类由插件持有（SDK 不再上报）

SDK 曾经在 `HK16011_ParamDefStruct` 里带 `category`，其 `description` 也一直是固件自己
写的文案，两处现均改由插件提供。原因是它们都是**第二份事实来源**：固件一改就会和
EZSpecCam 漂移，与其让两份对不上，不如只留一份。`displayName` 仍取自 SDK。

现在唯一给 hk16011 参数写描述和归组的地方是静态表 `hk16011ParameterMetadata()`，
这张表同时是**白名单**，由 `applyParameterMetadata()` 按「先查后定」的方式套用：

| 方向 | 行为 |
|------|------|
| 固件有、静态表没有 | `qInfo` 点名后**不暴露**，从参数表和值表一并删除 |
| 静态表有、固件没有 | `qInfo` 点名后跳过，不报错 |
| 静态表没写描述 | `qInfo` 告警并回退到 SDK 的文案（`isValid()` 不接受空描述） |

前两条是为了固件增删参数时能立刻看见，而不是让 GUI 里悄悄多出或少掉一个控件；
第三条是因为 `ParameterDefinition::isValid()` 会把空描述判为非法，那样参数就直接消失了。

分组内部的顺序不用写：`order` 保持 LISTPARAMS 下标，GUI 按它排，
于是每组自然沿用固件自己的顺序。

| 类别 | 参数 |
|------|------|
| Core | `exposure_time_us` `read_mode` `freq_sel` `adc_gain_r` `adc_gain_g` `adc_gain_b` `adc_offset_r` `adc_offset_g` `adc_offset_b` |
| Cooling | `tec_enable` `tec_set_temp` `sensor_temp` `environment_temp` `tec_voltage` `tec_current` |
| Info | `camera_name` |
| Advanced | `tec_kp` `tec_ki` `tec_kd` `mon_dwell_ms` |
| Debug | `mock_mode` `cdsclk_delay` `image_width` `image_height` `bevel_left` `bevel_top` `bevel_right` `bevel_bottom` `blank_left` `blank_right` `acq_state` `frame_num_ready` `frame_capacity` `exception_flag` `exception_cnt` |

**只读 ≠ Info。** `sensor_temp` 等四个遥测虽然只读，但归 `Cooling` —— 它们和
`tec_set_temp` 是同一件事的设定端与读数端。归组看的是参数**拿来干什么**。

**adc_\* 与 `mon_dwell_ms`（2026-10 固件改版）。** 固件把六个 adc 参数从整型码值
改成了物理量浮点：增益 [1, 6] V/V、偏移 [-300, 300] mV，描述文案随改；同时新增
`mon_dwell_ms`（ADS1118 四通道遥测轮询的通道切换稳定延时，四通道一圈 =
4×此值，下次轮转才生效）。它 pacing 的虽然是那四个 Cooling 遥测，但作为
监控环的调节旋钮归 **`Advanced`**（与 `tec_kp/ki/kd` 同组）。

### isDynamic / isExtrinsic 与 parameterValue() 的实读

`hk16011ParameterMetadata()` 同时给每个参数标注 `isDynamic` / `isExtrinsic`。
四个 Cooling 遥测和 Debug 的 `acq_state` / `frame_num_ready` / `exception_flag` /
`exception_cnt` 只读且会自行变化，两个标志都给 —— CameraTab 对
`isReadOnly && isDynamic && isExtrinsic` 的参数每 100 ms 轮询一次；
`camera_name` / `frame_capacity` 恒定不变，两个都不给，轮询没有意义。

`parameterValue()` 对这两类参数**每次调用都回设备实读**（与 PicamDriver /
HamamatsuDriver 同一契约），实读成功就更新缓存，失败回落到缓存。代价是
每次 GETPARAM 都是 ~9 ms 的 UART 往返：GUI 轮询的 tick 最多会被 8 个参数
阻塞掉大半，读遥测期间也会短暂占住 `m_mutex`、推迟 `deliverQueuedFrames()`
—— 这是接受的取舍，**不要**在这里再加缓存或限频。

### `exposure_time_us` 的单位下拉

量程是 `[1, 2147483647] µs`，一个光秃秃的 `QSpinBox` 谁也没法手动输入，所以静态表
给它带上 `units = {us, ms, s}` 和 `unitScale = {1000, 1000000}`。

注意 `unitScale` 是**相对基准单位的累计倍率**，不是相邻档位之间的倍率。
`ParameterConstraint::getUnitIndex()` 拿它当阈值比较，`toDisplayValue()` 拿它当除数，
只有按累计理解两者才自洽：写 `{1000, 1000}` 会把 2000 µs 显示成 `2 s`。

另外，**单独一个 `unit` 字符串在 GUI 上是看不见的** ——
`ParameterWidgetFactory` 只有在 `unitRange` 同时非空（`hasUnitRange()`）时才渲染单位
下拉。raw 值始终是 µs，设备侧和 CLI 输出都不受影响。

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

## enumerate() 用 HK16011_Open

SDK 没有独立的发现接口，所以 `enumerate()` 直接 `HK16011_Open()` 再
`HK16011_Close()`，相机 id 取自 `HK16011_GetDeviceId()`。

代价是 **~365 ms/次**。实测分段：USB 打开 ~10 ms、串口打开 ~40 ms、
**`fetch_list()` ~310 ms**。`Open` 会预热参数缓存，即 `LISTPARAMS` + 每个参数
一次 `GETINFO`，共 35 次 UART 往返；115200 波特率下每次往返约 8–10 ms。

需要知道这个开销的几个位置：

- `enumerate()` 每次插件扫描都会执行（启动一次，用户每点一次「Scan Plugins」
  再一次），而 `AppController` 跑在 `m_controllerThread` 上，**同时负责把驱动
  的 `frameReady()` 转发出去**。实时采集中触发扫描会卡住帧转发，而 SDK 帧队列
  只有 4 帧深，扫一次就可能丢帧。
- 测试里每次 `connectAndGetId()` 都要付一次这 365 ms，全套 43 个用例约 45 秒。

**另一个必须知道的副作用**：打开失败时相机会**直接从列表里消失**，
而 `PluginLoader::scan()` 完全不处理 `enumerate()` 的返回值
（`src/core/PluginLoader.cpp:106` 直接 `e.cameraIds = driver->enumerate();`，
`g_loadFailedCallback` 只用于插件**加载**失败）。也就是说枚举失败对所有驱动都是
静默的 —— 串口桥没插好、权限不足（不在 `dialout` 组）等情况都表现为「相机不见了」，
界面不给任何原因。这是本驱动接受的行为：要拿到确定答案就得走 SDK，
而 SDK 的 `Open` 同时校验 USB 设备和 UART 桥。

> 曾经试过用 libusb 直接探测 VID/PID 绕开这段开销（约 1 ms），但那样只能确认
> USB 在位、确认不了 UART 桥，反而更容易出现「列出来了却连不上」。

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

测试的 `CMakeLists.txt` 会**复用** `src/plugins/hk16011` 已经解析好的
`HK16011_SDK_ROOT`（那个子目录先被 `add_subdirectory`），而不是自己从
`$ENV{HK16011_ROOT}` 重新推导一遍。早期版本只认自己的 `HK16011_TEST_SDK_ROOT`
和环境变量，于是只要 SDK 是靠 `-D` 或上一轮 configure 遗留在 cache 里的值定位的，
测试 target 就会静默地 `return()` 掉 —— 插件照常编译，测试压根不存在，
`ctest` 里也看不到，只剩一个过期的旧二进制躺在 `bin/Debug/` 里。

覆盖范围：`enumerate` / 连接与重连 / 非法 id / 断开、参数表形状与逐个定义合法性、
逐类别归属（Core/Cooling/Info/Advanced/Debug 各取代表，外加六个 `adc_*`）、
枚举的 label 与 token 双向换算、枚举写入是否真的到达设备（靠重连后复读）、
`exposure_time_us` 单位换算、只读与 dynamic/extrinsic 标志、
14 个已验证可回读参数各一个用例、只读参数与未实测遥测、越界与未知参数拒绝、
批量语义与关键参数整批拒绝、单帧 / 定量 / 实时采集。共 49 项。

---

## 反模式

- **不要**在 `frame_num_ready` 达到目标帧数之前就发有界的 `ACQ fetch <n>`（n ≥ 1）—— 固件回 ERR 5。
  这条容量校验**仍然有效**，只是驱动已经不用有界 fetch 了（见上）。
  要判断丢帧是不是这条规则造成的，先确认驱动走的是 `StartFetch(0)`。
- **不要**把「连续 fetch 在 single/burst 下丢帧」当成固件缺陷 —— 已修（`ccd.c` v1.13）。
  旧结论（burst 恰好丢最后一帧、single 一帧不回）是 v1.13 之前的现象；
  现在三种模式都用 `StartFetch(0)`，有帧即读。
- **不要**在帧回调里 emit Qt 信号或访问 QObject。
- **不要**读取联合体里与 `type` 不匹配的成员。
- **不要**让 `HK16011_ValueStruct` 指向一个活不过本次调用的缓冲区。
  `data.s.set` 是 `const char *`，如果它指向 `fromVariant()` 内部的局部 `QByteArray`，
  函数一返回就悬垂，SDK 随后会把**已释放的内存**格式化进命令里。真实踩过的表现是
  下发 `SETPARAM read_mode read_mode`，固件回 ERR 3 —— 而同样的值用探针程序（传的是
  静态字符串）永远成功，因为静态存储期不会失效。缓冲区由调用方持有
  （见 `writeValue()` 里的 `QByteArray storage`）。
- **不要**只把 `validValues` 换成 label 而不同时归一化读回来的值 ——
  下拉框会静默停在第 0 项，并把错误值写回去。
- **不要**在 CMakeLists 里写死 SDK 的绝对路径。
- **不要**为那四个占位遥测断言具体数值。
- **不要**指望设备替 EZSpecCam 决定分类 —— 那份分类已经从 SDK 里删掉了，
  新增参数要自己往 `hk16011ParameterMetadata()` 里加，否则它不会出现在界面上。
