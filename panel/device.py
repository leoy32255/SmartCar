"""Serial/SPP and the real C simulated device share one framing/session path."""
import ctypes
import datetime as dt
import json
import os
from pathlib import Path
import struct
import time

ROOT = Path(__file__).resolve().parents[1]

def frame(cmd, payload=b""):
    if len(payload)>48: raise ValueError("payload exceeds protocol limit")
    body=bytes([cmd,len(payload)])+payload
    return b"\xaa\x55"+body+bytes([sum(body)&255])

class Decoder:
    def __init__(self): self.buffer=bytearray()
    def feed(self,data):
        self.buffer.extend(data)
        frames=[]
        while len(self.buffer)>=2:
            if self.buffer[:2]!=b"\xaa\x55": del self.buffer[0];continue
            if len(self.buffer)<4:break
            n=self.buffer[3]
            if n>48:del self.buffer[0];continue
            if len(self.buffer)<n+5:break
            candidate=self.buffer[:n+5]
            if sum(candidate[2:-1])&255==candidate[-1]:
                frames.append((candidate[2],bytes(candidate[4:-1])))
                del self.buffer[:n+5]
            else:del self.buffer[0]
        return frames

class SimTransport:
    def __init__(self):
        path=ROOT/"build/host"/("smartcar_sim.dll" if os.name=="nt" else "smartcar_sim.so")
        self.lib=ctypes.CDLL(str(path))
        self.lib.sim_write.argtypes=[ctypes.c_void_p,ctypes.c_uint]
        self.lib.sim_read.argtypes=[ctypes.c_void_p,ctypes.c_uint]
        self.lib.sim_read.restype=ctypes.c_uint
        self.lib.sim_advance.argtypes=[ctypes.c_uint32]
        self.lib.sim_init()
    def advance(self,seconds): self.lib.sim_advance(int(seconds*1000)&0xffffffff)
    def write(self,data): self.lib.sim_write(data,len(data));return len(data)
    def read(self):
        data=ctypes.create_string_buffer(8192)
        size=self.lib.sim_read(data,len(data))
        return data.raw[:size]
    def close(self):pass
    def fault(self,value):self.lib.sim_fault(value)

class SerialTransport:
    def __init__(self,port):
        import serial
        self.serial=serial.Serial(port,9600,timeout=0,write_timeout=0.05)
        self.serial.reset_input_buffer()
    def write(self,data):return self.serial.write(data)
    def read(self):return self.serial.read(min(self.serial.in_waiting,4096))
    def close(self):self.serial.close()

class Session:
    def __init__(self,log_dir):
        self.log_dir=Path(log_dir);self.transport=None;self.log=None
        self.status={};self.params=[];self.error="";self.ack=None;self.port=""
        self.log_path="";self.last_rx=None;self.started=0;self.decoder=Decoder()
    def _record(self,kind,**values):
        if self.log:
            self.log.write(json.dumps({"time":dt.datetime.now(dt.timezone.utc).isoformat(),"kind":kind,**values},ensure_ascii=False)+"\n")
            self.log.flush()
    def connect(self,port):
        self.disconnect()
        self.log_dir.mkdir(parents=True,exist_ok=True)
        self.log_path=str(self.log_dir/(dt.datetime.now().strftime("%Y%m%d-%H%M%S-%f")+".jsonl"))
        self.log=open(self.log_path,"w",encoding="utf-8")
        try:
            self.transport=SimTransport() if port=="SIM" else SerialTransport(port)
            self.port=port;self.started=time.monotonic();self.last_rx=None
            self.status={};self.params=[];self.ack=None;self.decoder=Decoder();self.error=""
            self._record("connect",port=port,protocol=1,firmware="F407 classroom v1 (verify flashed hash separately)")
            self.command(1,b"\x00");self.command(8)
        except Exception:
            self.disconnect();raise
    def command(self,cmd,payload=b""):
        if self.transport is None:raise RuntimeError("尚未连接")
        data=frame(cmd,payload)
        try:
            if self.transport.write(data)!=len(data):raise OSError("串口只写出部分帧")
            self._record("command",cmd=cmd,payload=payload.hex())
        except Exception as exc:
            self.error=str(exc)
            # Fail closed locally: cease heartbeats; firmware timeout remains active.
            self.transport.close();self.transport=None
            raise
    def heartbeat(self):self.command(6)
    def poll(self,elapsed=None):
        if self.transport is None:return
        try:
            if isinstance(self.transport,SimTransport):
                self.transport.advance(time.monotonic()-self.started if elapsed is None else elapsed)
            for cmd,payload in self.decoder.feed(self.transport.read()):
                self._record("rx",cmd=cmd,payload=payload.hex())
                if cmd==0x81 and len(payload)==42:
                    if payload[0]!=1:raise ValueError("不兼容的协议版本")
                    values=struct.unpack_from("<14h",payload,6)
                    self.status=dict(version=payload[0],mode=payload[1],fault=int.from_bytes(payload[2:4],"little"),mask=payload[4],health=payload[5],deviation=values[0],speed=list(values[1:3]),pwm=list(values[3:5]),accel=list(values[5:8]),gyro_raw=list(values[8:11]),bias=list(values[11:14]),device_ms=int.from_bytes(payload[34:38],"little"),bad_frames=int.from_bytes(payload[38:40],"little"),dropped_tx=int.from_bytes(payload[40:42],"little"))
                    self.last_rx=time.monotonic();self._record("telemetry",**self.status)
                elif cmd==0x82 and len(payload)==2:self.ack={"command":payload[0],"ok":payload[1]==0}
                elif cmd==0x84 and len(payload)==12:
                    self.params=list(struct.unpack("<6h",payload));self._record("parameters",values=self.params)
        except Exception as exc:
            self.error=str(exc);self.disconnect();raise
    def snapshot(self):
        return dict(connected=self.transport is not None,port=self.port,status=self.status,params=self.params,ack=self.ack,error=self.error,log=self.log_path,stale=self.last_rx is None or time.monotonic()-self.last_rx>0.5)
    def disconnect(self):
        if self.transport is not None:
            try:self.transport.write(frame(1,b"\x00"))
            except Exception as exc:self.error=str(exc)
            finally:self.transport.close();self.transport=None
        if self.log:
            try:self._record("disconnect")
            finally:self.log.close();self.log=None
        self.status={};self.params=[];self.last_rx=None
