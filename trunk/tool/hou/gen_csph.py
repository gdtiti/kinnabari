# This file is part of Kinnabari.
# Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
# Released under the terms of Version 3 of the GNU General Public License.
# See LICENSE.txt for details.

import sys
import hou
import re
from math import *
from array import array

g_jntMap = {}
g_rig = []

g_primGrpExpr = re.compile(r"prim_(\S+)")
g_jntExpr = re.compile(r"jnt_(\S+)")
g_jntNameExpr = re.compile(r"(\S+)/cregion")

class Vtx:
	def __init__(self, pnt):
		self.id = pnt.number()
		self.pos = pnt.position()
		self.idx = []
		self.wgt = []
	
	def setSkin(self, pnt, omd):
		capt = pnt.attribValue("pCapt")
		for j in xrange(len(capt)/2):
			jntId = int(capt[j*2])
			jntWgt = capt[j*2 + 1] 
			if (jntId < 0 or jntWgt <= 0): break
			self.idx.append(omd.getJntId(jntId))
			self.wgt.append(jntWgt)

class BSph:
	def __init__(self, grp, sphNode, lnkNode, jntId):
		self.grp = grp
		self.sphNode = sphNode
		self.lnkNode = lnkNode
		self.jntId = jntId
		self.imin = [-1, -1, -1]
		self.imax = [-1, -1, -1]
		self.center = hou.Vector3(0, 0, 0)
		self.radius = 0

	def update(self, idx):
		vlist = self.grp.omd.vtxList
		v = vlist[idx]
		for i in xrange(3):
			if self.imin[i] < 0 or v.pos[i] < vlist[self.imin[i]].pos[i]: self.imin[i] = idx
			if self.imax[i] < 0 or v.pos[i] > vlist[self.imax[i]].pos[i]: self.imax[i] = idx

	def estimate(self):
		dv = []
		vlist = self.grp.omd.vtxList
		for i in xrange(3):
			dv.append(vlist[self.imax[i]].pos - vlist[self.imin[i]].pos)
		d2x = dv[0].dot(dv[0])
		d2y = dv[1].dot(dv[1])
		d2z = dv[2].dot(dv[2])
		minIdx = self.imin[0]
		maxIdx = self.imax[0]
		if d2y > d2x and d2y > d2z:
			minIdx = self.imin[1]
			maxIdx = self.imax[1]
		if d2z > d2x and d2z > d2y:
			minIdx = self.imin[2]
			maxIdx = self.imax[2]
		maxPos = vlist[maxIdx].pos
		self.center = (vlist[minIdx].pos + maxPos) * 0.5
		diff = maxPos - self.center
		self.radius = sqrt(diff.dot(diff))
	
	def grow(self, pos):
		v = pos - self.center
		dist2 = v.dot(v)
		if dist2 > self.radius*self.radius:
			dist = sqrt(dist2)
			r = (self.radius + dist) * 0.5
			s = (r - self.radius) / dist
			self.radius = r
			self.center += v * s

