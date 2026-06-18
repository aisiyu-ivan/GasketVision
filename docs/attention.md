# 常见问题与解答

工业垫片视觉质检：**HMI** / **HMI_Test** 做人机界面（双 exe、一套源码），**CameraService** 负责采图，**VisionEngine** 做检测；**三进程独立 exe**，由脚本或 HMI 分离拉起。**`ipc/` 共享内存传图**（Mutex 互斥读写 + 信号量通知新帧）；**Camera↔HMI `socket/camera_hmi/` 本地套接字传 ready/ack、hello/ping 心跳**（不传像素）。

面向 GasketVision 三进程视觉质检项目。架构细节见 [architecture.md](architecture.md)。

---

## 一、架构与进程

### Q1：为什么做成三进程，而不是一个程序？

**答：** 职责分离。**CameraService** 只管采图，**VisionEngine** 只管检测，**HMI** 只管界面与编排。好处：故障隔离（相机或算法崩溃不必拖垮界面）、依赖拆分（HMI 不链 OpenCV）、可独立升级换相机或换算法、采图/检测/显示可并行。工业现场常见「上位机 + 采集 + 算法站」的缩小版。

### Q2：三个进程分别做什么？

**答：**

- **HMI / HMI_Test**：界面、OK/NG 统计；套接字就绪后 **分离拉起** VisionEngine / CameraService（`QProcess::startDetached`，非父子进程）
- **CameraService**：合成样本或 GigE 采图，**直写相机 SHM**（严格模式在发布线程完成 ack→采图→notify）
- **VisionEngine**：浅拷读相机 SHM → 原地检测+标注 → 写 control+notify（**无套接字**）

### Q3：为什么先启动 HMI？

**答：** HMI 需先 **listen `GasketVision.CameraHmi.Control`**，CameraService（严格模式）才能 connect。HMI 就绪后会 **分离启动** Engine 与 Camera（间隔约 2s）；也可只运行 HMI（Qt Creator F5）或双击 **`run.bat`** / **`test/run_test.bat`**（仅启动 HMI，其余由 HMI 拉起）。关 HMI **不会**结束已分离的 Engine/Camera。

**注意：** 正式 **HMI** 与 **HMI_Test** 共用同一套 SHM 与套接字名，**不能同时运行**。切换模式前请先关闭所有 HMI 窗口；启动时会自动结束残留的 VisionEngine / CameraService，再按当前 profile 的工作目录（`build/` 或 `test/`）拉起新链路。

### Q4：每个进程几个线程？

**答：**

| 进程 | 线程 |
|------|------|
| HMI | 2：主线程（事件循环 + UI）+ 通信线程（读相机 SHM 标注 + Camera 套接字） |
| CameraService | 3：主线程（事件循环）+ 采图线程（持续 `grabFrame`）+ 发布线程（写 SHM 前槽位 gate） |
| VisionEngine | 3：主线程（事件循环）+ 通信线程（SHM poll/调度）+ 算法线程（OpenCV 检测） |

检测算法本身未多线程并行；多线程用于 **I/O 与计算分线程、主线程/UI 不卡顿**；严格模式下 **ready/ack 反压** 会在下游慢时 **主动限流**（不是全链路无阻塞）。

---

## 二、进程通信

### Q5：进程之间传什么？怎么传？

**答：** **两层通信**（详见 [architecture.md](architecture.md) §4）：

| 层次 | 路径 | 方式 | 内容 |
|------|------|------|------|
| 数据面 | 各进程 `ipc/` | **一块**四槽相机存储区（SHM）+ **命名 Mutex** + **新帧信号量** | Gray→原地 BGR 标注、OK/NG、测量值 |
| 套接字层 | HMI / CameraService `socket/` | Qt 本地套接字行协议 | **Camera↔HMI ready/ack 槽位反压**；hello/ping |

具体链路：

1. **CameraService ↔ HMI**：`GasketVision.CameraHmi.Control`（`hello`/`ping`/`ready`/`ack`）
2. **CameraService / VisionEngine / HMI**：**同一块相机存储区**（Gray→原地 BGR 标注）

HMI attach **相机 SHM** 浅拷展示标注；**paint+落盘后** 向 Camera 发 **`ack slot releasedFrameId`** 释放槽位。HMI **仅** `captures/{frameId}_annot.png` 落盘。

### Q6：为什么用共享内存而不是 TCP 传图？

**答：** 同机高清图体积大，TCP 需序列化/拷贝/协议开销。SHM 多进程映射同一块内存，配合环形存储区适合高频帧；TCP 更适合跨机或文本协议。本项目历史配置里曾有 `hmiPort:9527`，当前方案已改为 SHM。

### Q7：四槽环形存储区是干什么的？

**答：** 写端按 `frameId % 4` 选槽写入，读端慢于写端时，旧槽尚未读完不会被新帧立刻覆盖，提供约 2～3 帧量级的抗抖动余量，降低丢帧概率。

### Q8：相机路为什么用互斥锁而不是读写锁？

**答：** 当前相机路与 Engine→HMI 路均为 **1 写 1 读**。用 **命名 Mutex** 保证读写 SHM 时互斥、避免脏读；用 **命名信号量** 在 publish 后 **通知** 读端有新帧（**不是**用信号量做互斥）。读写锁适合多读者同时读同一帧；本项目暂无该场景。

