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

g_jntMap = {}
g_rig = []
g_mtlList = []
g_mtlMap = {}
g_primGrpExpr = re.compile(r"prim_(\S+)")
g_jntExpr = re.compile(r"jnt_(\S+)")
g_jntNameExpr = re.compile(r"(\S+)/cregion")
g_lnkExpr = re.compile(r"lnk_(\S+)")

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
		self.id = pnt.number()
		self.pos = pnt.position()
		self.nrm = pnt.attribValue("N")
		self.tex = [0.0, 1.0]
		self.idx = []
		self.wgt = []
	
	def setUV(self, pnt):
		self.tex = pnt.attribValue("uv")

	def setSkin(self, pnt, omd):
		capt = pnt.attribValue("pCapt")
		for j in xrange(len(capt)/2):
			jntId = int(capt[j*2])
			jntWgt = capt[j*2 + 1] 
			if (jntId < 0 or jntWgt <= 0): break
			self.idx.append(omd.getJntId(jntId))
			self.wgt.append(jntWgt)

	def write(self, f):
		f.writeF32(self.pos[0])
		f.writeF32(self.pos[1])
		f.writeF32(self.pos[2])
		f.writeF32(self.nrm[0])
		f.writeF32(self.nrm[1])
		f.writeF32(self.nrm[2])
		f.writeF32(self.tex[0])
		f.writeF32(1.0 - self.tex[1]) # flip v
		n = len(self.idx)
		if n> 4:
			n = 4
			print "Too many weights for point #", self.id
			for j in self.idx:
				print "  :", g_rig[j].name
		for i in xrange(n):
			f.writeI8(self.idx[i])
		for i in xrange(4-n):
			f.writeI8(0)
		for i in xrange(n):
			f.writeF32(self.wgt[i])
		for i in xrange(4-n):
			f.writeF32(0)

class CullList:
	def __init__(self, geo):
		self.geo = geo
		self.idList = []
		self.sphList = []
		for node in geo.children():
			if node.type().name() == "object_merge" and g_lnkExpr.match(node.name()):
				jntName = node.name().lstrip("lnk_")
				jntId = g_jntMap[jntName]
				self.idList.append(jntId)
				sphNode = hou.node(node.parm("objpath1").evalAsString())
				x = sphNode.parm("tx").eval()
				y = sphNode.parm("ty").eval()
				z = sphNode.parm("tz").eval()
				r = sphNode.parm("radx").eval()
				sph = hou.Vector4(x, y, z, r)
				self.sphList.append(sph)
				#print jntName, "->", jntId, ":", sph

	def write(self, f):
		for sph in self.sphList: f.writeFV(sph)
		for id in self.idList: f.writeU8(id)

class Grp:
	def __init__(self, omd, primGrp):
		self.omd = omd
		self.primGrp = primGrp
		self.mtlId = -1
		self.polStart = len(omd.idxList)
		self.polCount = 0
		for prim in self.primGrp.prims(): self.addPrim(prim)
		mtlPath = primGrp.prims()[0].attribValue("shop_materialpath")
		self.clr = primGrp.prims()[0].attribValue("Cd")
		mtlGid = g_mtlMap[mtlPath];
		if not mtlGid in omd.mtlMap:
			omd.mtlMap[mtlGid] = len(omd.mtlList)
			omd.mtlList.append(mtlGid)
		self.mtlId = omd.mtlMap[mtlGid]
		self.cullList = None
		if self.omd.cullNode:
			self.cullList = CullList(self.omd.cullNode.node("csph_" + primGrp.name()))

	def addPrim(self, prim):
		if len(prim.vertices()) != 3: return
		for vtx in prim.vertices():
			pnt = vtx.point()
			pos = pnt.position()
			self.omd.idxList.append(pnt.number())
		self.polCount += 1

	def write(self, f):
		f.writeF32(self.clr[0])
		f.writeF32(self.clr[1])
		f.writeF32(self.clr[2])
		f.writeI32(self.mtlId)
		f.writeU32(self.polStart)
		f.writeU32(self.polCount)
		f.writeU32(0)
		f.writeU32(0)

class Jnt:
	def __init__(self, node):
		self.id = -1
		self.parentId = -1
		self.parentNode = node.inputConnectors()[0][0].inputNode()
		self.node = node
		self.name = node.name()
		self.pos = [node.parm("tx").eval(), node.parm("ty").eval(), node.parm("tz").eval()]

	def write(self, f):
		f.writeFV(self.pos) # +00
		f.writeI16(self.id) # +0C
		f.writeI16(self.parentId) # +0E

