import time
import threading
from concurrent import futures
from perfetto.trace_processor import TraceProcessor
import grpc
import remote_writer_pb2_grpc
import remote_writer_pb2


STARTED = False
# tp = TraceProcessor(trace="/Users/kk1999/Documents/my_code/innovativeai/github/perfetto/trace.perfetto-trace", addr="http://localhost:9001")

# tp = TraceProcessor(trace="tmp-trace", addr="http://localhost:9001")
# qr_it = tp.query('''select * from sqlite_master where type='table' ''')
# for row in qr_it:
#     print(row.ts, row.dur, row.name)


class RemoteServerServicer(remote_writer_pb2_grpc.TraceDataServiceServicer):
    def __init__(self):
        """"""
        # 初始化 TraceProcessor，不指定 trace 文件
        # self.tp = TraceProcessor(trace=None, addr="http://localhost:9001")
        self.f = open("tmp-trace", 'wb')

    def SendTraceData(self, request: remote_writer_pb2.TraceDataRequest, context):
        """"""
        global STARTED
        print(request.size)
        try:
            if STARTED is False:
                STARTED = True
            trace_data = request.data
            # 将数据添加到 TraceProcessor 中
            # self.tp._parse_trace(trace_data)
            # r = self.tp.http.parse(trace_data)
            print(f"r: {len(trace_data)}")
            self.f.write(trace_data)
            return remote_writer_pb2.TraceDataResponse(status="success")
        except Exception:
            import traceback
            traceback.print_exc()
            return remote_writer_pb2.TraceDataResponse(status="success")


def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    
    s = RemoteServerServicer()
    t = threading.Thread(target=thread_notify, args=(s, ))
    t.start()
    remote_writer_pb2_grpc.add_TraceDataServiceServicer_to_server(s, server)
    server.add_insecure_port("[::]:3456")
    server.start()
    print("server started")
    server.wait_for_termination()


def thread_notify(s: RemoteServerServicer):
    global STARTED
    c = 0
    while c < 10:
        time.sleep(1)
        if not STARTED:
            continue
        c += 1
    print('start notify_eof')
    # s.tp.http.notify_eof()

serve()