### Q9：为什么 SHM 和套接字要分开？

**答：** **SHM 传大图**，**套接字传控制**。像素走共享内存；**Camera↔HMI ready/ack** 协调「何时可覆写存储区槽」，`hello`/`ping` 维护 Camera 连接。控制 **Server/Client 在 `socket/camera_hmi/`**，与 **`ipc/`** 分离。

- **Camera ↔ HMI**（`GasketVision.CameraHmi.Control`）：**`hello`** 连接握手、**`ping`** 约 1s 心跳、严格模式 **`ready <frameId> <slotIndex>` / `ack <slotIndex> <releasedFrameId>`**

**VisionEngine 无套接字**，只读 SHM 做检测。**都不传图像像素。**

### Q9b：CameraService 和 HMI 之间为什么有套接字？

**答：** 严格模式（`strictSampleAccounting: true`）在 **`HMI/socket/camera_hmi/`** 与 **`CameraService/socket/camera_hmi/`** 增加 **`GasketVision.CameraHmi.Control`**，实现 **存储区槽位反压**（数据仍只走 SHM）：

- **CameraService（client，发布线程）**：**写相机 SHM 前** 快路径（本地 `lastAcked`）或慢路径 **`ready slot`** → **等 HMI `ack`** → 再 **`captureFromSource`**
- **CameraService（采图线程）**：按 **`intervalMs` 持续 `grabFrame`**，经信号槽交给发布队列（队列满则丢弃新帧）
- **HMI（server）**：收到 `ready` 且 `frameId ≤ lastReleased + 4` 时 **`ack`**；展示+落盘后也主动 **`ack`** 释放槽

未收到 `ack` 时 **`sendReadyAndWaitAck`**：每 **3s** 重发 `ready`，总超时 **60s**。非严格模式不 connect。

### Q9c：VisionEngine 还需要握手吗？

**答：** **不需要。** Engine 只 poll 读相机 SHM、原地标注、notify；槽位释放由 **HMI → Camera `ack`** 完成，Engine 不参与套接字。

**测试环境**（`strictSampleAccounting: false`）Camera 不 connect HMI，连续推 SHM。

### Q9d：套接字报文 `hello` / `ping` / `ready` / `ack` 各是什么？

**答：**

| 报文 | 通道 | 方向 | 含义 |
|------|------|------|------|
| **hello** | Camera↔HMI | Camera→HMI | 连接成功后上线握手；HMI 不专门回复 |
| **ping** | Camera↔HMI | Camera→HMI | 约 1s 心跳；HMI 15s 无收包断连 |
| **ready** | Camera↔HMI | Camera→HMI | **申请覆写 slotIndex 槽**，写 Gray **之前** 发送 |
| **ack** | Camera↔HMI | HMI→Camera | 该槽已展示+落盘，**允许覆写**；带 `releasedFrameId` |

**无 `done` 报文**（旧 Engine↔HMI / Camera↔Engine 链已移除）。**都不传像素。**

### Q10：启动顺序错了会怎样？

**答：** 若 HMI 未 listen `GasketVision.CameraHmi.Control` 就启动 Camera（严格模式），Camera 控制连接失败。相机 SHM 未就绪时 Engine 读相机会超时空转。VisionEngine **无套接字**，可独立启动。正确顺序：**HMI → listen → VisionEngine / CameraService**（Camera 严格模式需 HMI 先 listen）。

### Q10b：为什么做成 HMI.exe 和 HMI_Test.exe 两个程序？

**答：** 正式（`gige`）与测试（`synthetic`）共用同一套检测链路，但配置与工作目录不同。原先在一个 HMI 里运行时切换模式需要重启子进程、重置 IPC，易卡死且结果易混淆。现改为**编译期 profile**：两个 exe、两个启动脚本，无 UI 切换；CameraService / VisionEngine 仍各一份，由对应 HMI 按工作目录传 `vision_engine.json` 路径。

---

## 三、相机与样品

### Q11：没有真机时怎么调试？

**答：** 双击 **`test/run_test.bat`**（生成样品并启动 HMI_Test，Engine/Camera 由 HMI 分离拉起）。测试配置 `test/vision_engine.json` 中 **`strictSampleAccounting: false`**，合成样品约每 800ms 连续播放，无需 Camera 控制套接字握手。正式环境 `build/vision_engine.json` 保持 **`strictSampleAccounting: true`**。 **HMI.exe 与 HMI_Test.exe 不要同时运行。**

### Q12：样品图是 HMI 读还是 Engine 读？

**答：** **都不是直接读。** 样品由 **CameraService** 的 `SyntheticImageSource` 用 OpenCV `imread` 读取，经 **CameraService → VisionEngine** 的 SHM 交给引擎。HMI 只看到检测后的结果 SHM（或路径回退）。

### Q13：GigE 相机实现了吗？

**答：** **骨架已有**（`GigEVisionCamera`），`grabFrame` 未接完整 SDK。**正式部署**用 `config/vision_engine.json`（`gige`）；**合成联调**用 `test/vision_engine.json`（`synthetic`）。IPC 不变。

---

## 四、检测算法

### Q14：检测流程是什么？

