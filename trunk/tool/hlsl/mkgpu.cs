// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

using System;
using System.IO;
using System.Text;
using System.Collections;
using System.Globalization;
using System.Collections.Generic;

class mkgpu {
	static string g_baseDir;
	static string g_codeDir;
	static string g_binDir;
	static List<Prog> g_vtxProgLst = new List<Prog>();
	static List<Prog> g_pixProgLst = new List<Prog>();
	static Dictionary<string, Param> g_paramTbl = new Dictionary<string, Param>();

	enum PARAMTYPE {
		FVEC = 0,
		IVEC = 1,
		FLOAT = 2,
		INT = 3,
		SMP = 4,
		BOOL = 5
	};

	class SymTbl {
		private static Dictionary<string, int> map = new Dictionary<string, int>();
		private static List<byte> store = new List<byte>();

		public static int GetId(string name) {
			int id = -1;
			if (map.ContainsKey(name)) {
				id = map[name];
			} else {
				id = store.Count;
				foreach (char c in name) {
					store.Add((byte)c);
				}
				store.Add((byte)0);
				map[name] = id;
			}
			return id;
		}

		public static void Write(BinaryWriter bw) {
			foreach (byte b in store) {bw.Write(b);}
		}
	}

	class Param {
		private ParamList lst;
		public string name;
		public int nameId;
		public int reg;
		public int len;
		public PARAMTYPE type;
		public int offs;

		public Param(ParamList lst) {
			this.lst = lst;
			offs = -1;
		}

		public void Read(BinaryReader br) {
			/* see PARAM_INFO in hlsl_tool */
			int nameOffs = br.ReadUInt16();
			reg = br.ReadUInt16();
			len = br.ReadUInt16();
			type = (PARAMTYPE)br.ReadUInt16();
			br.BaseStream.Seek(lst.nameBase + nameOffs, SeekOrigin.Begin);
			name = ReadStr(br);
			if (name.Substring(0, 2) == "g_") {
				name = name.Substring(2);
			}
			if (g_paramTbl.ContainsKey(name)) {
				if (len > g_paramTbl[name].len) {
					g_paramTbl[name] = this;
				}
			} else {
				g_paramTbl.Add(name, this);
			}
			nameId = SymTbl.GetId(name);
		}

		public void Write(BinaryWriter bw) {
			// RDR_GPARAM_INFO
			int id = g_paramTbl[name].offs | ((int)type << 13); // RDR_GPARAM_ID
			bw.Write((ushort)id);
			bw.Write((ushort)reg);
			bw.Write((ushort)len);
			bw.Write((ushort)nameId);
		}
	}

	class ParamList {
		public Param[] lst;
		public int nameBase;

		public void Read(BinaryReader br) {
			int n = br.ReadInt32();
			nameBase = 4 + (n * 8); // sizeof(int) + sizeof(PARAM_INFO)
			lst = new Param[n];
			for (int i = 0; i < n; ++i) {
				lst[i] = new Param(this);
				br.BaseStream.Seek(4 + (i * 8), SeekOrigin.Begin);
				lst[i].Read(br);
			}
		}

		public int Count { get { return lst.Length; } }

		public void Load(string name) {
			BinaryReader br = FileOpen(GetPath(name + ".prm"));
			Read(br);
			br.Close();
		}

		public void Write(BinaryWriter bw) {
			foreach (Param param in lst) {
				param.Write(bw);
			}
		}
	}

	class Prog {
		public string name;
		public ParamList prm;
		public byte[] cod;
		public int nameId;

		public void Load(string name) {
			nameId = SymTbl.GetId(name);
			this.name = name;
			prm = new ParamList();
			prm.Load(name);

			BinaryReader br = FileOpen(GetPath(name + ".cod"));
			cod = br.ReadBytes((int)br.BaseStream.Length);
			br.Close();
		}

		public void WriteInfo(BinaryWriter bw) {
			bw.Write((ushort)nameId); // +00
			bw.Write((ushort)prm.Count); // +02
			bw.Write((int)0); // +04 -> params
			bw.Write((int)0); // +08 -> code
			bw.Write((int)cod.Length); // +0C
		}

		public void WriteParams(BinaryWriter bw) {
			prm.Write(bw);
		}

		public void WriteCode(BinaryWriter bw) {
			bw.Write(cod);
		}
	}

	private static List<string> s_progArgs = new List<string>();
	private static Dictionary<string, string> s_progOpts = new Dictionary<string, string>();

	private static string GetPath(string fname) {
		return g_baseDir + Path.DirectorySeparatorChar + fname;
	}

	private static void PrintUsage() {
		Console.Error.WriteLine("mkgpu <proglist.txt>");
	}

	private static void SetDefaults() {
	}

