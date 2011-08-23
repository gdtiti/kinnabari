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

g_mtlList = []
g_mtlMap = {}
g_primGrpExpr = re.compile(r"prim_(\S+)")

def align(x, y): return ((x + (y - 1)) & (~(y - 1)))

def halfFromBits(bits):
	e = ((bits & 0x7F800000) >> 23) - 127 + 15
	if e < 0: return 0
	if e > 31: e = 31
	s = bits & (1<<31)
	m = bits & 0x7FFFFF
	return ((s >> 16) & 0x8000) | ((e << 10) & 0x7C00) | ((m >> 13) & 0x3FF)

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
	def writeF16(self, val):
		if val == 0:
			self.writeI16(0)
			return
		arr = array('f', [val]).tostring()
		bits = ord(arr[0]) | (ord(arr[1]) << 8) | (ord(arr[2]) << 16) | (ord(arr[3]) << 24)
		self.writeU16(halfFromBits(bits))
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
		self.nrm = pnt.attribValue("N")
		self.clr = [1.0, 1.0, 1.0]
		self.tex = [0.0, 1.0]

	def setColor(self, pnt):
		self.clr = pnt.attribValue("Cd")
	
	def setUV(self, pnt):
		self.tex = pnt.attribValue("uv")

	def write(self, f):
		f.writeF32(self.pos[0])
		f.writeF32(self.pos[1])
		f.writeF32(self.pos[2])
		f.writeF32(self.nrm[0])
		f.writeF32(self.nrm[1])
		f.writeF32(self.nrm[2])
		f.writeF32(self.clr[0])
		f.writeF32(self.clr[1])
		f.writeF32(self.clr[2])
		f.writeF32(self.tex[0])
		f.writeF32(1.0 - self.tex[1]) # flip v

class Grp:
	def __init__(self, rmd, primGrp):
		self.rmd = rmd
		self.primGrp = primGrp
		self.AABB = None
		self.mtlId = -1
		self.polStart = len(rmd.idxList)
		self.polCount = 0
		for prim in self.primGrp.prims(): self.addPrim(prim)
		mtlPath = primGrp.prims()[0].attribValue("shop_materialpath")
		mtlGid = g_mtlMap[mtlPath];
		if not mtlGid in rmd.mtlMap:
			rmd.mtlMap[mtlGid] = len(rmd.mtlList)
			rmd.mtlList.append(mtlGid)
		self.mtlId = rmd.mtlMap[mtlGid]

	def addPrim(self, prim):
		if len(prim.vertices()) != 3: return
		for vtx in prim.vertices():
			pnt = vtx.point()
			pos = pnt.position()
			if self.AABB:
				self.AABB.enlargeToContain(pos)
			else:
				self.AABB = hou.BoundingBox(pos[0], pos[1], pos[2], pos[0], pos[1], pos[2])
			self.rmd.idxList.append(pnt.number())
		self.polCount += 1

	def write(self, f):
		f.writeAABB(self.AABB)
		f.writeI32(self.mtlId)
		f.writeU32(self.polStart)
		f.writeU32(self.polCount)
		f.writeU32(0)

class RMD:
	def __init__(self, geo):
		self.geo = geo
		self.mtlList = []
		self.mtlMap = {}
		self.vtxList = []
		self.idxList = []
		cdAttr = geo.geometry().findPointAttrib("Cd")
		uvAttr = geo.geometry().findPointAttrib("uv")
		for pnt in geo.geometry().points():
			vtx = Vtx(pnt)
			self.vtxList.append(vtx)
			if cdAttr: vtx.setColor(pnt)
			if uvAttr: vtx.setUV(pnt)
		self.grpList = []
		for grp in geo.geometry().primGroups():
			if g_primGrpExpr.match(grp.name()):
				self.grpList.append(Grp(self, grp))

	def writeMtlInfo(self, f):
		for i, gi in enumerate(self.mtlList):
			print i, "->", gi, g_mtlList[gi].path()
			mtlNode = g_mtlList[gi].node("suboutput1").inputConnections()[0].inputNode()
			for parmName in ["sun_dir", "sun_color", "sun_param", "rim_color",  "rim_param"]:
				f.writeFV(mtlNode.parmTuple("g_toon_" + parmName).evalAsFloats())

	def write(self, f):
		f.writeFOURCC("RMD") # +00 magic
		f.writeU32(len(self.mtlList)) # +04
		f.writeU32(len(self.grpList)) # +08
		f.writeU32(len(self.vtxList)) # +0C
		f.writeU32(len(self.idxList)) # +10
		f.writeU32(0) # +14
		f.writeU32(0) # +18
		f.writeU32(0) # +1C
		f.writeAABB(self.geo.geometry().boundingBox()) # +20
		self.writeMtlInfo(f)
		mtlNamePos = f.getPos()
		for i in xrange(len(self.mtlList)): f.writeU32(0)
		f.align(16)
		f.patchCur(0x14)
		for grp in self.grpList: grp.write(f)
		f.patchCur(0x18)
		for vtx in self.vtxList: vtx.write(f)
		f.patchCur(0x1C)
		for idx in self.idxList: f.writeU16(idx)
		for i, gi in enumerate(self.mtlList):
			f.patchCur(mtlNamePos + i*4);
			f.writeStr(g_mtlList[gi].name())
	
	def save(self):
		out = BinFile()
		out.open(g_outPath)
		self.write(out)
		out.close()


if not globals().has_key("g_rmdName"):
	g_rmdName = "room"
if not globals().has_key("g_rmdPath"):
	g_rmdPath = os.getcwd()
if not globals().has_key("g_geoName"):
	g_geoName = "ROOM"

g_outPath = g_rmdPath+"/"+g_rmdName+".rmd"
print g_outPath

objList = hou.node("/obj/MTL").children()
for obj in objList:
	if obj.type().name() == "material":
		g_mtlMap[obj.path()] = len(g_mtlList)
		g_mtlList.append(obj)
		print obj.path(), "->", g_mtlMap[obj.path()] 

geo = hou.node("obj/"+g_geoName+"/EXP")
rmd = RMD(geo)
rmd.save()

