adb shell atrace --list_categories
         gfx - Graphics
       input - Input
        view - View System
     webview - WebView
          wm - Window Manager
          am - Activity Manager
          sm - Sync Manager
       audio - Audio
       video - Video
      camera - Camera
         hal - Hardware Modules
         res - Resource Loading
      dalvik - Dalvik VM
          rs - RenderScript
      bionic - Bionic C Library
       power - Power Management
          pm - Package Manager
          ss - System Server
    database - Database
     network - Network
         adb - ADB
    vibrator - Vibrator
        aidl - AIDL calls
       nnapi - NNAPI
         rro - Runtime Resource Overlay
         pdx - PDX services
       sched - CPU Scheduling
         irq - IRQ Events
        freq - CPU Frequency
        idle - CPU Idle
        disk - Disk I/O
        sync - Synchronization
  memreclaim - Kernel Memory Reclaim
  binder_driver - Binder Kernel driver
  binder_lock - Binder global lock trace
      memory - Memory
     thermal - Thermal event
         gfx - Graphics (HAL) (emu才有)
         ion - ION allocation (HAL)  (emu才有)


adb -s 0123456789ABCDEF shell atrace --list_categories
         gfx - Graphics
       input - Input
        view - View System
     webview - WebView
          wm - Window Manager
          am - Activity Manager
          sm - Sync Manager
       audio - Audio
       video - Video
      camera - Camera
         hal - Hardware Modules
         res - Resource Loading
      dalvik - Dalvik VM
          rs - RenderScript
      bionic - Bionic C Library
       power - Power Management
          pm - Package Manager
          ss - System Server
    database - Database
     network - Network
         adb - ADB
    vibrator - Vibrator
        aidl - AIDL calls
       nnapi - NNAPI
         rro - Runtime Resource Overlay
         pdx - PDX services
       sched - CPU Scheduling
         irq - IRQ Events
         i2c - I2C Events (比emu多)
        freq - CPU Frequency
        idle - CPU Idle
        disk - Disk I/O
         mmc - eMMC commands  (比emu多)
        sync - Synchronization
       workq - Kernel Workqueues  (比emu多)
  memreclaim - Kernel Memory Reclaim
  regulators - Voltage and Current Regulators  (比emu多)
  binder_driver - Binder Kernel driver
  binder_lock - Binder global lock trace
   pagecache - Page cache  (比emu多)
      memory - Memory
     thermal - Thermal event