	private static bool IsOption(string arg) {
		return arg.StartsWith("-") || arg.StartsWith("/");
	}

	private static int ParseInt(string s) {
		try {
			NumberStyles nstyles = NumberStyles.AllowExponent | NumberStyles.AllowDecimalPoint | NumberStyles.AllowLeadingSign;
			long i = Int64.Parse(s, nstyles);
			return (int)i;
		} catch {
			return -1;
		}
	}

	private static void ParseArgs(string[] args) {
		if (args == null || args.Length == 0) return;
		foreach (string arg in args) {
			if (IsOption(arg)) {
				int sep = arg.IndexOf(":");
				if (sep == -1) {
					s_progOpts.Add(arg.Substring(1), "");
				} else {
					s_progOpts.Add(arg.Substring(1, sep - 1), arg.Substring(sep + 1));
				}
			} else {
				s_progArgs.Add(arg);
			}
		}
	}

	public static void Patch32(BinaryWriter bw, long offs, int val) {
		long oldPos = bw.BaseStream.Position;
		bw.BaseStream.Seek(offs, SeekOrigin.Begin);
		bw.Write(val);
		bw.BaseStream.Seek(oldPos, SeekOrigin.Begin);
	}

	public static void PatchWithCurrPos32(BinaryWriter bw, long offs) {
		Patch32(bw, offs, (int)bw.BaseStream.Position);
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
		if (!fi.Exists) {
			throw new Exception(String.Format("File \"{0}\" not found", fname));
		}
		long fsize = fi.Length;
		FileStream fs = File.OpenRead(fname);
		BinaryReader br = new BinaryReader(fs);
		return br;
	}

	public static TextWriter MakeTextFile(string fname) {
		FileStream ofs = new FileStream(fname, FileMode.Create);
		TextWriter tw = new StreamWriter(ofs);
		return tw;
	}

	public static uint FOURCC(char c1, char c2, char c3, char c4) {
		return (uint)((((byte)c4) << 24) | (((byte)c3) << 16) | (((byte)c2) << 8) | ((byte)c1));
	}

	public static long Align(long x, int y) {
		return ((x + (y - 1)) & (~(y - 1)));
	}

	public static int Align32(int x, int y) {
		return ((x + (y - 1)) & (~(y - 1)));
	}

	static void WriteOneParam(TextWriter tw, Param param) {
		if (param.type == PARAMTYPE.SMP) {
			tw.WriteLine("\tRDR_SAMPLER {0};", param.name);
		} else {
			string tstr = "UVEC";
			if (param.type == PARAMTYPE.FLOAT) {
				tstr = "float";
			} else if (param.type == PARAMTYPE.INT) {
				tstr = "sys_i32";
			} else if (param.type == PARAMTYPE.BOOL) {
				tstr = "sys_byte";
			}
			tw.WriteLine("\t{0} {1}{2};", tstr, param.name, param.len > 1 ? String.Format("[{0}]", param.len) : "");
		}
	}

	class ParamGrp {
		private PARAMTYPE type;
		public int count;
		public string topName;

		public ParamGrp(PARAMTYPE type) {
			this.type = type;
			count = 0;
		}

		public void Write(TextWriter tw) {
			count = 0;
			int offs = 0;
			foreach (string name in g_paramTbl.Keys) {
				Param param = g_paramTbl[name];
				if (param.type == type) {
					param.offs = offs;
					WriteOneParam(tw, param);
					if (count == 0) {
						topName = param.name;
					}
					++count;
					offs += param.len;
				}
			}
		}

		public void WriteTop(TextWriter tw) {
			tw.WriteLine("#define D_RDR_GPTOP_{0} {1}", type, count > 0 ? String.Format("D_FIELD_OFFS(RDR_GPARAM, {0})", topName) : "(-1)");
		}

		public void WriteRel(TextWriter tw) {
			foreach (string name in g_paramTbl.Keys) {
				Param param = g_paramTbl[name];
				if (param.type == type) {
					tw.WriteLine("#define D_RDR_GP_{0} (0x{1:X})", param.name, param.offs);
				}
			}
		}
	}


	static ParamGrp g_grpFvec = new ParamGrp(PARAMTYPE.FVEC);
	static ParamGrp g_grpIvec = new ParamGrp(PARAMTYPE.IVEC);
	static ParamGrp g_grpF = new ParamGrp(PARAMTYPE.FLOAT);
	static ParamGrp g_grpI = new ParamGrp(PARAMTYPE.INT);
	static ParamGrp g_grpSmp = new ParamGrp(PARAMTYPE.SMP);
	static ParamGrp g_grpBool = new ParamGrp(PARAMTYPE.BOOL);

