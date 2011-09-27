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
	def __init__(self, pos = None, rad = None):
		if not pos: pos = hou.Vector3()
		if not rad: rad = hou.Vector3()
		self.minPos = pos - rad
		self.maxPos = pos + rad

	def write(self, f):
		f.writeQVec(self.minPos)
		f.writeQVec(self.maxPos)

class Node:
	def __init__(self, bvh, bbox, left, right, prim):
		self.bvh = bvh
		self.bbox = bbox
		self.left = left
		self.right = right
		self.prim = prim
	
	def write(self, f):
		if self.right < 0:
			f.writeI16(self.prim)
		else:
			f.writeI16(self.left)
		f.writeI16(self.right)

class BVH:
	def __init__(self, geo):
		self.geo = geo
		self.nodeList = []
		radAttr = geo.findPointAttrib("bboxRad")
		leftAttr = geo.findPointAttrib("left")
		rightAttr = geo.findPointAttrib("right")
		primAttr = geo.findPointAttrib("prim")
		for pnt in geo.points():
			pos = hou.Vector3(pnt.position())
			rad = hou.Vector3(pnt.attribValue(radAttr))
			left = pnt.attribValue(leftAttr)
			right = pnt.attribValue(rightAttr)
			prim = pnt.attribValue(primAttr)
			node = Node(self, BBox(pos, rad), left, right, prim)
			self.nodeList.append(node)

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

geo = hou.node("obj/OBST/EXP_BVH").geometry()
bvh = BVH(geo)

out = BinFile()
out.open(outPath)
bvh.write(out)
out.close()
