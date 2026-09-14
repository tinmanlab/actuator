"""Documentation examples and geometry must remain coupled to actual source."""
from pathlib import Path
from html.parser import HTMLParser
from html import unescape
import math,re,sys,unittest,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from stop_geometry import envelope,point_box,segment_box

class Document(HTMLParser):
 def __init__(self,text):
  super().__init__();self.ids=set();self.links=[];self.svg=0;self.titles=0;self.feed(text)
 def handle_starttag(self,tag,attrs):
  a=dict(attrs)
  if 'id' in a:self.ids.add(a['id'])
  if tag=='a':self.links.append(a.get('href',''))
  if tag=='svg':self.svg+=1
  if tag=='title':self.titles+=1

class Contracts(unittest.TestCase):
 def test_geometry_lower_is_original_front_tangent(self):
  lo,hi=envelope(ROOT/'examples/mujoco/bench.xml')
  self.assertAlmostEqual(lo,.6,places=8);self.assertGreater(hi,1.09);self.assertLess(hi,1.12)
 def test_geometry_upper_is_tip_corner_tangent(self):
  root=ET.parse(ROOT/'examples/mujoco/bench.xml');g=root.find(".//geom[@name='stopper']")
  c=list(map(float,g.get('pos').split()));s=list(map(float,g.get('size').split()))
  x=-(c[0]-s[0]);z=.6-(c[2]+s[2]);d=math.hypot(x,z)
  expected=math.atan2(x,z)+math.acos((d*d+.32**2-.031**2)/(2*d*.32))
  self.assertAlmostEqual(envelope(ROOT/'examples/mujoco/bench.xml')[1],expected,places=10)
 def test_segment_box_crossing_not_missed(self):
  self.assertEqual(segment_box((-2,0),(2,0),(-1,-1),(1,1)),0)
  self.assertEqual(segment_box((-2,2),(2,2),(-1,-1),(1,1)),1)
  self.assertAlmostEqual(point_box((2,2),(-1,-1),(1,1)),math.sqrt(2))
 def test_control_page_has_labeled_diagrams_and_sections(self):
  d=Document((ROOT/'web/control.html').read_text())
  self.assertGreaterEqual(d.svg,3);self.assertGreaterEqual(d.titles,4)
  self.assertTrue({'pipeline','foc','timing','contact','sources','stage-detail'}<=d.ids)
 def test_api_has_complete_set_and_get_tables(self):
  text=(ROOT/'web/api.html').read_text()
  self.assertEqual(set(map(int,re.findall(r'data-set="(\d+)"',text))),set(range(11)))
  self.assertEqual(set(map(int,re.findall(r'data-get="(\d+)"',text))),set(range(31)))
 def test_displayed_cpp_example_is_compiled_source(self):
  text=(ROOT/'web/api.html').read_text()
  snippet=unescape(re.search(r'<code id="live-example">(.*?)</code>',text,re.S).group(1))
  self.assertEqual(snippet,(ROOT/'examples/live_api.cpp').read_text())
 def test_internal_links_resolve(self):
  from urllib.parse import urlsplit
  for name in ('control.html','api.html'):
   doc=Document((ROOT/'web'/name).read_text())
   for href in doc.links:
    u=urlsplit(href)
    if u.scheme or u.netloc:continue
    target=ROOT/'web'/(u.path or name)
    self.assertTrue(target.exists(),(name,href))
    if u.fragment and target.suffix=='.html':self.assertIn(u.fragment,Document(target.read_text()).ids,(name,href))
 def test_api_source_links_exist(self):
  for path in re.findall(r'data-source="([^"#]+)',(ROOT/'web/api.html').read_text()):
   self.assertTrue((ROOT/path).is_file(),path)
 def test_telemetry_width_matches_worker(self):
  text=(ROOT/'web/app.js').read_text();cols=re.search(r'const telemetry=\[(.*?)\];',text).group(1)
  self.assertEqual(len(re.findall(r"'[^']+'",cols)),31)
  self.assertIn('const fields=31;',(ROOT/'web/worker.js').read_text())
 def test_start_page_navigation_and_nonfatal_notice(self):
  text=(ROOT/'web/index.html').read_text();d=Document(text)
  self.assertIn('notice',d.ids);self.assertIn('control.html',d.links);self.assertIn('api.html',d.links)

if __name__=='__main__':unittest.main()
