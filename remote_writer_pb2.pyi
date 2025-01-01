from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class TraceDataRequest(_message.Message):
    __slots__ = ("data", "size")
    DATA_FIELD_NUMBER: _ClassVar[int]
    SIZE_FIELD_NUMBER: _ClassVar[int]
    data: bytes
    size: int
    def __init__(self, data: _Optional[bytes] = ..., size: _Optional[int] = ...) -> None: ...

class TraceDataResponse(_message.Message):
    __slots__ = ("status",)
    STATUS_FIELD_NUMBER: _ClassVar[int]
    status: str
    def __init__(self, status: _Optional[str] = ...) -> None: ...
