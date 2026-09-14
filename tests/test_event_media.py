import importlib.util,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class EventWindows(unittest.TestCase):
 def function(self):
  path=ROOT/'tools/event_media.py'
  self.assertTrue(path.is_file(),'event-window selector required by saved site_media.py is missing')
  spec=importlib.util.spec_from_file_location('event_media',path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
  return module.event_window
 def rows(self):
  return [dict(time_s=str(k*.001),output_rad=str(.3 if k>=50 else 0),disturbance_Nm=str(4 if 700<=k<820 else 0),intended_contact_force_N=str(20 if 1000<=k<1100 else 0),fault=str(9 if k>=1000 else 0)) for k in range(2001)]
 def test_events_are_in_short_windows(self):
  f=self.function()
  for case,expected in [('tracking',.05),('disturbance',.7),('contact',1),('impact',1),('fault',1)]:
   start,end,event=f(case,self.rows());self.assertAlmostEqual(event,expected);self.assertLessEqual(start,event);self.assertLess(event,end);self.assertLess(end-start,.85);self.assertGreater(end-start,.25)
 def test_missing_event_is_not_fabricated(self):
  f=self.function();r=self.rows()
  for row in r:row['fault']='0'
  with self.assertRaises(ValueError):f('fault',r)
 def test_bad_trace_rejected(self):
  f=self.function();r=self.rows();r[10]['time_s']='nan'
  with self.assertRaises(ValueError):f('contact',r)
  with self.assertRaises(ValueError):f('unknown',self.rows())
if __name__=='__main__':unittest.main()
