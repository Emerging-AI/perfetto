

ftrace是以Module的方式加入到perfetto，包含两个module

- FtraceModuleImpl
- EtwModuleImpl

使用RegisterAdditionalModules方法以抽象工厂模式注册到TraceProcessorContext context
 The context is used to insert the machine ID into the sqlite tables
 context->multi_machine_trace_manager->EnableAdditionalModules 

 最后由ProtoTraceReader在执行ParsePacket中返回实际的Module来执行 reader->ParsePacket
-----

ProtoTraceReader的Parse方法就是 TraceProcessorStorage

TraceProcessor 继承 TraceProcessorStorage




连设备
选游戏

HismartRerf, Qualcomm Tool, Tencent Rerfdog?

