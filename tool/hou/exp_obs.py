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
	def writeFOURCC(self, str):
		n = len(str)
		if n > 4: n = 4
		for i in xrange(n): self.writeU8(ord(str[i]))
		for i in xrange(4-n): self.writeU8(0)

def ckConvex(v0, v1, v2, v3):
	v = v3 - v1
	if v.cross(v0 - v1).dot(v.cross(v2 - v1)) >= 0: return False
	v = v2 - v0
	if v.cross(v3 - v0).dot(v.cross(v1 - v0)) >= 0: return False
	return True

def ckPlanar(vtx, n):
	d = vtx[0].dot(n)
	for v in vtx:
		if abs(v.dot(n) - d) > 1e-6: return False
	return True

class PolyAttr:
	def __init__(self): raise Error, "enum ctor"


PolyAttr.FLOOR = 1 << 0
PolyAttr.CEIL = 1 << 1
PolyAttr.WALL = 1 << 2

if not globals().has_key("g_obsName"):
	g_obsName = "room"
if not globals().has_key("g_obsPath"):
	g_obsPath = os.getcwd()

outPath = g_obsPath+"/"+g_obsName+".obs"
print outPath

geo = hou.node("obj/OBST/EXP")

out = BinFile()
out.open(outPath)
nbPnt = len(geo.geometry().points())
nbPol = len(geo.geometry().prims())
out.writeFOURCC("OBST")
out.writeI32(nbPnt)
out.writeI32(nbPol)

for pnt in geo.geometry().points():
	pos = pnt.position()
	out.writeF32(pos[0])
	out.writeF32(pos[1])
	out.writeF32(pos[2])

for i, prim in enumerate(geo.geometry().prims()):
	nvtx = len(prim.vertices())
	if nvtx > 4:
		print "Warning: invalid number of vertices in prim #", i
	else:
		attr = 0
		nrm = prim.normal()
		if nrm[1] > 0.5:
			attr |= PolyAttr.FLOOR
		elif nrm[1] < -0.5:
			attr |= PolyAttr.CEIL
		else:
			attr |= PolyAttr.WALL
		if nvtx == 4:
			p0 = prim.vertices()[3].point()
			p1 = prim.vertices()[2].point()
			p2 = prim.vertices()[1].point()
			p3 = prim.vertices()[0].point()
			if not ckConvex(p0.position(), p1.position(), p2.position(), p3.position()):
				print "Warning: non-convex prim #", i
			if not ckPlanar([p0.position(), p1.position(), p2.position(), p3.position()], nrm):
				print "Warning: non-planar prim #", i
			
			out.writeI16(p0.number())
			out.writeI16(p1.number())
			out.writeI16(p2.number())
			out.writeI16(p3.number())
		else:
			out.writeI16(prim.vertices()[2].point().number())
			out.writeI16(prim.vertices()[1].point().number())
			out.writeI16(prim.vertices()[0].point().number())
			out.writeI16(prim.vertices()[0].point().number())
		out.writeI32(attr)

out.close()


