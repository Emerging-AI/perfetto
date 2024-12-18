https://www.androidperformance.com/2019/05/28/Android-Systrace-About/

alias st-start-gfx-trace = ‘st-start -t 8 am,binder_driver,camera,dalvik,freq,gfx,hal,idle,input,memory,memreclaim,res,sched,sync,view,webview,wm,workq,binder’


https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/
# 线程状态
perfetto的ui atrace是否能够显示 线程状态


## 运行中（Running）

作用
经常会查看 Running 状态的线程，查看其运行的时间，与竞品做对比，分析快或者慢的原因：
是否频率不够？
是否跑在了小核上？
是否频繁在 Running 和 Runnable 之间切换？为什么？
是否频繁在 Running 和 Sleep 之间切换？为什么？
是否跑在了不该跑的核上面？比如不重要的线程占用了超大核

看频率，一般分三档
小核 / 核（正常） / 大核（分四档才存在） /超大核

例如
339MHz-2400MHz  小核
622MHz-3300MHz  核 
798MHz-3626MHz  大核



## 可运行（Runnable）

线程可以运行但当前没有安排，在等待 cpu 调度

作用：Runnable 状态的线程状态持续时间越长，则表示 cpu 的调度越忙，没有及时处理到这个任务：

是否后台有太多的任务在跑？
没有及时处理是因为频率太低？
没有及时处理是因为被限制到某个 cpuset 里面，但是 cpu 很满？
此时 Running 的任务是什么？为什么？

## 白色 : 休眠中（Sleep）
线程没有工作要做，可能是因为线程在互斥锁上被阻塞。

作用 ： 这里一般是在等事件驱动


## 橘色 : 不可中断的睡眠态 （Uninterruptible Sleep - IO Block）
线程在I / O上被阻塞或等待磁盘操作完成，一般底线都会标识出此时的 callsite ：wait_on_page_locked_killable

作用：这个一般是标示 io 操作慢，如果有大量的橘色不可中断的睡眠态出现，那么一般是由于进入了低内存状态，申请内存的时候触发 pageFault, linux 系统的 page cache 链表中有时会出现一些还没准备好的 page(即还没把磁盘中的内容完全地读出来) , 而正好此时用户在访问这个 page 时就会出现 wait_on_page_locked_killable 阻塞了. 只有系统当 io 操作很繁忙时, 每笔的 io 操作都需要等待排队时, 极其容易出现且阻塞的时间往往会比较长.

## 紫色 : 不可中断的睡眠态（Uninterruptible Sleep）
线程在另一个内核操作（通常是内存管理）上被阻塞。

作用：一般是陷入了内核态，有些情况下是正常的，有些情况下是不正常的，需要按照具体的情况去分析


Frame Completed
Issue Draw Commands Start
Sync Start
Sync Queued
IDraw Start
Perform Traversals Start
Animation Start
Handle lnput Start
VSync


## https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/

