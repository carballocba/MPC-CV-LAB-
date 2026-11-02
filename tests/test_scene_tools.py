import copy,json,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import scene_tool as tool
class SceneTools(unittest.TestCase):
    def setUp(self):self.scene=tool.read_json(ROOT/'scenes/generic-performance.json')
    def test_roundtrip_cpp(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'input.mcvs';dest=Path(directory)/'output.mcvs'
            tool.atomic_write(source,tool.encode(self.scene))
            subprocess.run([str(ROOT/'build/scene_io_test'),str(source),str(dest)],check=True)
            self.assertEqual(source.read_bytes(),dest.read_bytes())
            self.assertEqual(tool.encode(tool.decode(dest.read_bytes())),dest.read_bytes())
    def test_preset_and_routes(self):
        preset=tool.read_json(ROOT/'presets/sine.engine.json')
        before=copy.deepcopy(self.scene)
        applied=tool.apply_preset(self.scene,preset,3)
        self.assertEqual(applied['nodes'][3]['type'],'sine')
        self.assertEqual(applied['nodes'][3]['cycles_per_beat'],.25)
        self.assertEqual(applied['routes'],tool.validate(self.scene)['routes'])
        self.assertEqual(self.scene,before)
    def test_bad_scene(self):
        for mutate in [lambda s:s.update(schema_version=2),lambda s:s.update(sync=False),
                       lambda s:s['nodes'][0].update(input=0),lambda s:s['nodes'][0].update(modulator=99),
                       lambda s:s['nodes'][0].update(scale=float('nan')),lambda s:s['nodes'][0].update(seed=True),
                       lambda s:s['routes'][0].update(maximum_volts=6),lambda s:s.update(extra=1)]:
            bad=copy.deepcopy(self.scene);mutate(bad)
            with self.assertRaises(ValueError):tool.encode(bad)
    def test_corruption_and_truncation(self):
        data=tool.encode(self.scene)
        for n in range(len(data)):
            with self.assertRaises(ValueError):tool.decode(data[:n])
        bad=bytearray(data);bad[40]^=1
        with self.assertRaises(ValueError):tool.decode(bad)
    def test_atomic_invalid_cli(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'bad.json';dest=Path(directory)/'state.mcvs';dest.write_bytes(b'preserve me')
            for invalid in ['{"schema_version":1,"schema_version":2}','{"nodes":[NaN]}']:
                source.write_text(invalid)
                result=subprocess.run([sys.executable,str(ROOT/'tools/scene_tool.py'),'compile',str(source),str(dest)],capture_output=True)
                self.assertEqual(result.returncode,2);self.assertEqual(dest.read_bytes(),b'preserve me')
    def test_empty_scene(self):
        s={'schema_version':1,'kind':'performance_scene','nodes':[],'enabled_mask':0}
        self.assertEqual(tool.decode(tool.encode(s))['nodes'],[])
if __name__=='__main__':unittest.main()
