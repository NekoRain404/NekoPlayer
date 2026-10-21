# NekoPlayer 工程分层

本阶段使用 C++20 / Qt 6 Widgets / 单一 libmpv；参考 PotPlayer 的紧凑桌面交互。

| 组件 | 职责 | 依赖边界 |
| --- | --- | --- |
| `MpvController` | client RAII、异步命令、属性观察、文件事件、播放状态和渲染恢复快照 | Qt Core / libmpv client API，不依赖窗口 |
| `PlaybackState` | 只读播放快照；秒为时间单位，音量 0–100 | 控制器与展示层共用 |
| `PlaylistModel` | 本地文件队列与当前项；提供 Qt model 角色 | Qt Core，不直接控制播放器 |
| `PlaylistController` | 连接队列、播放控制器和 EOF/加载失败事件 | 仅依赖 `MpvController` 与 `PlaylistModel` |
| `VideoWidget` | GL 上下文、物理像素 FBO、帧更新、隐藏时跳帧、资源释放 | 只处理渲染，不提交播放命令 |
| `TransportBar` | 展示时间/进度/音量，拖动预览，发出定位和音量意图 | 状态快照和 QAction，不操作 mpv |
| `PlaylistPane` | 队列视图和上一项/下一项/清空动作入口 | 只依赖 model 与共享 QAction |
| `MainWindow` | 组件组装、文件入口、错误显示、拖放、全屏和生命周期协调 | 通过队列和控制器接口发出业务操作 |
| `windowactions.cpp` | 同一 MainWindow 类的动作与菜单装配 | 菜单、按钮、快捷键共用 QAction |
| `main.cpp` | 参数解析、启动和 smoke 退出条件 | 后端诊断实现单独保留 |

业务路径：用户文件入口 → `PlaylistController` 选择有效本地项 → 控制器异步命令 → mpv 事件 → Qt 接收者队列 → 状态信号 → 展示更新。EOF 自动选择下一项；加载失败只跳过后续项，不将一般 mpv 错误误认为队列切换。渲染路径独立：mpv 更新回调 → Qt 队列 → GL current → render API → Qt 合成。

后台回调只排队，不访问 UI、不调用 mpv。带 QObject 接收上下文的排队任务在对象销毁时取消。控制器和表面各自的回调 userdata 保留到对应 mpv 资源完全释放。

销毁顺序：停止窗口定时器 → 删除 central widget / VideoWidget → 撤销 render 回调 → GL current 下释放 renderer → 撤销 client 回调 → 销毁 client。主窗口显式删除 central widget，避免 QMainWindow 基类析构晚于控制器成员的问题。

上下文重建：表面发出即将重建信号 → 控制器保存当前文件、位置和暂停状态 → renderer 释放 → 新 GL context 创建 renderer → 控制器异步重新加载文件并恢复状态。正常全屏切换不重新挂载视频控件；测试通过跨窗口重新挂载主动触发 context 重建。

普通手写代码文件最多 500 行（含头文件、测试、CMake 和测试启动脚本）。`src/ui/mainwindow.cpp` 和 `tests/window_test.cpp` 是已登记的集中协调/桌面集成例外，最多 800 行；例外列表由 `cmake/CheckCodeSize.cmake` 强制执行，新增项必须有明确理由。新增职责拆成独立组件；同一类的紧密相关装配代码可以分文件，不为拆文件增加无用途的抽象。保持 Qt parent ownership 与 RAII 的资源归属唯一。

本阶段有内存播放列表，但没有队列持久化、拖放重排、随机/循环模式、配置持久化、插件框架、多引擎或完整主题系统。
