# PostgreSQL 课 1 作业答案

## 一 选择题

| 题号 | 答案 | 理由 |
|---|---|---|
| 1.1 | C | `--enable-debug` 加入调试符号，便于 GDB 调试；它不等于必然关闭优化。 |
| 1.2 | A | `-O0` 到 `-O3` 表示逐级提高编译器优化级别。优化级别可能影响编译耗时，但 `CFLAGS` 在这里主要指定优化。 |
| 1.3 | C | 通常发行版使用 `-O2`，兼顾性能和构建成本；具体发行版仍以其打包参数为准。 |
| 1.4 | A、B、C | 依次完成配置、编译和安装。 |
| 1.5 | B | 编译选项和构建位置不变时，改源码后执行 `make`；只有要安装新产物时才再执行 `make install`。 |
| 1.6 | B（最接近） | 应先读报错、修正根因再运行 `make`。`make clean` 通常并非必需；题目选项没有准确表述这一点，故 B 是四项中最接近的。 |
| 1.7 | A | 先找会话后端 PID，再附加 GDB、设断点、执行 SQL、单步观察。 |
| 1.8 | A、B、末尾重复标成“A”的选项 | 前两种分别在 `run` 前后传入 `-D` 参数，均可。末尾的 watchpoint 方法仅在监视地址有效时可行。C 在启动 GDB 前执行 `b`，顺序错误。原题最后一个选项标签误写为 A。 |
| 1.9 | A、C | 从 C 程序角度 `main()` 是入口；操作系统启动进程后，运行库启动代码通常先于 `main()` 执行。`PostmasterMain()` 只是 `main()` 分派到的路径之一。 |
| 1.10 | D | 列出的五种特殊首参数都能在代码中看到；其余参数（包括没有参数）落入 `PostmasterMain()`。`--fork` 分支只在定义 `EXEC_BACKEND` 时编译。 |

依据：[PostgreSQL 15 源码安装说明](https://www.postgresql.org/docs/15/install-procedure.html)（`configure`、`make`、`make install`、`--enable-debug`、`CFLAGS`、`make clean`）。第 1.10 题按题目给出的 PostgreSQL 15.2 `main.c` 代码片段判断。

## 二 开发与分析题

提交文件 `link_search.c` 是 C 语言双向循环链表搜索程序，保留题目给出的 `LinkData` 成员与三个指定函数。每个节点只做一次分配，数据紧接结构体存放；`FindNode` 从头逐个比较 `dataSize` 和数据内容，没有使用哈希。程序以链表最后一个节点作为搜索目标，重复搜索后用 `perf_event_open()` 统计：

- 退休指令数；
- CPU 周期数；
- L1 数据缓存读取次数，作为本程序可移植事件定义下的“访存次数”近似指标；
- L1 数据缓存读取命中率，计算式为 `(读取次数 - 读取未命中次数) / 读取次数`。

Linux 上使用 GCC 编译并运行：

```sh
gcc -std=c11 -O2 -Wall -Wextra -o link_search link_search.c
./link_search 10000 64 1000
```

若按题目要求将结果留在 `/home/assit/hk2`，把本目录四个文件复制到该目录，在该目录运行 `sh run_linux.sh`。脚本只在当前目录生成 `link_search`、`test_link` 和按当天日期命名的 `MMDD_1.log`；日志包含 CPU 信息、编译器版本、测试及性能计数结果。脚本使用 `-O2`，不使用 `-O3`。

三个参数依次是节点数、每节点数据字节数（32–512）、搜索重复次数。示例会搜索链表末节点 1000 次。`perf_event_open()` 需要 Linux CPU 和相应内核权限；如果被拒绝，程序会报告原因。不同 CPU 对 L1D 事件的支持可能不同。`PERF_COUNT_HW_CACHE_L1D` 的读取次数并不等于全部机器指令的内存访问次数，因此报告中明确写作 L1D reads。多次计数器复用时，程序按 `time_enabled / time_running` 缩放估计值，并输出运行时间占比。

链表正确性测试：

```sh
gcc -std=c11 -O2 -Wall -Wextra -Werror -o test_link test_link.c
./test_link
```

计数器定义与组读取格式依据：[Linux `perf_event_open(2)` 手册](https://man7.org/linux/man-pages/man2/perf_event_open.2.html)。

## 三 提交方式

1. 先在 Linux 上运行 `sh run_linux.sh`，确认日志里有四项性能指标；若计数器被权限或硬件支持限制阻止，不要编造数据，应把日志中的错误交给老师或管理员处理。
2. 成功实测后，将生成的 `MMDD_1.log` 上传到本仓库。不要上传编译出的可执行文件。
3. 提交给老师的是**本仓库的链接**。`postgres/postgres`、`HaloTech-Co-Ltd/openHalo` 与 `HaloTech-Co-Ltd/hk2` 是参考项目，不是这份作业的提交地址。

链表功能测试已在 macOS 上通过；Linux 硬件计数仍需在 Linux 环境实测。此仓库没有虚构的性能数字。
