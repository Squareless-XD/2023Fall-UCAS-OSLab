# Project2-A_Simple_Kernel

## 简介

实现了C-core的线程, 实现较简单, 采用与进程一视同仁的调度结构, 并未按照进程 - 线程的层次结构进行调度, 类似于弱线程.

## 使用方法

### 启动

在项目根目录下执行

```bash
./launch.sh
```

进入Qemu后, 输入 `loodboatd` 启动内核.

输入用户程序名称 (位于`test/test_project2`目录下) 并按回车, 将用户程序挂载至准备队列.

输入 `OS, strat!` 进入用户程序.

### debug

在项目根目录下执行

```bash
./debug.sh
```

并在另一个终端执行

```bash
make gdb
```

后执行方法同上.
