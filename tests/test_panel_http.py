"""Real local HTTP routes -> Session -> compiled firmware simulator; not UI automation."""
from http.server import HTTPServer
import json
from pathlib import Path
import tempfile
import threading
import time
import unittest
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from panel import server
from panel.device import Session

class PanelHTTP(unittest.TestCase):
    def test_sim_routes_and_rejected_input(self):
        with tempfile.TemporaryDirectory() as directory:
            previous=server.session
            server.session=Session(Path(directory))
            service=HTTPServer(("127.0.0.1",0),server.Handler)
            worker=threading.Thread(target=service.serve_forever,daemon=True);worker.start()
            base=f"http://127.0.0.1:{service.server_port}"
            def request(route,body=None):
                req=Request(base+route,data=None if body is None else json.dumps(body).encode(),headers={"Content-Type":"application/json","Origin":base})
                with urlopen(req,timeout=3) as response:return json.load(response)
            try:
                self.assertIn("SIM",request("/api/ports"))
                self.assertTrue(request("/api/connect",{"port":"SIM"})["connected"])
                request("/api/heartbeat",{});time.sleep(0.12)
                self.assertFalse(request("/api/state")["stale"])
                request("/api/start",{"mode":2});request("/api/drive",{"left":5,"right":5})
                time.sleep(0.11);state=request("/api/state")
                self.assertEqual(state["status"]["mode"],2)
                with self.assertRaises(HTTPError):request("/api/drive",{"left":100,"right":5})
                request("/api/stop",{});time.sleep(0.11)
                self.assertEqual(request("/api/state")["status"]["mode"],0)
                request("/api/disconnect",{});request("/api/connect",{"port":"SIM"});time.sleep(0.11)
                self.assertEqual(request("/api/state")["status"]["mode"],0)
            finally:
                service.shutdown();worker.join(timeout=3);service.server_close()
                server.session.disconnect();server.session=previous