**答：** OpenCV **模板匹配定位** → ROI **径向扫描**测外径/内径及中心 → 与配置 **公差** 比较 OK/NG → 输出 **标注图**及外径、内径、XY 偏心、匹配得分。匹配分低于阈值判 **缺件**。不是霍夫圆或深度学习。

### Q15：为什么用径向扫描而不是霍夫圆？

**答：** 垫片是圆环，在已知 ROI 内沿半径方向找灰度跳变更直接、可控；配合模板匹配先定位，对轻微偏移更稳。答辩时可说「按项目精度与实时性选的工程方案」。

### Q16：标注图存在哪？

**答：** `captures/{frameId}_annot.png`（仅标注图）。标注像素在相机 SHM 同槽；HMI 浅拷 `QImage` 视图落盘。

---

## 五、HMI 与线程

### Q17：Subscriber 和 DataService 有什么区别？

**答：** **CameraIpcSubscriber**（通信子线程）：attach 相机 SHM 读标注、`CameraHmiSocketServer` listen；严格模式下处理 Camera **`ready`** 并 **`ack`**（槽位反压），读 SHM 后 **`emit frameReceived`**；展示+落盘后 **`ack`** 释放槽。**InspectionDataService**（主线程）：收 `frameReceived`、聚合 OK/NG、刷 UI。

### Q18：为什么子线程读 SHM 还要用信号槽？

**答：** Qt 规定 **UI 只能在主线程**更新。子线程 `emit frameReceived`，队列连接到主线程 `onFrame`，避免跨线程直接改 `QLabel` 崩溃或花屏。

---

## 六、构建与部署

### Q19：config/ 和根目录 .bat 是什么关系？

**答：** **`config/`** 是正式环境配置的**源码**（`vision_engine.json`、饼图 JSON、模板）。**`.bat` 不直接读 config**，而是触发 CMake 编译；编译后 CMake 把 `config/` **复制到 `build/`**，运行时程序读的是 `build/` 里的副本。

| 脚本 | 作用 |
|------|------|
| `run.bat` | 正式：编译 + 启动 HMI（Engine/Camera 自动分离拉起） |
| `test/run_test.bat` | 测试：生成样品 + 启动 HMI_Test |

测试配置源码为 **`config/vision_engine_test.json`**；`test/run_test.bat` 每次启动会将其复制到 **`test/vision_engine.json`**（合成样品 + `strictSampleAccounting: false`）。正式配置源码为 **`config/vision_engine.json`**，编译后复制到 **`build/vision_engine.json`**。常用字段：`strictSampleAccounting`（是否严格逐件 ack）、`intervalMs` / `grabIntervalMs`（采图间隔）。改正式参数改 `config/vision_engine.json` 后需重编；改测试参数改 `config/vision_engine_test.json` 后重新运行 `test/run_test.bat` 即可。

### Q20：OpenCV 为什么 HMI 不用？

**答：** HMI 只显示 `QImage` 和统计，图像已在 Engine 侧处理好。不链 OpenCV 可减小依赖、加快启动、降低部署复杂度。

### Q21：MinGW 和 MSVC 的 OpenCV 能混用吗？

**答：** **不能。** 须与 Qt 套件工具链一致。MinGW 工程用 MinGW 编译的 OpenCV；运行时将 `libopencv_*.dll` 放到 `CameraService.exe`、`VisionEngine.exe` 同目录。

### Q22：部署要拷哪些 exe？

**答：** 项目根目录双击 **`run.bat`**（正式）；测试目录双击 **`test/run_test.bat`**。首次会自动 CMake。**HMI.exe 与 HMI_Test.exe 不要同时运行。**

---

## 七、局限与扩展（主动说明加分）

### Q23：当前已知局限？

**答：**

- GigE 未接完整 SDK
- SHM 限同机；跨机需改网络方案
- `strictSampleAccounting: false` 时仍无法严格保证每件样品进统计
- `HMI.exe` 与 `HMI_Test.exe` 不可同时运行（IPC 名冲突）
- 主路径以 **stationId = 1** 为主

### Q23b：如何保证每个样品都进 OK/NG 统计？

**答：** 配置 `strictSampleAccounting: true`（正式默认）。**数据**走 `ipc/`；**写 SHM 前**走 **ready/ack**：

1. **Camera → Engine**：发布线程 **`ready`** → Engine **`ack`** → **publish** 相机 SHM（采图线程持续 grab，与 ack 解耦）
2. **Engine → HMI**：Engine **`ready`**（写标注前）→ HMI **`ack`** → Engine 原地标注 + notify → HMI 读相机 SHM 标注  
3. **HMI → Engine → Camera**：paint + annot 落盘 → **`done(hmi)`** → Engine 转发 **`done(cam)`**
3. Engine 读完相机 SHM 后可提前 **`cam_ack`**，使采图与检测部分重叠  

`sendReadyAndWaitAck` 未收到应答时每 **3s** 重发 `ready`，总 **60s**。关闭严格模式则仅 SHM 推送，无协议保证。

### Q24：如何扩展多工位？

**答：** 架构上已按进程拆分：可加多个 CameraService 实例或扩展 SHM 协议工位字段；`CameraIpc::ControlBlock` 含 `stationId`；HMI `InspectionAggregator` 按工位计数。环形存储区与分进程便于水平扩展。

---

## 八、阻塞与死锁预防

