import tempfile
from pathlib import Path
import unittest
from panel.device import Session, frame, Decoder
class PanelTests(unittest.TestCase):
    def test_actual_c_simulator_and_reconnect(self):
        with tempfile.TemporaryDirectory() as directory:
            s=Session(Path(directory));self.addCleanup(s.disconnect);s.connect("SIM");s.poll(0.005)
            s.heartbeat();s.command(1,b"\x02");s.command(2,b"\x05\x00\x05\x00")
            s.poll(0.11);self.assertEqual(s.status["mode"],2)
            s.poll(0.61);self.assertEqual(s.status["mode"],0);self.assertEqual(s.status["fault"]&1,1)
            s.disconnect();s.connect("SIM");s.poll(0.11);self.assertEqual(s.status["mode"],0)
            s.disconnect();self.assertTrue(list(Path(directory).glob("*.jsonl")))
    def test_frame_known_literal(self):
        self.assertEqual(frame(1,b"\x00"),bytes.fromhex("aa5501010002"))

    def test_decoder_noise_split_and_bad_checksum(self):
        d=Decoder()
        self.assertEqual(d.feed(b"noise\xaa"),[])
        self.assertEqual(d.feed(b"\x55\x06\x00\x07"),[])
        self.assertEqual(d.feed(frame(1,b"\x00")[:3]),[])
        self.assertEqual(d.feed(frame(1,b"\x00")[3:]),[(1,b"\x00")])
    def test_parameters_fault_clear_and_log_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            s=Session(Path(directory))
            try:
                s.connect("SIM");s.poll(0.005);s.heartbeat()
                s.command(3,b"\x00\x08\x00");s.command(8);s.poll(0.01)
                self.assertEqual(s.params[0],8)
                s.command(1,b"\x01");s.transport.fault(1);s.poll(0.11)
                self.assertEqual(s.status["fault"]&4,4)
                s.transport.fault(0);s.poll(0.12);s.heartbeat();s.command(7);s.poll(0.22)
                self.assertEqual(s.status["mode"],0);self.assertEqual(s.status["fault"],0)
                path=Path(s.log_path)
            finally:s.disconnect()
            import json
            records=[json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
            self.assertTrue(any(r["kind"]=="parameters" and r["values"][0]==8 for r in records))
            self.assertTrue(any(r["kind"]=="telemetry" for r in records))
