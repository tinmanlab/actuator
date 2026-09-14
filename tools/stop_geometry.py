"""Derive a one-axis contact envelope from existing MJCF collision proxies.
This is a build-time geometry reduction, not a second runtime collision engine.
Only the coplanar, axis-aligned single-stop fixture below is supported.
"""
import math
from pathlib import Path
import xml.etree.ElementTree as ET


def values(element, attribute):
    return tuple(map(float, element.get(attribute).split()))


def point_box(p, lo, hi):
    return math.hypot(*(max(lo[i]-p[i], 0, p[i]-hi[i]) for i in range(2)))


def point_segment(p, a, b):
    d = (b[0]-a[0], b[1]-a[1])
    t = max(0, min(1, sum((p[i]-a[i])*d[i] for i in range(2))/sum(x*x for x in d)))
    return math.hypot(*(p[i]-a[i]-t*d[i] for i in range(2)))


def segment_box(a, b, lo, hi):
    enter, leave = 0., 1.
    for i in range(2):
        d = b[i]-a[i]
        if abs(d) < 1e-14:
            if not lo[i] <= a[i] <= hi[i]:
                enter, leave = 1., 0.
                break
        else:
            x, y = sorted(((lo[i]-a[i])/d, (hi[i]-a[i])/d))
            enter, leave = max(enter, x), min(leave, y)
    if enter <= leave:
        return 0.
    return min(point_box(a, lo, hi), point_box(b, lo, hi),
               *(point_segment((x, y), a, b) for x in (lo[0], hi[0]) for y in (lo[1], hi[1])))


def envelope(xml):
    root = ET.parse(xml).getroot()
    output = root.find("worldbody/body[@name='output']")
    stop = root.find("worldbody/geom[@name='stopper']")
    tip = output.find("geom[@name='tip']")
    link = output.find("geom[@name='link']")
    pivot = values(output, 'pos'); center = values(stop, 'pos'); half = values(stop, 'size')
    endpoint = values(tip, 'pos'); line = values(link, 'fromto')
    if (values(output.find('joint'), 'axis') != (0., 1., 0.) or
        stop.get('quat') or stop.get('euler') or stop.get('type') != 'box' or
        endpoint[:2] != (0., 0.) or line[:5] != (0., 0., 0., 0., 0.) or
        endpoint[2] != line[5] or not math.isclose(pivot[1], center[1], abs_tol=1e-12)):
        raise ValueError('Unsupported contact geometry: require coplanar Y hinge and straight downward link')
    length = -endpoint[2]; rt = values(tip, 'size')[0]; rl = values(link, 'size')[0]
    if length <= 0 or half[1] < max(rt, rl):
        raise ValueError('Stop must cover the collision proxies in Y')
    lo = (center[0]-half[0], center[2]-half[2]); hi = (center[0]+half[0], center[2]+half[2])
    a = (pivot[0], pivot[2])
    def gap(q):
        b = (a[0]-length*math.sin(q), a[1]-length*math.cos(q))
        return min(point_box(b, lo, hi)-rt, segment_box(a, b, lo, hi)-rl)
    brackets = []
    n = 4096
    for i in range(n):
        left, right = math.tau*i/n, math.tau*(i+1)/n
        if (gap(left) < 0) != (gap(right) < 0):
            for _ in range(45):
                mid = (left+right)/2
                if (gap(mid) < 0) == (gap(left) < 0): left = mid
                else: right = mid
            brackets.append((left+right)/2)
    if gap(0) <= 0 or len(brackets) != 2:
        raise ValueError('Expected exactly one blocked angular interval away from zero')
    return brackets


def write_header(xml, destination):
    lower, upper = envelope(xml)
    Path(destination).write_text(
        '// Generated from bench.xml collision proxies. Do not edit. SI radians.\n'
        '#pragma once\nnamespace qdd::web {\n'
        f'inline constexpr double stop_entry={lower:.17g};\n'
        f'inline constexpr double stop_exit={upper:.17g};\n'
        '}\n', encoding='utf-8')