class OMD:
	def __init__(self, geo):
		self.geo = geo
		self.mtlList = []
		self.mtlMap = {}
		self.vtxList = []
		self.idxList = []
		cullPath = self.geo.parent().path() + "_csph"
		self.cullNode = hou.node(cullPath)
		uvAttr = geo.geometry().findPointAttrib("uv")
		for pnt in geo.geometry().points():
			vtx = Vtx(pnt)
			self.vtxList.append(vtx)
			if uvAttr: vtx.setUV(pnt)
			vtx.setSkin(pnt, self)
		self.grpList = []
		for grp in geo.geometry().primGroups():
			if g_primGrpExpr.match(grp.name()):
				self.grpList.append(Grp(self, grp))
	
	def getJntId(self, jntId):
		jntName = self.geo.geometry().findGlobalAttrib("pCaptPath").strings()[jntId]
		jntName = g_jntNameExpr.match(jntName).group(1)
		return g_jntMap[jntName]

	def writeMtlInfo(self, f):
		for i, gi in enumerate(self.mtlList):
			# print i, "->", gi, g_mtlList[gi].path()
			mtlNode = g_mtlList[gi].node("suboutput1").inputConnections()[0].inputNode()
			for parmName in ["sun_dir", "sun_color", "sun_param", "rim_color",  "rim_param"]:
				f.writeFV(mtlNode.parmTuple("g_toon_" + parmName).evalAsFloats())

	def writeCullInfo(self, f):
		flg = False
		for grp in self.grpList:
			if grp.cullList:
				flg = True
				break
		if not flg: return
		f.align(16)
		f.patchCur(0x24)
		topPos = f.getPos()
		f.writeFOURCC("CULL")
		sizePos = f.getPos()
		f.writeU32(0) # dataSize
		offsPos = f.getPos()
		for grp in self.grpList:
			f.writeU32(0)
			if grp.cullList:
				f.writeU32(len(grp.cullList.idList))
			else:
				f.writeU32(0)
		for i, grp in enumerate(self.grpList):
			if grp.cullList:
				f.align(16)
				f.patch(offsPos + i*8, f.getPos() - topPos);
				grp.cullList.write(f)
		size = f.getPos() - topPos
		f.patch(sizePos, size)

	def write(self, f):
		f.writeFOURCC("OMD") # +00 magic
		f.writeU32(len(self.mtlList)) # +04
		f.writeU32(len(self.grpList)) # +08
		f.writeU32(len(self.vtxList)) # +0C
		f.writeU32(len(self.idxList)) # +10
		f.writeU32(0) # +14 -> vtx
		f.writeU32(0) # +18 -> grp
		f.writeU32(0) # +1C -> idx
		f.writeU32(len(g_rig)) # +20
		f.writeU32(0) # +24 -> cull (optional)
		f.writeU32(0) # +28
		f.writeU32(0) # +2C
		self.writeMtlInfo(f)
		for jnt in g_rig: jnt.write(f)
		mtlNamePos = f.getPos()
		for i in xrange(len(self.mtlList)): f.writeU32(0)
		jntNamePos = f.getPos()
		for i in xrange(len(g_rig)): f.writeU32(0)
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
		for i, jnt in enumerate(g_rig):
			f.patchCur(jntNamePos + i*4);
			f.writeStr(jnt.name)
		self.writeCullInfo(f)

	def save(self):
		out = BinFile()
		out.open(g_outPath)
		self.write(out)
		out.close()

def hie(node):
	if node.name() != "root":
		if node.name() == "center" or g_jntExpr.match(node.name()):
			g_rig.append(Jnt(node))
	for link in node.outputConnectors()[0]:
		hie(link.outputNode())


if not globals().has_key("g_omdName"):
	g_omdName = "char"
if not globals().has_key("g_omdPath"):
	g_omdPath = os.getcwd()

g_outPath = g_omdPath+"/"+g_omdName+".omd"
print g_outPath

hie(hou.node("/obj/root"))

for i, jnt in enumerate(g_rig):
	jnt.id = i
	g_jntMap[jnt.name] = i
	# print jnt.name, "->", g_jntMap[jnt.name] 

for jnt in g_rig:
	parent = filter(lambda x: x.name == jnt.parentNode.name(), g_rig)
	if parent:
		jnt.parentId = parent[0].id


objList = hou.node("/obj/MTL").children()
for obj in objList:
	if obj.type().name() == "material":
		g_mtlMap[obj.path()] = len(g_mtlList)
		g_mtlList.append(obj)
		# print obj.path(), "->", g_mtlMap[obj.path()] 

geo = hou.node("obj/char_geo/EXP")
omd = OMD(geo)
omd.save()