本节汇总 **GasketVision 全链路** 中可能卡住 UI、停住采图、或进程间互相等待的场景，以及 **设计约束与运维习惯**。架构细节见 [architecture.md](architecture.md) §4。

### Q25：本项目里「阻塞」和「死锁」分别指什么？

**答：**

| 概念 | 在本项目中的典型表现 |
|------|----------------------|
| **阻塞** | 某线程在 `waitFor*`、`waitAck`、`waitFrameReady`、`BlockingQueuedConnection`、SHM `lock` 上长时间不返回；界面无响应、样品不再刷新、Camera 停在「等 ack」 |
| **死锁** | 两个及以上线程/进程 **互相等待** 对方释放资源，且无超时退出；例如 A 等 B 的 ack、B 等 A 的 UI 处理完 |

本项目 **刻意避免 UI 线程参与 Blocking 等待**；严格模式下 **Camera 写 SHM 前**、**Engine 标注后通知 HMI 前** 会在 ready/ack 上等待，属 **环形存储区/展示反压**；采图与算法检测与之 **解耦**。

### Q26：进程与 IPC 层面如何预防卡住？

**答：**

1. **不要同时运行 `HMI.exe` 与 `HMI_Test.exe`**  
   二者共用 `GasketVision.*` SHM 名与本地套接字名。双开会导致 attach 失败、读到旧帧、或一方 detach 另一方仍写，表现为「只出一件」「界面假死」「等待 VisionEngine…」循环。

2. **切换正式/测试前先关干净**  
   启动脚本与 `InspectionWindow::stopBackendProcesses()` 会 `taskkill` 残留的 `VisionEngine.exe` / `CameraService.exe`；若手动调试过，任务管理器里确认 **四个 exe 都已结束** 再启动。

3. **工作目录与配置必须成对**  
   - 正式：`build/` + `build/vision_engine.json`（`gige`，`strictSampleAccounting: true`）  
   - 测试：`test/` + `test/vision_engine.json`（`synthetic`，`strictSampleAccounting: false`）  
   **`test/run_test.bat` 必须从 `config/vision_engine_test.json` 复制测试配置**；若误用正式配置，Camera 会走 GigE + 严格 ack，测试环境极易 **只出一帧或长时间无图**。

4. **启动顺序**（见 Q10）  
   HMI listen `CameraHmi.Control` →（约 2s 后）VisionEngine / CameraService。严格模式下 Camera 须 **connect HMI** 后才能 `sendReady`。

5. **SHM 初始化只做一次 attach/detach**  
   `CameraIpcPublisher::initialize()` / `CameraIpcReader::initialize()` 等对残留段 **单次** detach，**禁止** `while (attach) detach` 空转（历史 bug 曾导致 CPU 100% 与假死）。

6. **HMI 切换链路时 reset IPC**  
   `InspectionDataService::resetIpcConnection()` 仅置位标志，由 IPC **子线程** 执行 detach，避免主线程与子线程同时操作 `QSharedMemory`。

### Q27：严格模式（`strictSampleAccounting: true`）下如何预防采图链阻塞？

**答：** 严格模式形成 **Camera ready/ack → 写 Gray SHM** 的反压（数据走 SHM，槽位确认走 Camera↔HMI 套接字）：

```
Camera:  grab(采图线程，持续) → frameGrabbed → 发布队列
         发布线程: [快/慢路径] → captureFromSource(相机 SHM, Gray)
Engine:  读相机 SHM(浅拷) → inspectDispatchAsync(原地标注) → finishInspectPublish(notify)
HMI:     读相机 SHM 标注 → annot 落盘 → paint → ack(slot, frameId) → Camera
HMI:     on Camera ready → ack（槽位未满时）
```

**预防要点：**

| 风险 | 后果 | 预防 |
|------|------|------|
| HMI 未 listen CameraHmi | Camera `waitAck` 失败 | 正式配置 `strictSampleAccounting: true`；HMI 先 `startComm` |
| 长时间收不到 ack | 发布写 SHM 暂停 | **`sendReadyAndWaitAck`** 每 3s 重发 `ready`，总 60s；失败后再 `intervalMs` 重试 |
| Engine 检测过慢 | HMI 展示滞后、Camera 反压 | 采图可继续；Engine 待检队列最多 4 帧 |
| HMI 展示过慢 | Camera 槽位 gate 阻塞 | paint 完成后 **`ack`** 释放槽；pending ready 自动重试 ack |
| 算法 `inspectFrame` 永久阻塞 | 整条链停 | **当前无算法超时**；现场需 taskkill 或后续加看门狗 |

**测试联调**用 `strictSampleAccounting: false`，无 ready/ack，按 `intervalMs` 连续推帧。

### Q28：Qt 线程与信号槽如何避免死锁？

**答：**

