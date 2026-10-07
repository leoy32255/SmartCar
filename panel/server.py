"""Local Windows-first panel. No auto-connect and no background heartbeats."""
import argparse
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
from pathlib import Path
import struct
from panel.device import Session, SimTransport

ROOT=Path(__file__).resolve().parent
session=Session(ROOT.parent/"logs")
class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def reply(self,data,status=200,mime="application/json; charset=utf-8"):
        body=data if isinstance(data,bytes) else json.dumps(data,ensure_ascii=False).encode()
        self.send_response(status);self.send_header("Content-Type",mime)
        self.send_header("Cache-Control","no-store");self.send_header("Content-Length",str(len(body)))
        self.end_headers();self.wfile.write(body)
    def do_GET(self):
        try:
            if self.path=="/":self.reply((ROOT/"index.html").read_bytes(),mime="text/html; charset=utf-8")
            elif self.path=="/api/state":session.poll();self.reply(session.snapshot())
            elif self.path=="/api/ports":
                ports=["SIM"]
                try:
                    from serial.tools.list_ports import comports
                    ports += [p.device for p in comports()]
                except ImportError:pass
                self.reply(ports)
            else:self.reply({"error":"not found"},404)
        except Exception as exc:self.reply({"error":str(exc)},400)
    def do_POST(self):
        if self.headers.get("Origin") not in (None,"http://"+self.headers.get("Host","")):
            self.reply({"error":"origin rejected"},403);return
        if not self.headers.get("Content-Type","").startswith("application/json"):
            self.reply({"error":"JSON required"},415);return
        try:
            size=int(self.headers.get("Content-Length","0"))
            if not 0<size<=1024:raise ValueError("invalid request length")
            data=json.loads(self.rfile.read(size))
            session.poll()
            if self.path=="/api/connect":session.connect(data["port"])
            elif self.path=="/api/disconnect":session.disconnect()
            elif self.path=="/api/heartbeat":session.heartbeat()
            elif self.path=="/api/stop":session.command(1,b"\x00")
            elif self.path=="/api/start":
                if data["mode"] not in (1,2):raise ValueError("invalid mode")
                if session.snapshot()["stale"]:raise ValueError("等待新鲜遥测后再启动")
                session.command(1,bytes([data["mode"]]))
            elif self.path=="/api/drive":
                l,r=int(data["left"]),int(data["right"])
                if not (-10<=l<=10 and -10<=r<=10):raise ValueError("低速遥控范围为-10至10")
                session.command(2,struct.pack("<hh",l,r))
            elif self.path=="/api/param":
                index,value=int(data["id"]),int(data["value"])
                if not (0<=index<6 and 0<=value<=(20 if index==0 else 200)):raise ValueError("参数越界")
                session.command(3,bytes([index])+struct.pack("<h",value));session.command(8)
            elif self.path=="/api/clear":session.command(7)
            elif self.path=="/api/calibrate":session.command(4)
            elif self.path=="/api/fault":
                if not isinstance(session.transport,SimTransport):raise ValueError("仅模拟设备允许注入")
                session.transport.fault(int(data["value"]))
            else:raise ValueError("unknown action")
            session.poll();self.reply(session.snapshot())
        except Exception as exc:self.reply({"error":str(exc)},400)

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--port",type=int,default=8765)
    args=parser.parse_args();server=HTTPServer(("127.0.0.1",args.port),Handler)
    print(f"SmartCar panel: http://127.0.0.1:{args.port} (SIM default)",flush=True)
    try:server.serve_forever()
    finally:session.disconnect();server.server_close()
if __name__=="__main__":main()
