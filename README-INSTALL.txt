Opencraft 一键安装包说明

1. 双击运行本程序，文件会解压到旁边的 Opencraft\ 目录。
2. 首次使用请运行 Opencraft\extract_assets.exe，在弹出的对话框中选择你自己的
   Minecraft jar 文件（通常在 .minecraft\versions\<版本>\<版本>.jar，旧版是
   .minecraft\bin\minecraft.jar），工具会把方块/物品贴图提取到 Opencraft\assets\。
3. 运行 Opencraft\opencraft.exe 开始游戏。

重要（法律说明）：
本安装包不包含、也不分发任何 Minecraft 资源文件（贴图/音效等）。所有游戏资源
均由玩家通过 extract_assets.exe 从自己拥有的 Minecraft 客户端中提取，仅供本地
运行本程序使用。Minecraft 是 Mojang Studios 的商标，本程序与之无隶属关系。

附带的命令行参数：
  opencraft.exe --help          查看全部参数
  opencraft.exe --version       打印版本号（版本名-编译时间戳）
  opencraft.exe --frames 600    运行 600 帧后退出（基准测试）
  opencraft.exe --screenshot a.png --drive  自动行走并截图