| 位置 | 做法 | 禁止 |
|------|------|------|
| **HMI 主线程** | UI 更新、`onFrame`、`InspectionAggregator` | 主线程 `BlockingQueuedConnection` 等通信线程；主线程 `waitFor*` |
| **HMI 通信线程** | `CameraIpcSubscriberWorker` 读相机 SHM + `CameraHmiSocketServer` listen/ack（同线程） | 直接操作 `QLabel` / `QWidget` |
| **frameReceived → UI** | 通信线程 `emit`；主线程 Queued `onFrame` | 主线程读 SHM |
| **VisionEngine 通信线程** | `QTimer` 轮询读 SHM、派发检测 | 不在 `onPollFrame` 内 Blocking 等算法 |
| **VisionEngine 算法线程** | `inspectDispatchAsync` | 完成后 **finishInspectPublish + notify** |
| **CameraService 采图线程** | 按间隔 **`grabFrame`** | **不**调用 `requestCaptureSlot` |
| **CameraService 发布线程** | **`requestCaptureSlot` 后 publish** | 存储区槽满时在套接字上等 ack，采图不阻塞 |

**原则：** UI 线程不参与 Blocking 等待；严格反压下 **会阻塞** 采图/发布/通信线程，属 **故意限流**，不是 UI 卡顿防护失败。

### Q29：SHM 互斥锁与 notify/read 竞态如何预防？

**答：** 相机路与 HMI 路均为 **1 写 1 读 + 互斥锁 + 新帧信号量**。

1. **持锁时间尽量短**  
   锁内只做拷贝/校验；OpenCV 检测、大图 `QImage::copy` 在锁外完成（Engine 算法线程、Publisher 写槽布局）。

2. **所有分支必须 unlock**  
   `readFrame` / `publish` 每个 `return false` 路径都要 `unlock`，否则对端 `lock` 超时（默认 5000ms）后丢帧或假死。

3. **notify 先于 payload 可见的竞态**  
   写端：`lock` → 写槽与 control → `unlock` → `notifyFrameReady`。  
   读端：`waitFrameReady` 返回后若 `readFrame` 失败（`frameId` 未变、槽头不一致），**短窗口内重试**（Engine `waitAndRead` 循环、HMI Subscriber 100ms 重试），避免 **白消费信号量** 导致永久丢帧。

4. **frameId 去重**  
   `m_lastFrameId` 防止重复处理；勿在 reset 后忘记清零，否则新会话首帧可能被拒。

5. **读端 attach 失败**  
   HMI Subscriber 200ms 退避重试 attach，**不**在主线程 spin；Engine `CameraIpcReader` lazy attach，Camera 未起时 poll 超时空转而非崩溃。

### Q30：套接字层哪些调用会阻塞？如何预防？

**答：**

| 调用 | 位置 | 超时 | 预防 |
|------|------|------|------|
| `waitForConnected` | Engine→HMI Client | 可配置 | HMI 须先 listen；Engine `onPingTimer` 断线重连 |
| `waitForBytesWritten(3000)` | 各端写行 | 3s | 对端进程存在且 event loop 运行；避免在已断开 socket 上写 |
| `waitAck` / `sendReadyAndWaitAck` | Camera↔HMI（严格） | 单次 3s，总 60s，超时 **重发 ready** | HMI 须 listen；存储区槽满时暂不回 ack |
| `hello` / `ping` | Engine→HMI | — / 约 1s | 连接与保活；HMI 15s 无包断连 |
| 心跳超时 15s | HMI Server | 断开 client | Engine `onPingTimer` 重连并再发 hello |

**本地套接字全在同一台机器**；不要用阻塞读占满 Camera 单线程 event loop——Camera 主循环在 `waitAck` 内分段 `waitForReadyRead(200)`，仍可能被 strict 模式长时间占满，故测试环境关闭 strict。

### Q31：VisionEngine 三线程分工如何防止通信与算法互相拖死？

**答：**

```
主线程：     exec() 事件循环
通信线程：   poll 读相机 SHM → 待检队列 → Queued inspectDispatchAsync
             onReadyTick → takeDone(HMI) → releaseFrame(Camera) + processReadyRequests
             tryDispatchInspect → sendReadyAndWaitAck(HMI) → inspectDispatchAsync
             onInspectCompleted → finishInspectPublish(notify)
算法线程：   inspectDispatchAsync → emit inspectCompleted
```

- **通信 ↔ 算法**：`QueuedConnection` + `inspectCompleted` 回调，**不 Blocking 占满通信线程**。
- **严格模式**：HMI **ready/ack** 在 **Engine 写标注前**；Camera **ready/ack** 在 **发布线程写相机 SHM 前**；Camera **`done`** 在 **HMI `done` 经 Engine 转发后**。

### Q32：HMI 界面「假死」或长时间无刷新如何预防？

**答：**

1. **子线程读 SHM，主线程刷 UI**（Q17、Q18）；`frameReceived` → `onFrame` 使用 **QueuedConnection**（默认），不在 IPC 线程里 `applyInspectionResult`。
2. **不在主线程做文件对话框/同步 IO 阻塞帧路径**；加载样品图等用户操作与 IPC 路径分离。
3. **通信单线程**：读 SHM 与套接字同在通信线程；严格模式下 **`ready` 时 `ack`**（写许可），`readFrame` 后 **`emit`** 到主线程刷 UI。
4. **防抖**：`flushTimer` 50ms 合并刷新，避免每帧多次重绘 pie/大图。
5. **若整窗无响应**：先查是否 **双 HMI**、**残留 Engine/Camera**、或 **误用正式配置跑测试**（见 Q26），再查任务管理器 CPU 是否 100%（SHM attach 死循环）。

### Q33：构建、部署与运维上如何避免「Permission denied」与链路半启动？

**答：**

