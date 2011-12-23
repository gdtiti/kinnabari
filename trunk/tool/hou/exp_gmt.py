# This file is part of Kinnabari.
# Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
# Released under the terms of Version 3 of the GNU General Public License.
# See LICENSE.txt for details.

import sys
import hou
import os
import re
from math import *
from hou import hmath
from array import array


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
	def writeQV(self, v):
		n = len(v)
		if n > 4: n = 4
		for i in xrange(n): self.writeF32(v[i])
		for i in xrange(4-n): self.writeF32(0)
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

def writeAttrType(f, type):
	val = -1
	if type == hou.attribData.Int: val = 0
	if type == hou.attribData.Float: val = 1
	if type == hou.attribData.String: val = 2
	f.writeI16(val)

class MDL:
	def __init__(self, geo):
		self.geo = geo
		self.strList = []
		self.strMap = {}
		self.strCount = 0
		self.glbAttrList = []
		self.pntAttrList = []
		self.polAttrList = []
		self.mkGlbAttrList()
		self.mkPntAttrList()
		self.mkPolAttrList()

	def mkGlbAttrList(self):
		for attr in self.geo.geometry().globalAttribs():
			if attr.dataType() == hou.attribData.Int: self.glbAttrList.append(attr)
		for attr in self.geo.geometry().globalAttribs():
			if attr.dataType() == hou.attribData.Float: self.glbAttrList.append(attr)
		for attr in self.geo.geometry().globalAttribs():
			if attr.dataType() == hou.attribData.String: self.glbAttrList.append(attr)

	def mkPntAttrList(self):
		for attr in self.geo.geometry().pointAttribs():
			if attr.dataType() == hou.attribData.Int: self.pntAttrList.append(attr)
		for attr in self.geo.geometry().pointAttribs():
			if attr.dataType() == hou.attribData.Float:
				if attr.name() != "P" and attr.name() != "Pw": self.pntAttrList.append(attr)
		for attr in self.geo.geometry().pointAttribs():
			if attr.dataType() == hou.attribData.String: self.pntAttrList.append(attr)

	def mkPolAttrList(self):
		for attr in self.geo.geometry().primAttribs():
			if attr.dataType() == hou.attribData.Int: self.polAttrList.append(attr)
		for attr in self.geo.geometry().primAttribs():
			if attr.dataType() == hou.attribData.Float: self.polAttrList.append(attr)
		for attr in self.geo.geometry().primAttribs():
			if attr.dataType() == hou.attribData.String: self.polAttrList.append(attr)

	def addStr(self, str):
		if str not in self.strMap:
			self.strMap[str] = self.strCount
			self.strList.append(str)
			self.strCount += len(str) + 1
		return self.strMap[str]

	def writeHead(self, f):
		f.writeFOURCC("GMT") # +00 magic
		f.writeU32(len(self.geo.geometry().globalAttribs())) # +04
		f.writeU32(len(self.geo.geometry().pointAttribs()) - 2) # +08
		f.writeU32(len(self.geo.geometry().primAttribs())) # +0C
		f.writeU32(len(self.geo.geometry().points())) # +10
		f.writeU32(len(self.geo.geometry().prims())) # +14
		f.writeU32(0) # +18 offs_glb_attr
		f.writeU32(0) # +1C offs_pnt_attr
		f.writeU32(0) # +20 offs_pol_attr
		f.writeU32(0) # +24 offs_pnt
		f.writeU32(0) # +28 offs_pol
		f.writeU32(0) # +2C offs_str
		f.writeAABB(self.geo.geometry().boundingBox()) # +30

	def writeAttrInfo(self, f, list):
		offs = 0
		for attr in list:
			f.writeI32(self.addStr(attr.name()))
			f.writeI32(offs)
			offs += attr.size() * 4
			writeAttrType(f, attr.dataType())
			f.writeI16(attr.size())

	def writeGlbAttr(self, f):
		list = self.glbAttrList
		self.writeAttrInfo(f, list)
		geo = self.geo.geometry()
		for attr in list:
			if attr.dataType() == hou.attribData.Int:
				for val in geo.intListAttribValue(attr): f.writeI32(val)
			elif attr.dataType() == hou.attribData.Float:
				for val in geo.floatListAttribValue(attr): f.writeF32(val)
			elif attr.dataType() == hou.attribData.String:
				f.writeI32(self.addStr(geo.stringAttribValue(attr)))

	def writePntAttr(self, f):
		list = self.pntAttrList
		self.writeAttrInfo(f, list)
		for pnt in self.geo.geometry().points():
			for attr in list:
				if attr.dataType() == hou.attribData.Int:
					for val in pnt.intListAttribValue(attr): f.writeI32(val)
				elif attr.dataType() == hou.attribData.Float:
					for val in pnt.floatListAttribValue(attr): f.writeF32(val)
				elif attr.dataType() == hou.attribData.String:
					f.writeI32(self.addStr(pnt.stringAttribValue(attr)))

	def writePolAttr(self, f):
		list = self.polAttrList
		self.writeAttrInfo(f, list)
		for prim in self.geo.geometry().prims():
			for attr in list:
				if attr.dataType() == hou.attribData.Int:
					for val in prim.intListAttribValue(attr): f.writeI32(val)
				elif attr.dataType() == hou.attribData.Float:
					for val in prim.floatListAttribValue(attr): f.writeF32(val)
				elif attr.dataType() == hou.attribData.String:
					f.writeI32(self.addStr(prim.stringAttribValue(attr)))

	def writePnt(self, f):
		f.align(16)
		f.patchCur(0x24)
		for pnt in self.geo.geometry().points():
			f.writeFV(tuple(pnt.position()) + (pnt.weight(),))

	def writePol(self, f):
		f.align(16)
		f.patchCur(0x28)
		top = f.getPos()
		for i in xrange(len(self.geo.geometry().prims())):
			f.writeI32(0)
		for i, prim in enumerate(self.geo.geometry().prims()):
			f.patch(top + i*4, f.getPos() - top)
			f.writeI32(len(prim.vertices()))
			for vtx in prim.vertices():
				f.writeI32(vtx.point().number())

	def writeStr(self, f):
		nbStr = len(self.strList)
		if nbStr:
			f.align(16)
			f.patchCur(0x2C)
			for s in self.strList:
				f.writeStr(s)

	def write(self, f):
		self.writeHead(f)
		
		if len(self.glbAttrList):
			f.patchCur(0x18)
			self.writeGlbAttr(f)

		self.writePnt(f)
		if len(self.pntAttrList):
			f.patchCur(0x1C)
			self.writePntAttr(f)

		self.writePol(f)
		if len(self.polAttrList):
			f.patchCur(0x20)
			self.writePolAttr(f)

		self.writeStr(f)

	def save(self, outPath):
		out = BinFile()
		out.open(outPath)
		self.write(out)
		out.close()


if not globals().has_key("g_outName"): g_outName = "geom"
if not globals().has_key("g_outPath"): g_outPath = os.getcwd()

if 1: #not globals().has_key("g_geoName"):
	g_geoName = None
	objList = hou.node("/obj").children()
	for obj in objList:
		if obj.type().name() == "geo":
			if hou.node(obj.path() + "/EXP"):
				g_geoName = obj.name()
				break

if g_geoName:
	print g_geoName

	outPath = g_outPath+"/"+g_outName+".gmt"
	print outPath

	geo = hou.node("obj/"+g_geoName+"/EXP")
	mdl = MDL(geo)
	mdl.save(outPath)