class Grp:
	def __init__(self, omd, primGrp):
		self.omd = omd
		self.primGrp = primGrp
		self.polStart = len(omd.idxList)
		self.polCount = 0
		for prim in self.primGrp.prims(): self.addPrim(prim)
		self.cullGeo = None
		self.sphList = []
		self.sphMap = {}

	def addPrim(self, prim):
		if len(prim.vertices()) != 3: return
		for vtx in prim.vertices():
			self.omd.idxList.append(vtx.point().number())
		self.polCount += 1

	def addJntSph(self):
		for i in xrange(self.polCount):
			for n in xrange(3):
				vidx = self.omd.idxList[self.polStart + i*3 + n]
				v = self.omd.vtxList[vidx]
				for j in xrange(len(v.idx)):
					jntId = v.idx[j]
					jntWgt = v.wgt[j]
					jnt = g_rig[jntId]
					sphName = "sph_" + jnt.name
					if not self.cullGeo.node(sphName):
						sphNode = self.cullGeo.createNode("sphere", sphName)
						sphNode.setParms({"type":5, "surftype":2}) # Primitive Type: Bezier, Connectivity: Rows and Columns
						sphNode.setParms({"orderu":3, "orderv":3, "imperfect":0})
						sphNode.setParmExpressions({"rady":'ch("radx")', "radz":'ch("radx")'})
						# Object Merge
						lnkName = "lnk_" + jnt.name;
						lnkNode = self.cullGeo.createNode("object_merge", lnkName)
						lnkNode.setParms({"xformpath":jnt.node.path(), "objpath1":sphNode.path()})
						lnkNode.setParms({"invertxform":1})
						#
						bsph = BSph(self, sphNode, lnkNode, jntId)
						self.sphList.append(bsph)
						self.sphMap[jntId] = bsph
					bsph = self.sphMap[jntId]
					bsph.update(vidx)
		# center (global)
		for bsph in self.sphList: bsph.estimate()
		# radius
		for i in xrange(self.polCount):
			for n in xrange(3):
				vidx = self.omd.idxList[self.polStart + i*3 + n]
				v = self.omd.vtxList[vidx]
				for j in xrange(len(v.idx)):
					jntId = v.idx[j]
					bsph = self.sphMap[jntId]
					bsph.grow(v.pos)
		for bsph in self.sphList:
			bsph.center = bsph.center - g_rig[bsph.jntId].wpos
			bsph.sphNode.setParms({"tx":bsph.center[0], "ty":bsph.center[1], "tz":bsph.center[2], "radx":bsph.radius})
		# merge
		mrgNode = self.cullGeo.createNode("merge", "merge")
		hou.hscript("opset -d on " + mrgNode.path())
		for bsph in self.sphList:
			mrgNode.setNextInput(bsph.lnkNode)


class Jnt:
	def __init__(self, node):
		self.id = -1
		self.parentId = -1
		self.parentNode = node.inputConnectors()[0][0].inputNode()
		self.node = node
		self.name = node.name()
		self.pos = [node.parm("tx").eval(), node.parm("ty").eval(), node.parm("tz").eval()]
		wm = node.worldTransform()
		self.wpos = hou.Vector3(wm.at(3, 0), wm.at(3, 1), wm.at(3, 2))

class OMD:
	def __init__(self, geo):
		self.geo = geo
		self.vtxList = []
		self.idxList = []
		for pnt in geo.geometry().points():
			vtx = Vtx(pnt)
			self.vtxList.append(vtx)
			vtx.setSkin(pnt, self)
		self.grpList = []
		for grp in geo.geometry().primGroups():
			if g_primGrpExpr.match(grp.name()):
				self.grpList.append(Grp(self, grp))
	

	def getJntId(self, jntId):
		jntName = self.geo.geometry().findGlobalAttrib("pCaptPath").strings()[jntId]
		jntName = g_jntNameExpr.match(jntName).group(1)
		return g_jntMap[jntName]

	def mkCullGeo(self):
		cullName = self.geo.parent().name() + "_csph"
		print cullName
		cullNet = hou.node("/obj").createNode("subnet", cullName)
		for grp in self.grpList:
			grp.cullGeo = cullNet.createNode("geo", "csph_" + grp.primGrp.name())
			grp.cullGeo.children()[0].destroy() # delete default file node
		for grp in self.grpList:
			grp.addJntSph()


def hie(node):
	if node.name() != "root":
		if node.name() == "center" or g_jntExpr.match(node.name()):
			g_rig.append(Jnt(node))
	for link in node.outputConnectors()[0]:
		hie(link.outputNode())

hie(hou.node("/obj/root"))

for i, jnt in enumerate(g_rig):
	jnt.id = i
	g_jntMap[jnt.name] = i
	# print jnt.name, "->", g_jntMap[jnt.name] 

for jnt in g_rig:
	parent = filter(lambda x: x.name == jnt.parentNode.name(), g_rig)
	if parent:
		jnt.parentId = parent[0].id

geo = hou.node("obj/char_geo/EXP")
omd = OMD(geo)
omd.mkCullGeo()
