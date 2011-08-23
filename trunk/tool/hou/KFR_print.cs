// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

using System;
using System.IO;
using System.Collections.Generic;

class KFR_print {

	public static int g_maxFrame;

	[Flags]
	public enum ATTR {
		TX = 1 << 0,
		TY = 1 << 1,
		TZ = 1 << 2,
		RX = 1 << 3,
		RY = 1 << 4,
		RZ = 1 << 5
	}

	public class Key {
		public float val, lslope, rslope;

		public void Read(BinaryReader br) {
			val = br.ReadSingle();
			lslope = br.ReadSingle();
			rslope = br.ReadSingle();
		}
	}

	public class Chan {
		public long fileOffs;
		public ATTR attr;
		public int nbKey;
		public int[] frmNoList;
		public Key[] keyList;

		public void Read(BinaryReader br) {
			fileOffs = br.BaseStream.Position;
			attr = (ATTR)br.ReadUInt16();
			nbKey = br.ReadUInt16();
			frmNoList = new int[nbKey];
			for (int i = 0; i < nbKey; ++i) {
				frmNoList[i] = br.ReadUInt16();
			}
			br.BaseStream.Seek(Align(br.BaseStream.Position, 4), SeekOrigin.Begin);
			keyList = new Key[nbKey];
			for (int i = 0; i < nbKey; ++i) {
				keyList[i] = new Key();
				keyList[i].Read(br);
			}
		}

		public float Eval(int frameNo) {
			if (nbKey < 2 || frameNo == frmNoList[0]) {
				return keyList[0].val;
			}
			if (frameNo == frmNoList[nbKey - 1]) {
				return keyList[nbKey - 1].val;
			}
			for (int i = 0; i < nbKey - 1; ++i) {
				if (frameNo == frmNoList[i]) {
					return keyList[i].val;
				}
				if (frameNo > frmNoList[i] && frameNo < frmNoList[i + 1]) {
					float v0 = keyList[i].val;
					float outgoing = keyList[i].rslope;
					float v1 = keyList[i + 1].val;
					float incoming = keyList[i + 1].lslope;
					float start = frmNoList[i];
					float end = frmNoList[i + 1];

					float t = (frameNo - start) / (end - start);
					float v = Hermite(v0, outgoing, v1, incoming, t);
					return v;
				}
			}
			throw new Exception(String.Format("can't evaluate channel at frame {0}", frameNo));
		}

		public void Write(TextWriter tw) {
			float s = 1.0f;
			if ((attr & (ATTR.RX | ATTR.RY | ATTR.RZ)) != 0) s = (float)(180.0f / Math.PI);
			tw.WriteLine("  chn @ 0x{0:X}, attr = {1}, nbKey = {2}", fileOffs, attr, nbKey);
			for (int i = 0; i < nbKey; ++i) {
				tw.WriteLine("    {0}: <{1} {2} {3}>", frmNoList[i], keyList[i].val * s, keyList[i].lslope * s, keyList[i].rslope * s);
			}
			tw.WriteLine();
			for (int i = 0; i <= g_maxFrame; ++i) {
				float val = Eval(i);
				tw.WriteLine("    {0}: {1}", i, val * s);
			}
		}
	}

	public class Grp {
		public long fileOffs;
		public string name;
		public ATTR attr;
		public int nbChan;
		public int[] chOffs;
		public Chan[] ch;

		public void Read(BinaryReader br) {
			fileOffs = br.BaseStream.Position;
			int nameOffs = br.ReadInt32();
			attr = (ATTR)br.ReadUInt16();
			nbChan = br.ReadUInt16();
			chOffs = new int[nbChan];
			for (int i = 0; i < nbChan; ++i) {
				chOffs[i] = br.ReadInt32();
			}
			ch = new Chan[nbChan];
			for (int i = 0; i < nbChan; ++i) {
				ch[i] = new Chan();
				br.BaseStream.Seek(chOffs[i], SeekOrigin.Begin);
				ch[i].Read(br);
			}
			br.BaseStream.Seek(nameOffs, SeekOrigin.Begin);
			name = ReadStr(br);
		}

		public void Write(TextWriter tw) {
			tw.WriteLine("{0} @ 0x{1:X}, attr = {2}", name, fileOffs, attr);
			for (int i = 0; i < nbChan; ++i) {
				ch[i].Write(tw);
				tw.WriteLine();
			}
			tw.WriteLine();
		}
	}

	public static float Hermite(float p0, float m0, float p1, float m1, float t) {
		float tt = t * t;
		float ttt = tt * t;
		float tt3 = tt * 3.0f;
		float tt2 = tt + tt;
		float ttt2 = tt2 * t;
		float h00 = ttt2 - tt3 + 1.0f;
		float h10 = ttt - tt2 + t;
		float h01 = -ttt2 + tt3;
		float h11 = ttt - tt;
		return (h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1);
	}

	public static float Rad2Deg(float rad) {
		return (float)(((rad) * 180.0f) / Math.PI);
	}

	public static uint FOURCC(char c1, char c2, char c3, char c4) {
		return (uint)((((byte)c4) << 24) | (((byte)c3) << 16) | (((byte)c2) << 8) | ((byte)c1));
	}

	public static long Align(long x, int y) {
		return ((x + (y - 1)) & (~(y - 1)));
	}

	public static string ReadStr(BinaryReader br) {
		string s = "";
		while (true) {
			char c = (char)br.ReadByte();
			if (c == 0) break;
			s += c;
		}
		return s;
	}

	public static BinaryReader FileOpen(string fname) {
		FileInfo fi = new FileInfo(fname);
		long fsize = fi.Length;
		FileStream fs = File.OpenRead(fname);
		BinaryReader br = new BinaryReader(fs);
		return br;
	}

	static void Main(string[] args) {
		if (args == null || args.Length < 1) {
			Console.Error.WriteLine("KFR_print <file>");
			return;
		}
		string fname = args[0];
		BinaryReader br = FileOpen(fname);
		uint magic = br.ReadUInt32();
		if (magic != FOURCC('K','F','R','\0')) {
			throw new Exception("KFR");
		}
		g_maxFrame = br.ReadInt16();
		int nbGrp = br.ReadInt16();
		Console.WriteLine("maxFrame = {0}", g_maxFrame);
		Console.WriteLine("nbGrp = {0}", nbGrp);
		Console.WriteLine();
		int[] grpOffs = new int[nbGrp];
		for (int i = 0; i < nbGrp; ++i) {
			grpOffs[i] = br.ReadInt32();
		}
		Grp[] grp = new Grp[nbGrp];
		for (int i = 0; i < nbGrp; ++i) {
			br.BaseStream.Seek(grpOffs[i], SeekOrigin.Begin);
			grp[i] = new Grp();
			grp[i].Read(br);
		}

		TextWriter tw = Console.Out;
		for (int i = 0; i < nbGrp; ++i) {
			tw.WriteLine("Grp # {0}", i);
			grp[i].Write(tw);
			tw.WriteLine();
		}

		br.Close();
	}
}