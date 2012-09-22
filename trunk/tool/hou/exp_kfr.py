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

g_FPS = hou.fps()
g_maxFrame = int(hou.hscript("frange")[0].split()[4])
g_chExpr = re.compile(r"/obj/(\S+)/(\S+)")
g_grpMap = {}

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


class ChAttr:
	def __init__(self): raise Error, "enum ctor"


ChAttr.TX = 1 << 0
ChAttr.TY = 1 << 1
ChAttr.TZ = 1 << 2
ChAttr.RX = 1 << 3
ChAttr.RY = 1 << 4
ChAttr.RZ = 1 << 5

def getChAttr(chName):
	if chName == 'tx': return ChAttr.TX
	elif chName == 'ty': return ChAttr.TY
	elif chName == 'tz': return ChAttr.TZ
	elif chName == 'rx': return ChAttr.RX
	elif chName == 'ry': return ChAttr.RY
	elif chName == 'rz': return ChAttr.RZ
	return 0

class Key:
	def __init__(self, val, lslope, rslope):
		self.val = val
		self.lslope = lslope
		self.rslope = rslope

	def write(self, f):
		f.writeF32(self.val)
		f.writeF32(self.lslope)
		f.writeF32(self.rslope)

class Chan:
	def __init__(self, grp, name):
		self.grp = grp
		self.name = name
		self.path = "/obj/"+grp.name+"/"+name
		parm = hou.parm(self.path)
		trk = parm.overrideTrack()
		if trk:
			print "Override:", self.path,"->",trk.name()
			self.keyframes = parm.getReferencedParm().keyframes()
		else:
			self.keyframes = parm.keyframes()
		self.attr = getChAttr(name)
		self.frmNoList = []
		for k in self.keyframes:
			if k.frame() <= g_maxFrame:
				self.frmNoList.append(int(k.frame()))
		self.keyList = []
		if len(self.frmNoList) > 0:
			rotFlg = self.attr & (ChAttr.RX | ChAttr.RY | ChAttr.RZ)
			for i, k in enumerate(self.keyframes):
				if k.frame() <= g_maxFrame:
					val = k.value()
					if rotFlg: val = hou.hmath.degToRad(val)
					lslope = 0
					rslope = 0
					if k.isSlopeSet():
						if not k.isSlopeTied():
							lslope = k.inSlope()
						else:
							lslope = k.slope()
						rslope = k.slope()
						if rotFlg:
							lslope = hou.hmath.degToRad(lslope)
							rslope = hou.hmath.degToRad(rslope)
						lslope /= g_FPS
						rslope /= g_FPS
					if i == 0: lslope = 0
					else: lslope *= k.frame() - self.keyframes[i-1].frame()
					if i == len(self.keyframes)-1: rslope = 0
					else: rslope *= self.keyframes[i+1].frame() - k.frame()
					self.keyList.append(Key(val, lslope, rslope))

	def write(self, f):
		if (len(self.frmNoList)):
			f.writeU16(self.attr)
			f.writeU16(len(self.frmNoList))
			for n in self.frmNoList: f.writeU16(n)
			f.align(4)
			for k in self.keyList:
				k.write(f)

class ChGrp:
	def __init__(self, name):
		self.name = name
		self.chList = []
		self.attr = 0

	def addCh(self, chName):
		self.chList.append(Chan(self, chName))
		self.attr |= getChAttr(chName)

	def getChCount(self):
		nbChan = 0
		for ch in self.chList:
			if len(ch.frmNoList):
				nbChan = nbChan + 1
		return nbChan

	def write(self, f):
		nbChan = self.getChCount()
		f.writeU32(0) # name offs
		f.writeU16(self.attr)
		f.writeU16(nbChan)
		chanListTop = f.getPos()
		for i in xrange(nbChan): # chan offs list
			f.writeU32(0)
		i = 0
		for ch in self.chList:
			if len(ch.frmNoList):
				f.patchCur(chanListTop + i*4)
				ch.write(f)
				i = i + 1


chList = hou.hscript("chgls -l EXP")[0].split()
del chList[0] # remove group name from the list
for chName in chList:
	m = g_chExpr.match(chName)
	grpName = m.group(1)
	if not grpName in g_grpMap:
		#print grpName
		g_grpMap[grpName] = ChGrp(grpName)
	g_grpMap[grpName].addCh(m.group(2))

#print "maxFrame =", g_maxFrame


if not globals().has_key("g_kfrName"):
	g_kfrName = "anim"
if not globals().has_key("g_kfrPath"):
	g_kfrPath = os.getcwd()

outPath = g_kfrPath + "/" + g_kfrName + ".kfr"
print outPath
out = BinFile()
out.open(outPath)
out.writeFOURCC("KFR") # +00 magic
out.writeI16(int(g_maxFrame)) # +04
nbGrp = 0
for g in g_grpMap.keys():
	if g_grpMap[g].getChCount(): nbGrp = nbGrp + 1
out.writeI16(nbGrp) # +06
grpListTop = out.getPos()
for i in xrange(nbGrp):
	out.writeU32(0)
nameOffsList = []
i = 0
for g in g_grpMap.keys():
	if g_grpMap[g].getChCount():
		nameOffsList.append(out.getPos())
		out.patchCur(grpListTop + i*4)
		g_grpMap[g].write(out)
		i = i + 1
i = 0
for g in g_grpMap.keys():
	if g_grpMap[g].getChCount():
		out.patchCur(nameOffsList[i])
		out.writeStr(g_grpMap[g].name)
		i = i + 1
out.close()