| 现象 | 原因 | 预防 |
|------|------|------|
| 链接 `Permission denied` | exe 仍在运行 | 编译前 `taskkill` 四个进程 |
| Engine/Camera 起不来 | 工作目录无 `vision_engine.json` | 正式从 `build/` 启动 HMI；测试从 `test/` 启动 HMI_Test |
| 只编译了 HMI 未编 Engine | 旧 Engine 行为与 HMI 不匹配 | `run.bat` / `run_test.bat` 会编齐目标 |
| 关 HMI 后 Engine/Camera 仍在 | 分离启动设计 | 切换模式前手动结束或重新开 HMI（会 kill 后再拉） |

### Q34：现场快速排查清单（按顺序）

**答：**

1. 任务管理器：是否 **仅一个 HMI** + 至多各一个 VisionEngine / CameraService？  
2. `test/vision_engine.json`：`imageSource` 是否为 `synthetic`，`strictSampleAccounting` 是否为 `false`（测试）？  
3. HMI 状态栏：是否出现「本地套接字已就绪」「VisionEngine 已连接」？  
4. 严格正式模式：无新样品 → 查 Camera 是否在 **`sendReadyAndWaitAck`**、Engine 是否在 **等 HMI ack** 或 **算法阻塞**。
5. 测试模式仍只出一帧：确认 `test/vision_engine.json` 为 **`synthetic` + `strictSampleAccounting: false`**（`run_test.bat` 应从 `config/vision_engine_test.json` 复制）。
6. 仍异常：`taskkill /F /IM HMI.exe /IM HMI_Test.exe /IM VisionEngine.exe /IM CameraService.exe` 后冷启动。

**设计总纲（改代码时自检）：** 数据面（SHM + Mutex + 信号量）；控制面 **ready/ack 在写 SHM 前**、`sendReadyAndWaitAck` 带重试；UI 线程不参与 Blocking；严格模式 **允许业务线程反压等待**，与「UI 不卡」目标分离。

---

## 五、项目使用的容器与算法（汇总）

面向面试/自查：本项目 **以 Qt 容器为主**，STL 序列/关联容器用得很少；**没有**使用 `std::stack`、`std::deque`、`std::list`、`std::map`、`std::set` 等。检测主链路额外用 **固定长度 C 数组**（IPC 控制块）和 **4 槽环形存储区**（非 `std::` 容器）。

### 5.1 序列容器

| 类别 | 类型 | 主要用途 | 代表位置 |
|------|------|----------|----------|
| **Qt 动态数组** | `QList<T>` | 扇区角、颜色、饼图指针、协议拆行后的字段 | `SinglePieChart`、`PieChartConfig`、`CameraHmiProtocol` |
| **Qt 字符串列表** | `QStringList` | 样品路径列表、目录扫描、饼图变量名 | `SyntheticImageSource`、`InspectionWindow::validateTestAssets` |
| **Qt 队列（FIFO）** | `QQueue<VisionFrame>` | 采图→发布待写队列；通信→算法待检队列 | `CameraPublishWorker`、`StationCommWorker` |
| **STL 动态数组** | `std::vector<T>` | 径向扫描半径样本；饼图外部灌入 `VarCountData` | `GasketInspector::measureRing`、`Pie_Chart_Control/widget` |
| **定长 C 数组** | `char path[260]` 等 | IPC `ControlBlock` 内路径/状态/缺陷文本 | `CameraIpcLayout.h` |
| **环形缓冲（布局）** | 4 槽 ×（头 + 像素区） | 相机 SHM，非 `std::` 容器，`frameId % 4` 选槽 | `CameraIpc::kRingSlots` |
| **字节缓冲** | `QByteArray` | 套接字读缓冲、行协议编解码 | `SocketLineCodec`、`CameraHmiSocketClient` |
| **图像矩阵** | `cv::Mat` | 采图、检测、SHM 浅拷视图 | `VisionFrame`、`GasketInspector` |
| **显示图像** | `QImage` | HMI 标注图（深拷贝后上屏） | `InspectionResult`、`ShmImageLabel` |

**未使用：** `std::array`、`std::deque`、`std::list`、`QVector`（全项目未用）、`QStack`、`QLinkedList`。

**队列操作：** `enqueue` / `dequeue` / `prepend`（发布失败时回队首重试）。

### 5.2 关联容器（映射 / 集合）

| 类别 | 类型 | 主要用途 | 代表位置 |
|------|------|----------|----------|
| **哈希映射** | `QHash<K,V>` | OK/NG 计数、工位→计数、变量名→编辑框指针 | `InspectionAggregator`、`OkNgStatsPanel`、`PieChartConfig` |
| **嵌套哈希** | `QHash<int, QHash<QString,uint>>` | 按工位累计 OK/NG | `InspectionAggregator::m_perStation` |
| **有序映射** | `QMap<QString, QStringList>` | 按缺陷类别分组样品路径 | `SyntheticImageSource::interleaveByCase` |
| **哈希集合** | `QSet<QString>` | 饼图变量名去重 | `PieChartConfig`、`PieChartConfigDialog` |

**未使用：** `std::map`、`std::unordered_map`、`std::set`、`std::unordered_set`、`QMultiMap`、`QMultiHash`。

### 5.3 适配器与其它「容器式」类型

