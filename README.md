# UNO CLI
用C++编写的一个UNO牌游戏，基于我的Console UI打造。
## 关于Console UI
Console UI是我用C++编写的一个控制台用户界面库，支持Windows和Linux平台。它提供了丰富的功能，可以帮助开发者快速构建控制台应用程序的用户界面。
此项目的仓库地址：[Console UI](https://github.com/kevin-steve772/console-ui)
## 由来
我是一个初中学生，我们年段有一个有点悠久的规定：
> 禁止带卡牌来学校！

这使得很多想爽玩的同学非常难受。我作为班级的信息管理员，想使用代码的力量解决。

于是，一个赛博UNO卡牌诞生了，它就是UNO CLI！
## 游戏玩法
UNO是一种流行的卡牌游戏，适合2-4人玩。 游戏的目标是成为第一个出完手中牌的玩家。每个玩家在自己的回合可以出一张与当前牌面颜色或数字相同的牌，或者出一张特殊牌来改变游戏规则。特殊牌包括跳过、反转、加2、换色等。玩家需要根据手中的牌和当前牌面来制定策略，尽快出完所有牌。
## 运行游戏
### 方式1 - 直接去Release下载exe文件
1. 访问[Release页面](https://github.com/kevin-steve772/uno_cli/releases)下载最新版本的exe文件。
2. 双击exe文件运行游戏。
> 注意: 该exe文件是为Windows系统编译的，如果你使用的是Linux系统，请参考方式2进行编译运行。
> Windows - 从v1.1.0开始，我们成功让游戏的exe文件不依赖编译器，不会报dll缺失的错误。
### 方式2 - 克隆仓库并编译
1. 克隆仓库：
   ```bash
   git clone https://github.com/kevin-steve772/uno_cli.git
   ```
2. 进入项目目录：
   ```bash
   cd ./uno_cli/
   ```
3. 编译项目：
   ```bash
   g++ -std=c++11 -static-libgcc -static-libstdc++ -o uno_game.exe uno_cli.cpp
   ```
> 注意: 需要安装g++编译器，并确保其路径已添加到系统PATH环境变量中。
> 如果是Linux，可以使用以下命令编译：
>   ```bash
>    g++ -std=c++11 -o uno_game uno_cli.cpp
>   ```
4. 运行游戏：
   ```bash
   ./uno_game.exe
   ```
> Linux用户运行命令：
>   ```bash
>    ./uno_game
>   ```
## 开发计划
- [x] 实现基本的UNO游戏逻辑。
- [x] 集成Console UI库，提供更好的用户界面。
- [x] 加入一些加载动画使其更精致（虽然是假的）。
- [x] 加入类似于Minecraft的指令系统，用来调试。（在游戏中狂按`/`调出）
- [ ] 制作配置菜单。
- [x] 修复一个已知的牌桌边框显示Bug。
## 关于Dev edition
看见上面的源码了吗？这是Dev edition的源码。
我随时会把当前开发过程的源代码Commit上去，让你们随时可以看看我的开发进度。
Dev edition因为没开发完全，Bug是常有的事，所以请不要交Dev edition的Issue。
如果你想要正式版的源码，请移步[Releases](https://github.com/kevin-steve772/uno_cli/releases)中相应版本的Source Code压缩包！
## Bug/建议报告
如果你在使用过程中遇到任何问题，或者有任何功能建议，请随时在GitHub仓库的[Issue](https://github.com/kevin-steve772/uno_cli/issues)页面提交你的反馈。我们会尽快回复并处理你的问题。
### Issue(Bug)中包含的内容
- 版本？(在源码里下的程序(Dev edition)不接受Issue。)
- Bug怎么发生的？
- 我可以怎么复现它？
### Issue(建议)中包含的内容
- 功能具体描述？
## 已知的Bug
- 当AI有2个及以上时，显示会有问题(来自班级中的内测人员)(✅修好了！)
- 本该显示白色的东西变成了终端默认颜色(来自我自己)(✅修好了！)