"""The entry page has one running bench and one progressive signal explanation."""
from pathlib import Path
from html.parser import HTMLParser
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Elements(HTMLParser):
 def __init__(self,text):
  super().__init__();self.tags=[];self.feed(text)
 def handle_starttag(self,t,attrs):self.tags.append((t,dict(attrs)))
class Home(unittest.TestCase):
 def setUp(self):
  self.text=(ROOT/'web/index.html').read_text();self.doc=Elements(self.text)
 def test_pipeline_precedes_viewer_and_media(self):
  ids=[a.get('id') for _,a in self.doc.tags]
  self.assertIn('live-pipeline',ids)
  self.assertLess(ids.index('live-pipeline'),ids.index('viewport'))
  self.assertLess(ids.index('pipeline-detail'),ids.index('videos'))
 def test_one_ordered_chain_and_no_duplicate_lesson_launchers(self):
  stages=[a['data-live-stage'] for _,a in self.doc.tags if 'data-live-stage' in a]
  self.assertEqual(stages,['command','foc','bridge','motion','sensor'])
  self.assertNotIn('data-lesson=',self.text)
  self.assertNotIn('class="readouts"',self.text)
  self.assertNotIn('class="flow"',self.text)
 def test_signal_scope_and_external_modes_are_named(self):
  self.assertIn('Same running joint',self.text)
  self.assertIn('Independent calculation',self.text)
  self.assertIn('Recorded',self.text)
  self.assertIn('Averaged inverter',self.text)
  self.assertNotIn('<iframe',self.text)
 def test_axis_labels_are_not_ambiguous(self):
  code=(ROOT/'web/home-pipeline.js').read_text()
  self.assertIn('Iq ${f(s[3])} A',code)
  self.assertIn('ia ${f(s[8])} A',code)
  self.assertIn('outer torque loop bypassed',code)
 def test_unique_ids_and_zero_default_feedforward(self):
  ids=[a['id'] for _,a in self.doc.tags if 'id' in a]
  self.assertEqual(len(ids),len(set(ids)))
  for field in ['velocity','torque']:
   element=next(a for _,a in self.doc.tags if a.get('id')==field)
   self.assertEqual(float(element['value']),0,field+' hidden bias on entry')
if __name__=='__main__':unittest.main()