| 类型 | 用途 |
|------|------|
| `std::unique_ptr<IVisionImageSource>` | 采图源工厂创建，独占所有权 |
| `QSharedMemory` | 跨进程共享存储区 attach/create |
| `QMutex` + `QMutexLocker` | 发布队列、OK/NG 聚合器互斥 |
| `std::atomic<bool>` | HMI 订阅线程运行/重置标志 |
| `QJsonArray` / `QJsonObject` | 配置解析（非业务容器，只读配置树） |

### 5.4 算法（查找 / 排序 / 遍历 / 统计）

#### STL / `<algorithm>`

| 算法 | 用途 | 位置 |
|------|------|------|
| **`std::sort`** | 变量名字典序排序 | `PieChartConfigDialog::sortedUniqueNamesFromEdits` |
| **`std::nth_element`** | 径向半径样本取**中位数**（O(n) 部分排序） | `GasketInspector::measureRing` |

**未使用：** `std::find` / `find_if`、`binary_search`、`lower_bound`、`partition`、`stable_sort`、`std::accumulate` 等。

#### Qt / 业务遍历

| 方式 | 用途 |
|------|------|
| `QStringList::sort()` | 每组样品文件名排序 |
| `QDir::entryList(..., QDir::Name)` | 扫描 `station1/*.png` |
| `for (const T &v : std::as_const(m_sectorData))` | 饼图扇区遍历 |
| `QQueue` 出队入队 | 帧流水线调度 |
| `qMax` / `qMin` | 超时、布局、字符串截断上界 |
| `QString::split` + `parseReady` / `parseAck` | 套接字行协议解析（按空格拆字段） |

#### OpenCV / 视觉算法（`GasketInspector`）

| 类别 | API / 逻辑 |
|------|------------|
| **查找（匹配）** | `cv::matchTemplate` + `cv::minMaxLoc`（模板相关峰） |
| **遍历（测径）** | 0°–358° 每 2° 径向射线扫描；ROI 内像素遍历 |
| **滤波** | `cv::GaussianBlur` |
| **分割** | `cv::threshold`（固定阈值 120） |
| **矩 / 质心** | `cv::moments`、`cv::countNonZero` |
| **几何** | `std::cos` / `std::sin`、`std::hypot`、`std::abs`、`std::round` |
| **绘制** | `cv::circle`、`cv::putText`、`cv::cvtColor` |

#### 业务调度（非 STL，属流水线算法）

| 逻辑 | 说明 |
|------|------|
| **环形槽反压** | `frameId % 4`；严格模式 `ready/ack` 或本地 `lastAcked` 门控 |
| **等待标注** | `waitForAnnotated` 轮询 `ControlBlock.flags` |
| **样品交错播放** | `interleaveByCase`：按 ok / 各 NG 类型轮询出队 |
| **饼图扇区角分配** | 按 `count/total × 360°` 累加起始角（`QPainter::drawPie`） |

#### Python 测试脚本（`test/scripts/`）

| 脚本 | 容器 | 算法 |
|------|------|------|
| `generate_samples.py` | `dict`（manifest）、`list`、`bytearray` 像素缓冲 | 双重 `for` 画矩形/圆；PNG zlib 压缩；按权重分配样品数量 |
| `diagnose_samples.py` | `list`、`dict`、`defaultdict` | 与 C++ 类似的径向扫描；**`sorted` 取中位数**；按 case 统计遍历 manifest |

### 5.5 小结（面试一句话）

- **容器：** 帧队列用 **`QQueue`**，统计用 **`QHash`**，样品分组用 **`QMap`**，测径临时数据用 **`std::vector`**，IPC 用 **定长数组 + 4 槽环**，图像用 **`cv::Mat` / `QImage`**。  
- **算法：** 唯一 STL 标准算法是 **`std::sort`**（饼图配置）和 **`std::nth_element`**（测径中位数）；视觉侧是 **模板匹配 + 径向扫描 + 阈值分割**；流水线侧是 **环形缓冲 + ready/ack 反压**。

---

## 六、合成样品图基础信息（我对图像的理解）

测试链路读的是 `test/station1/*.png`，由 **`test/scripts/generate_samples.py`** 生成；每张图的「真值」写在 **`test/manifest.json`**。以下说明 **图像里有什么、物理量怎么对应像素、算法为什么能检**。

### 6.1 图像文件与标定

| 项目 | 值 | 说明 |
|------|-----|------|
| 格式 | **8 位灰度 PNG** | 单通道，与 Camera 发布 **Gray8**、Engine 读灰度一致 |
| 分辨率 | **640 × 480** | 与 `vision_engine.json` 中 `imageCenterPx: [320, 240]` 匹配 |
| 像素比例 | **10 px/mm** | `pxPerMm`，1 mm 对应 10 像素 |
| 工位 | **station1** | 文件名 `{case}_{序号}.png`，如 `ok_00.png` |
| 定位模板 | **32×32** `templates/fiducial_L.png` | 从全图左上角 `(30,30)` 裁切的 L 形角标 |

**尺寸换算示例：** 标称外径 12 mm → 直径 **120 px**（半径 60 px）；内径 8 mm → 直径 **80 px**（半径 40 px）。

### 6.2 场景构成（由底到顶）

合成图不是照片，而是 **按几何规则绘制的灰度场景**，层次固定：