	static void WriteParams(TextWriter tw) {
		g_grpFvec.Write(tw);
		g_grpIvec.Write(tw);
		g_grpF.Write(tw);
		g_grpI.Write(tw);
		g_grpSmp.Write(tw);
		g_grpBool.Write(tw);
	}

	static void WriteTops(TextWriter tw) {
		g_grpFvec.WriteTop(tw);
		g_grpIvec.WriteTop(tw);
		g_grpF.WriteTop(tw);
		g_grpI.WriteTop(tw);
		g_grpSmp.WriteTop(tw);
		g_grpBool.WriteTop(tw);
	}

	static void WriteRels(TextWriter tw) {
		g_grpFvec.WriteRel(tw);
		g_grpIvec.WriteRel(tw);
		g_grpF.WriteRel(tw);
		g_grpI.WriteRel(tw);
		g_grpSmp.WriteRel(tw);
		g_grpBool.WriteRel(tw);
	}

	static void WriteProgs(TextWriter tw, List<Prog> lst) {
		int idx = 0;
		foreach (Prog prog in lst) {
			tw.WriteLine("#define D_RDRPROG_{0} (0x{1:X})", prog.name, idx);
			++idx;
		}
	}

	static void WriteBin() {
		int nbVtx = g_vtxProgLst.Count;
		int nbPix = g_pixProgLst.Count;
		FileStream fs = File.Create(g_binDir + Path.DirectorySeparatorChar + "prog.gpu");
		BinaryWriter bw = new BinaryWriter(fs);
		bw.Write(FOURCC('G', 'P', 'U', '\0')); // +00
		bw.Write((int)nbVtx); // +04
		bw.Write((int)nbPix); // +08
		bw.Write((int)0); // +0C symTblOffs
		long vtxHeadTop = bw.BaseStream.Position;
		foreach (Prog prog in g_vtxProgLst) {
			prog.WriteInfo(bw);
		}
		long pixHeadTop = bw.BaseStream.Position;
		foreach (Prog prog in g_pixProgLst) {
			prog.WriteInfo(bw);
		}

		int offs;

		offs = 0;
		foreach (Prog prog in g_vtxProgLst) {
			PatchWithCurrPos32(bw, vtxHeadTop + offs + 4);
			offs += 0x10;
			prog.WriteParams(bw);
		}
		offs = 0;
		foreach (Prog prog in g_pixProgLst) {
			PatchWithCurrPos32(bw, pixHeadTop + offs + 4);
			offs += 0x10;
			prog.WriteParams(bw);
		}

		offs = 0;
		foreach (Prog prog in g_vtxProgLst) {
			PatchWithCurrPos32(bw, vtxHeadTop + offs + 8);
			offs += 0x10;
			prog.WriteCode(bw);
		}
		offs = 0;
		foreach (Prog prog in g_pixProgLst) {
			PatchWithCurrPos32(bw, pixHeadTop + offs + 8);
			offs += 0x10;
			prog.WriteCode(bw);
		}

		PatchWithCurrPos32(bw, 0xC);
		SymTbl.Write(bw);

		bw.Close();
	}

	static int Main(string[] args) {
		SetDefaults();
		ParseArgs(args);
		string listName = @"..\..\bin\hlsl\proglist.txt";
		if (s_progArgs.Count == 0) {
			//PrintUsage();
			//return -1;
		} else {
			listName = s_progArgs[0];
		}
		StreamReader sr = File.OpenText(listName);
		List<string> progList = new List<string>();
		while (!sr.EndOfStream) {
			string progName = sr.ReadLine();
			Console.WriteLine(progName);
			progList.Add(progName);
		}
		sr.Close();

		g_baseDir = Path.GetDirectoryName(Path.GetFullPath(listName));
		//Console.WriteLine(g_baseDir);

		g_codeDir = @"..\..\src\gen";
		g_binDir = @"..\..\bin\data\sys";

		foreach (string progName in progList) {
			Prog prog = new Prog();
			prog.Load(progName);
			if (progName.StartsWith("vtx_")) {
				g_vtxProgLst.Add(prog);
			} else {
				g_pixProgLst.Add(prog);
			}
		}

		TextWriter tw = MakeTextFile(g_codeDir + Path.DirectorySeparatorChar + "gparam.h");
		tw.WriteLine(@"typedef struct _RDR_GPARAM {");
		WriteParams(tw);
		tw.WriteLine(@"} RDR_GPARAM;");
		tw.WriteLine();
		WriteTops(tw);
		tw.WriteLine();
		WriteRels(tw);
		tw.WriteLine();
		WriteProgs(tw, g_vtxProgLst);
		tw.WriteLine();
		WriteProgs(tw, g_pixProgLst);
		tw.Close();

		WriteBin();

		return 0;
	}
}
