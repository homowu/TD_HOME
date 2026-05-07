===========================================================================
           SOLIDWORKS 批量 FBX 转换器 v1.0
===========================================================================

功能说明：
----------
本工具可以将 SOLIDWORKS 文件（.sldprt, .sldasm）批量转换为 FBX 格式。
程序会后台调用 SOLIDWORKS 2024 COM API 执行转换，无需手动打开 SOLIDWORKS。

系统要求：
----------
1. 操作系统：Windows 10 / Windows 11 (64位)
2. 必须安装 SOLIDWORKS 2024（已安装 COM 组件）
3. .NET Framework 4.8 或更高版本（通常已预装）

使用方法：
----------
1. 双击运行 SolidWorksBatchConverter.exe
2. 点击「浏览...」选择包含 .sldprt/.sldasm 文件的输入目录
3. 点击「浏览...」选择输出目录（FBX 文件将保存到此处）
4. 点击「开始转换」按钮
5. 等待转换完成，可查看实时进度和日志

注意事项：
----------
- 首次运行可能需要等待 SOLIDWORKS 启动（约10-30秒）
- 转换过程中不要手动操作 SOLIDWORKS
- 日志文件保存在输出目录的 logs 子文件夹中
- 单个文件转换失败不会中断整个批处理

目录结构：
----------
SolidWorksBatchConverter/
├── SolidWorksBatchConverter.exe    (主程序)
├── SolidWorksBatchConverter.exe.config
└── logs/                           (日志目录，运行后自动创建)

常见问题：
----------

Q1: 运行时提示 "Class not registered" 或找不到 SolidWorks COM
A: 请确认已正确安装 SOLIDWORKS 2024，且安装路径正确。

Q2: 提示权限不足
A: 请尝试右键以管理员身份运行程序。

Q3: 转换完成后任务管理器中仍有 SLDWORKS.exe 进程
A: 正常情况下程序退出时会关闭 SOLIDWORKS。如果残留，可手动结束进程。

Q4: FBX 导出失败
A: 请检查日志文件。可能是 SOLIDWORKS 2024 未安装 FBX 导出插件，
   可尝试在 SOLIDWORKS 中手动导出 FBX 验证是否支持。

技术支持：
----------
如有问题，请查看 logs 目录下的日志文件获取详细错误信息。
===========================================================================