```
┌──────────────────────────────────── 640×480 ────────────────────────────────────┐
│  ■ L 形角标 (30,30)  灰度≈220          ← 模板匹配用 fiducial_L                    │
│                                                                                  │
│              ╭── 工装圆 灰度≈55（标称外径+6mm 的细环，仅示意）──╮                  │
│              │     ╭════ 垫片亮环 灰度≈205 ════╮               │                  │
│              │     ║   外径 OD / 内径 ID       ║  ← 检测目标    │                  │
│              │     ╚═══════════════════════════╝               │                  │
│              ╰──────── 图像中心 (320,240) ────────╯                              │
│  背景填充灰度≈38（整幅先铺底）                                                    │
└──────────────────────────────────────────────────────────────────────────────────┘
```

| 图层 | 灰度值 | 作用 |
|------|--------|------|
| 背景 | **38** | 暗场，与亮垫片、角标对比 |
| 工装圆 | **55** | 细圆描边，半径 ≈ `OD/2 + 6mm`，模拟夹具轮廓（不参与 OK/NG 判定） |
| L 形角标 | **220** | 24×4 + 4×24 像素直角，用于 **`matchTemplate` 缺件判断** |
| 垫片环带 | **205** | 环形区域：`inner_r ≤ 距离中心 ≤ outer_r` 的像素置亮 |

缺件类 **`missing`** 只画背景和工装，**不画亮环** → 匹配分低或测不到环，算法应判 **NG / 缺件**。

### 6.3 五类样品与物理含义

与 `vision_engine.json` 中 **标称 + 容差** 对照（检测端 `GasketInspector` 使用同一套数）：

| 标称 | 值 |
|------|-----|
| 外径 OD | 12.0 mm |
| 内径 ID | 8.0 mm |
| 外径容差 | ±0.10 mm |
| 内径容差 | ±0.08 mm |
| 偏心容差 | 0.12 mm（合位移） |

| case 前缀 | 预期 | 生成时 OD/ID/偏心（约） | 对应缺陷 |
|-----------|------|-------------------------|----------|
| **ok_** | OK | OD≈12.01 mm，ID≈8.01 mm，偏心 &lt;0.03 mm | 均在容差内 |
| **od_oversize_** | NG | OD≈**12.25 mm**（超外径容差） | 尺寸超差 |
| **id_undersize_** | NG | ID≈**7.75 mm**（超内径容差） | 尺寸超差 |
| **eccentric_** | NG | OD/ID 合格，偏心 **(0.15, 0.12) mm** | 偏心偏移 |
| **missing_** | NG | 不画垫片环 | 缺件 |

同类别多张图之间用 **seed 微扰**（`seed = 100 + 序号`）：OD ±0.005 mm 级、ID ±0.004 mm、偏心 ±0.002 mm，避免每张完全相同，更贴近「连续生产略有波动」。

### 6.4 与检测算法的对应关系

1. **缺件 / 有无垫片**  
   - 角标模板 `fiducial_L.png` 与全图做 **归一化互相关**；`matchScore < 0.45` → 缺件。  
   - `missing` 类虽仍有角标，但无亮环 → 后续 **测径失败** 也会归缺件。

2. **测径**  
   - 以配置中心 **(320, 240)** 为基准，在 ROI 内 **阈值 120 二值化** → 径向扫描 → **中值** 得内外径（mm）。  
   - 与 `manifest.json` 里 `outerDiameterMm` / `innerDiameterMm` 可对照验收。

3. **偏心**  
   - 亮区质心相对 **(320, 240)** 的偏移 ÷ `pxPerMm` → `offsetXMm` / `offsetYMm`，再与 `offsetTolMm` 比。

4. **标注图（BGR）**  
   - 算法在原图同槽画 **外圆（绿）、内圆（青）、基准点（蓝）、OK/NG 文字**；HMI 只显示这张标注图。

### 6.5 manifest.json 与播放顺序

**manifest 每条 entry 字段：**

| 字段 | 含义 |
|------|------|
| `imagePath` | 相对 `test/` 的路径 |
| `stationId` | 工位号（当前均为 1） |
| `case` | 缺陷类别键 |
| `expectedOk` | 生成脚本认定的期望 OK/NG |
| `outerDiameterMm` / `innerDiameterMm` | 该张图合成时的标称尺寸（缺件为 null） |

**数量分配（`--total N`）：** 按权重 **OK 60%**、四种 NG 各 **10%**（权重 6:1:1:1:1），避免演示时 NG 过多。

**播放顺序：** `SyntheticImageSource::interleaveByCase` 按 **ok → od_oversize → id_undersize → eccentric → missing** 交错轮播，避免长时间连续同类缺陷。

### 6.6 生成与使用命令

```bat
cd test
python scripts\generate_samples.py --total 20    REM 少量联调
python scripts\generate_samples.py --total 1000  REM run_test.bat 默认
python scripts\diagnose_samples.py               REM 对照 manifest 验收算法与样品是否一致
```

**面试可强调：** 样品图不是随意截图，而是 **按 px/mm 标定、按容差边界设计五类 case**；角标负责 **有无工件**，亮环负责 **尺寸与偏心**，manifest 提供 **可回归的 ground truth**，与 `GasketInspector` 的模板匹配 + 径向测径链路一一对应。
