# This file is part of Kinnabari.
# Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
# Released under the terms of Version 3 of the GNU General Public License.
# See LICENSE.txt for details.

import sys
import hou
import os
import re
from math import *
from array import array

g_laneGrpExpr = re.compile(r"lane_(\S+)")
g_areaGrpExpr = re.compile(r"area_(\S+)")

def align(x, y): return ((x + (y - 1)) & (~(y - 1)))

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
	def writeFV(self, v):
		for x in v: self.writeF32(x)
	def writeFOURCC(self, str):
		n = len(str)
		if n > 4: n = 4
		for i in xrange(n): self.writeU8(ord(str[i]))
		for i in xrange(4-n): self.writeU8(0)
	def writeStr(self, str):
		for c in str: self.writeU8(ord(c))
		self.writeU8(0)
		
	def writeAABB(self, AABB):
		self.writeF32(AABB.minvec()[0])
		self.writeF32(AABB.minvec()[1])
		self.writeF32(AABB.minvec()[2])
		self.writeF32(1.0)
		self.writeF32(AABB.maxvec()[0])
		self.writeF32(AABB.maxvec()[1])
		self.writeF32(AABB.maxvec()[2])
		self.writeF32(1.0)

	def patch(self, offs, val):
		nowPos = self.getPos()
		self.seek(offs)
		self.writeU32(val)
		self.seek(nowPos)

	def patchCur(self, offs): self.patch(offs, self.getPos())

	def align(self, x):
		pos0 = self.getPos()
		pos1 = align(pos0, x)
		for i in xrange(pos1-pos0): self.writeU8(0xFF)

class Vtx:
	def __init__(self, pnt):
		self.pos = pnt.position()
		self.uv = pnt.attribValue("uv")

	def write(self, f):
		f.writeFV(self.pos)
		f.writeF32(self.uv[0])
		f.writeF32(self.uv[1])

class Lane:
	def __init__(self, grp):
		self.name = g_laneGrpExpr.match(grp.name()).group(1)
		self.AABB = None
		self.area = None
		self.vtxList = []
		for prim in grp.prims():
			if len(prim.vertices()) != 4: return
			polVtxList = []
			for vtx in prim.vertices():
				pnt = vtx.point()
				polVtxList.append(Vtx(pnt))
				pos = pnt.position()
				if self.AABB:
					self.AABB.enlargeToContain(pos)
				else:
					self.AABB = hou.BoundingBox(pos[0], pos[1], pos[2], pos[0], pos[1], pos[2])
			self.vtxList.append(polVtxList[3])
			self.vtxList.append(polVtxList[2])
			self.vtxList.append(polVtxList[1])
			self.vtxList.append(polVtxList[0])

	def addArea(self, grp):
		for prim in grp.prims():
			for vtx in prim.vertices():
				pos = vtx.point().position()
				if self.area:
					self.area.enlargeToContain(pos)
				else:
					self.area = hou.BoundingBox(pos[0], pos[1], pos[2], pos[0], pos[1], pos[2])

	def write(self, f):
		if len(self.vtxList) > 0:
			f.writeAABB(self.AABB)
			for v in self.vtxList: v.write(f)
		


if not globals().has_key("g_lanName"):
	g_lanName = "room"
if not globals().has_key("g_lanPath"):
	g_lanPath = os.getcwd()

outPath = g_lanPath+"/"+g_lanName+".lan"
print outPath


geo = hou.node("obj/LANE/EXP")

out = BinFile()
out.open(outPath)
out.writeFOURCC("LANE")
laneList = []
laneMap = {}
for grp in geo.geometry().primGroups():
	if g_laneGrpExpr.match(grp.name()):
		lane = Lane(grp)
		laneList.append(lane)
		laneMap[lane.name] = lane
for grp in geo.geometry().primGroups():
	m = g_areaGrpExpr.match(grp.name())
	if m:
		name = m.group(1)
		lane = laneMap[name]
		if lane: lane.addArea(grp)

out.writeI32(len(laneList))
tblPos = out.getPos()
for lane in laneList:
	out.writeI32(0) # -> name
	out.writeI32(0) # -> info
	out.writeI32(0) # -> area (optional)
	out.writeI32(0) # -> nb_vtx
for i, lane in enumerate(laneList):
	out.align(16)
	out.patchCur(tblPos + i*0x10 + 4)
	out.patch(tblPos + i*0x10 + 0xC, len(lane.vtxList))
	lane.write(out)
out.align(16)
for i, lane in enumerate(laneList):
	if lane.area:
		out.patchCur(tblPos + i*0x10 + 8)
		out.writeAABB(lane.area)
for i, lane in enumerate(laneList):
	out.patchCur(tblPos + i*0x10)
	out.writeStr(lane.name)
out.close()
