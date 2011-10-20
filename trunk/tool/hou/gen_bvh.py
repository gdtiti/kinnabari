# This file is part of Kinnabari.
# Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
# Released under the terms of Version 3 of the GNU General Public License.
# See LICENSE.txt for details.

import os
import sys
import hou
from math import *
from array import array

class BinFile:
	def __init__(self): pass

	def open(self, name):
		self.name = name
		self.file = open(name, "wb")

	def close(self): self.file.close()
	def seek(self, offs): self.file.seek(offs)
	def getPos(self): return self.file.tell()
	def writeI8(self, val): array('b', [val]).tofile(self.file)
	def writeU8(self, val): array('B', [val]).tofile(self.file)
	def writeI32(self, val): array('i', [val]).tofile(self.file)
	def writeU32(self, val): array('I', [val]).tofile(self.file)
	def writeI16(self, val): array('h', [val]).tofile(self.file)
	def writeU16(self, val): array('H', [val]).tofile(self.file)
	def writeF32(self, val): array('f', [val]).tofile(self.file)
	def writeQVec(self, v):
		for i in xrange(3): self.writeF32(v[i])
		self.writeF32(1.0)
	def writeFOURCC(self, str):
		n = len(str)
		if n > 4: n = 4
		for i in xrange(n): self.writeU8(ord(str[i]))
		for i in xrange(4-n): self.writeU8(0)
			
class BBox:
	def __init__(self, bbox = None):
		if bbox:
			self.minPos = [bbox.minPos[i] for i in xrange(3)]
			self.maxPos = [bbox.maxPos[i] for i in xrange(3)]
		else:
			self.minPos = [0.0 for i in xrange(3)]
			self.maxPos = list(self.minPos)

	def addPnt(self, pos):
		for i in xrange(3):
			if pos[i] < self.minPos[i]: self.minPos[i] = pos[i]
			if pos[i] > self.maxPos[i]: self.maxPos[i] = pos[i]

	def fromPoly(self, poly):
		nbVtx = len(poly.prim.vertices())
		pos = poly.prim.vertices()[0].point().position()
		self.minPos = [pos[i] for i in xrange(3)]
		self.maxPos = list(self.minPos)
		for i in xrange(1, nbVtx):
			pos = poly.prim.vertices()[i].point().position()
			self.addPnt(pos)

	def fromAABB(self, aabb):
		vmin = aabb.minvec()
		vmax = aabb.maxvec()
		self.minPos = [vmin[i] for i in xrange(3)]
		self.maxPos = [vmax[i] for i in xrange(3)]

	def merge(self, bbox):
		for i in xrange(3):
			if bbox.minPos[i] < self.minPos[i]: self.minPos[i] = bbox.minPos[i]
			if bbox.maxPos[i] > self.maxPos[i]: self.maxPos[i] = bbox.maxPos[i]

	def size(self):
		return [self.maxPos[i] - self.minPos[i] for i in xrange(3)]

	def center(self):
		return [(self.minPos[i] + self.maxPos[i]) * 0.5 for i in xrange(3)]

	def maxDim(self):
		sv = self.size()
		return max(sv[0], max(sv[1], sv[2]))

	def maxAxis(self):
		s = self.size()
		d = self.maxDim()
		if d == s[0]: return 0
		if d == s[1]: return 1
		return 2

	def write(self, f):
		f.writeQVec(self.minPos)
		f.writeQVec(self.maxPos)

class Poly:
	def __init__(self, bvh, prim):
		self.bvh = bvh
		self.prim = prim
		self.bbox = BBox()
		self.bbox.fromPoly(self)

def split(polyList, idx, count, pivot, axis):
	mid = 0
	for i in xrange(count):
		cent = polyList[idx + i].bbox.center()[axis]
		if cent < pivot:
			t = polyList[idx + i]
			polyList[idx + i] = polyList[idx + mid]
			polyList[idx + mid] = t
			mid += 1
	if mid == 0 or mid == count: mid = count / 2
	return mid

class Node:
	def __init__(self, bvh):
		self.bvh = bvh
		self.bbox = None
		self.primList = []
		self.left = None
		self.right = None
		self.bvh.nodeMap[self] = len(self.bvh.nodeList)
		self.bvh.nodeList.append(self)

	def addPrims(self, polyList, idx, count):
		self.bbox = BBox(polyList[idx].bbox)
		for i in xrange(1, count):
			self.bbox.merge(polyList[idx+i].bbox)
		for i in xrange(count):
			self.primList.append(polyList[idx+i].prim)

	def build(self, polyList, idx, count, axis, lvl):
		self.lvl = lvl
		if count == 1:
			self.addPrims(polyList, idx, count)
		elif count == 2:
			self.left = Node(self.bvh)
			self.left.addPrims(polyList, idx, 1)
			self.left.lvl = lvl + 1
			self.right = Node(self.bvh)
			self.right.addPrims(polyList, idx+1, 1)
			self.bbox = BBox(self.left.bbox)
			self.bbox.merge(self.right.bbox)
			self.right.lvl = lvl + 1
		else:
			self.bbox = BBox(polyList[idx].bbox)
			for i in xrange(1, count):
				self.bbox.merge(polyList[idx + i].bbox)
			mid = split(polyList, idx, count, self.bbox.center()[axis], axis)
			nextAxis = (axis + 1) % 3
			self.left = Node(self.bvh)
			self.right = Node(self.bvh)
			self.left.build(polyList, idx, mid, nextAxis, lvl + 1)
			self.right.build(polyList, idx + mid, count - mid, nextAxis, lvl + 1)

	def write(self, f):
		prim = -1
		left = -1
		right = -1
		if len(self.primList): prim = self.primList[0].number()
		if self.left: left = self.bvh.nodeMap[self.left]
		if self.right: right = self.bvh.nodeMap[self.right]
		if right < 0:
			f.writeI16(prim)
		else:
			f.writeI16(left)
		f.writeI16(right)

class BVH:
	def __init__(self, geo):
		self.geo = geo
		self.bbox = BBox()
		self.bbox.fromAABB(geo.boundingBox())
		self.polyList = []
		self.nodeList = []
		self.nodeMap = {}
		for prim in geo.prims():
			self.polyList.append(Poly(self, prim))
		self.root = Node(self)
		self.root.build(self.polyList, 0, len(self.polyList), self.bbox.maxAxis(), 0)

	def write(self, f):
		f.writeFOURCC("BVH")
		f.writeI32(len(self.nodeList))
		f.writeI32(0)
		f.writeI32(0)
		for node in self.nodeList: node.bbox.write(f)
		for node in self.nodeList: node.write(f)

if not globals().has_key("g_bvhName"):
	g_bvhName = "room"
if not globals().has_key("g_bvhPath"):
	g_bvhPath = os.getcwd()

outPath = g_bvhPath+"/"+g_bvhName+".bvh"
print outPath

geo = hou.node("obj/OBST/EXP").geometry()
bvh = BVH(geo)

out = BinFile()
out.open(outPath)
bvh.write(out)
out.close()

