// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

using System;
using System.IO;
using System.Text;
using System.Globalization;
using System.Collections.Generic;

class txpk {

	enum TexFmt {
		INVALID = 0,
		DXT1 = 1,
		DXT3 = 2,
		DXT5 = 3
	}

	struct SurfDescr {
		public uint width;
		public uint height;
		public uint depth;
		public uint format;
		public uint nbMipMap;
		public uint size;
	}

	class Texture {
		public string fname;
		public SurfDescr dsc;
		public long fsize;
		public int topOffs;
		public TexFmt fmt;
		public uint width;
		public uint height;
		public uint dataSize;
		public uint lvlSize;
		public uint nbLvl;
		public List<byte[]> lvlList;
		public bool isCube;


		public void Load(string fname) {
			FileInfo fi = new FileInfo(fname);
			if (!fi.Exists) {
				throw new Exception(String.Format("File \"{0}\" not found", fname));
			}
			this.fname = fname;
			fsize = fi.Length;
			FileStream fs = File.OpenRead(fname);
			BinaryReader br = new BinaryReader(fs);
			uint magic = br.ReadUInt32();
			if (magic == FOURCC('D', 'D', 'S', ' ')) {
				ReadDDS(br);
			} else {
				throw new Exception(String.Format("Invalid data: 0x{0:X}", magic));
			}
			br.Close();
		}



		private void ReadDDS(BinaryReader br) {
			dsc = ReadDescr(br);
			TexFmt texFmt = GetTexFormat(dsc.format);
			if (texFmt == TexFmt.INVALID) {
				br.Close();
				Console.Error.WriteLine("Warning: unsupported data format 0x{0:X4}", dsc.format);
				return;
			}
			fmt = texFmt;
			long startPos = br.BaseStream.Position;
			int n = (int)dsc.nbMipMap;
			if (s_nomip) {
				n = 1;
			}
			if (n == 0) n = 1;
			int len = (int)dsc.size;
			lvlSize = dsc.size;
			nbLvl = (uint)n;
			width = dsc.width;
			height = dsc.height;
			dataSize = 0;
			lvlList = new List<byte[]>();
			int lw = (int)width;
			int lh = (int)height;
			for (int i = 0; i < n; ++i) {
				dataSize += (uint)len;
				byte[] texData = br.ReadBytes(len);
				lvlList.Add(texData);
				lw >>= 1;
				if (lw == 0) lw = 1;
				lh >>= 1;
				if (lh == 0) lh = 1;
				switch (fmt) {
					case TexFmt.DXT1:
						len = ((lw + 3) / 4) * ((lh + 3) / 4) * 8;
						break;
					default:
						len = ((lw + 3) / 4) * ((lh + 3) / 4) * 16;
						break;
				}
			}
	
			long endPos = br.BaseStream.Position;
		}

		public void LoadCubeDDS(string[] fnameList, string cubeName) {
			dataSize = 0;
			isCube = true;
			lvlList = new List<byte[]>();
			this.fname = cubeName;
			foreach (string fname in fnameList) {
				if (fname.Length > 0) {
					FileInfo fi = new FileInfo(fname);
					fsize = fi.Length;
					FileStream fs = File.OpenRead(fname);
					BinaryReader br = new BinaryReader(fs);
					uint magic = br.ReadUInt32();
					if (magic == FOURCC('D', 'D', 'S', ' ')) {
						ReadFaceDDS(br);
					} else {
						throw new Exception(String.Format("invalid data: 0x{0:X}", magic));
					}
					br.Close();
				}
			}
		}

		private void ReadFaceDDS(BinaryReader br) {
			dsc = ReadDescr(br);
			TexFmt texFmt = GetTexFormat(dsc.format);
			if (texFmt == TexFmt.INVALID) {
				br.Close();
				Console.Error.WriteLine("Warning: unsupported data format 0x{0:X4}", dsc.format);
				return;
			}
			fmt = texFmt;
			long startPos = br.BaseStream.Position;
			int n = (int)dsc.nbMipMap;
			if (s_nomip) {
				n = 1;
			}
			int len = (int)dsc.size;
			lvlSize = dsc.size;
			nbLvl = (uint)n;
			width = dsc.width;
			height = dsc.height;
			int lw = (int)width;
			int lh = (int)height;
			for (int i = 0; i < n; ++i) {
				dataSize += (uint)len;
				byte[] texData = br.ReadBytes(len);
				lvlList.Add(texData);
				lw >>= 1;
				if (lw == 0) lw = 1;
				lh >>= 1;
				if (lh == 0) lh = 1;
				switch (fmt) {
					case TexFmt.DXT1:
						len = ((lw + 3) / 4) * ((lh + 3) / 4) * 8;
						break;
					default:
						len = ((lw + 3) / 4) * ((lh + 3) / 4) * 16;
						break;
				}
			}
		}

		private static SurfDescr ReadDescr(BinaryReader br) {
			long fpos = br.BaseStream.Position;
			SurfDescr dsc = new SurfDescr();
			uint hsize = br.ReadUInt32();
			uint flags = br.ReadUInt32();
			dsc.height = br.ReadUInt32();
			dsc.width = br.ReadUInt32();
			dsc.size = br.ReadUInt32();
			dsc.depth = br.ReadUInt32();
			dsc.nbMipMap = br.ReadUInt32();
			br.BaseStream.Seek(11 * 4 + 2 * 4, SeekOrigin.Current);
			dsc.format = br.ReadUInt32();
			br.BaseStream.Seek(fpos + hsize, SeekOrigin.Begin);
			return dsc;
		}

		private static bool CkFormat(uint fmt) {
			return (fmt == FOURCC('D', 'X', 'T', '1')
					||
					fmt == FOURCC('D', 'X', 'T', '3')
					||
					fmt == FOURCC('D', 'X', 'T', '5')
			);
		}

		private static TexFmt GetTexFormat(uint fmt) {
			if (fmt == FOURCC('D', 'X', 'T', '1')) return TexFmt.DXT1;
			if (fmt == FOURCC('D', 'X', 'T', '3')) return TexFmt.DXT3;
			if (fmt == FOURCC('D', 'X', 'T', '5')) return TexFmt.DXT5;
			return TexFmt.INVALID;
		}
	}

	private static System.Collections.Generic.List<string> s_progArgs;
	private static System.Collections.Generic.Dictionary<string, string> s_progOpts;
	private static bool s_nomip;
	private static bool s_extInfo;
	private static string s_prefix;

	static txpk() {
		s_progArgs = new List<string>();
		s_progOpts = new Dictionary<string, string>();
	}

	private static void PrintUsage() {
		Console.WriteLine("usage: txpk <options> texlist.txt");
		Console.WriteLine("  options:");
		Console.WriteLine("    -o:output_name");
		Console.WriteLine("    -nomip:<1|0> (default 0)");
		Console.WriteLine("    -prefix:string");
	}

	private static void SetDefaults() {
		s_nomip = false;
		s_extInfo = true;
		s_prefix = "";
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

	private static uint FOURCC(char c1, char c2, char c3, char c4) {
		return (uint)((((byte)c4) << 24) | (((byte)c3) << 16) | (((byte)c2) << 8) | ((byte)c1));
	}

	private static long Align(long x, int y) {
		return ((x + (y - 1)) & (~(y - 1)));
	}

	private static void Patch32(BinaryWriter bw, long offs, int val) {
		long oldPos = bw.BaseStream.Position;
		bw.BaseStream.Seek(offs, SeekOrigin.Begin);
		bw.Write(val);
		bw.BaseStream.Seek(oldPos, SeekOrigin.Begin);
	}

	private static void PatchWithCurrPos32(BinaryWriter bw, long offs) {
		Patch32(bw, offs, (int)bw.BaseStream.Position);
	}

	static int Main(string[] args) {
		SetDefaults();
		ParseArgs(args);
		string fname = null;
		if (s_progArgs.Count != 0) {
			fname = s_progArgs[0];
		}
		if (s_progArgs.Count == 0) {
			PrintUsage();
			return -1;
		}

		string outName = null;
		if (s_progOpts.ContainsKey("o")) {
			outName = s_progOpts["o"];
		} else {
			outName = fname.Replace(".txt", ".tpk");
		}

		if (s_progOpts.ContainsKey("nomip")) {
			s_nomip = s_progOpts["nomip"] == "1" || s_progOpts["nomip"] == "true";
		}
		if (s_progOpts.ContainsKey("prefix")) {
			s_prefix = s_progOpts["prefix"];
		}

		StreamReader sr = File.OpenText(fname);
		List<string> files = new List<string>();
		while (!sr.EndOfStream) {
			files.Add(sr.ReadLine());
		}
		sr.Close();

		uint nbLvlTotal = 0;
		List<Texture> textures = new List<Texture>();
		int offs = 0;
		foreach (string texName in files) {
			if (texName.IndexOf('>') != -1) {
				string[] cubeList = texName.Split(' ', '>');
				string cubeName = cubeList[0];
				string[] faceList = new string[cubeList.Length - 1];
				Array.Copy(cubeList, 1, faceList, 0, faceList.Length);
				Texture tex = new Texture();
				//foreach (string faceName in faceList) {Console.Out.WriteLine(faceName);}
				tex.LoadCubeDDS(faceList, cubeName);
				textures.Add(tex);
				tex.topOffs = offs;
				offs += (int)tex.dataSize;
				nbLvlTotal += tex.nbLvl;
			} else {
				FileInfo fi = new FileInfo(texName);
				Texture tex = new Texture();
				textures.Add(tex);
				if (fi.Exists) {
					tex.Load(texName);
					tex.topOffs = offs;
					offs += (int)tex.dataSize;
					nbLvlTotal += tex.nbLvl;
				} else {
					Console.Error.WriteLine("Warning: file not found {0}", texName);
				}
			}
		}

		FileStream ofs = new FileStream(outName, FileMode.Create);
		BinaryWriter bw = new BinaryWriter(ofs);
		bw.Write(FOURCC('T', 'P', 'K', '\0')); // +00
		bw.Write(textures.Count); // +04
		long dataSizePos = bw.BaseStream.Position;
		bw.Write((int)0); // +08 data size
		bw.Write((int)0); // +0C ext info
		int listOffs = 0x10 + (textures.Count * 0x10);
		for (int i = 0; i < textures.Count; ++i) {
			bw.Write((byte)textures[i].fmt); // +00
			bw.Write((byte)textures[i].nbLvl); // +01
			bw.Write((ushort)textures[i].width); // +02
			bw.Write((ushort)textures[i].height); // +04
			short depth = 0;
			int nbFace = 1;
			if (textures[i].isCube) {
				depth = -1;
				nbFace = 6;
			}
			bw.Write((short)depth); // depth +06
			bw.Write(textures[i].lvlSize); // +08
			bw.Write(listOffs); // +0xC
			listOffs += (int)textures[i].nbLvl * nbFace * 4;
		}
		int dataOffs = (int)Align(listOffs, 0x10);
		int dataOrg = dataOffs;
		List<int> dataOffsList = new List<int>();
		List<long> offsPosList = new List<long>();
		for (int i = 0; i < textures.Count; ++i) {
			int nbFace = 1;
			if (textures[i].isCube) {
				nbFace = 6;
			}
			for (int n = 0; n < nbFace; ++n) {
				int len = (int)textures[i].lvlSize;
				for (int j = 0; j < textures[i].nbLvl; ++j) {
					if (nbFace == 1) {
						len = textures[i].lvlList[j].Length;
					}
					offsPosList.Add(bw.BaseStream.Position);
					bw.Write(dataOffs);
					dataOffsList.Add(dataOffs);
					dataOffs += (int)Align(len, 0x10);
					if (nbFace != 1) {
						len >>= 2;
					}
					len = (int)Align(len, 0x10);
				}
			}
		}
		int padLen = (int)(dataOrg - bw.BaseStream.Position);
		for (int i = 0; i < padLen; ++i) {
			bw.Write((byte)0xCC);
		}
		int dataOffsIdx = 0;
		for (int i = 0; i < textures.Count; ++i) {
			//Console.WriteLine("{0} @ {1:X}", textures[i].fname, bw.BaseStream.Position);
			int nbFace = 1;
			if (textures[i].isCube) {
				nbFace = 6;
			}
			for (int j = 0; j < textures[i].nbLvl * nbFace; ++j) {
				if (bw.BaseStream.Position != dataOffsList[dataOffsIdx]) {
					Console.Error.WriteLine("WARN: dataOffs mismatch, \"{0}\", 0x{1:X} != 0x{2:X}", textures[i].fname, bw.BaseStream.Position, dataOffsList[dataOffsIdx]);
					PatchWithCurrPos32(bw, offsPosList[dataOffsIdx]);
				}
				if (textures[i].lvlList[j].Length == 0) {
					Console.Error.WriteLine("WARN: zero-length level");
					for (int n = 0; n < 0x10; ++n) {
						bw.Write((byte)0);
					}
				} else {
					bw.Write(textures[i].lvlList[j]);
					padLen = (int)Align(textures[i].lvlList[j].Length, 0x10) - textures[i].lvlList[j].Length;
					for (int n = 0; n < padLen; ++n) {
						bw.Write((byte)0x00);
					}
				}
				++dataOffsIdx;
			}
		}

		if (s_extInfo) {
			padLen = (int)(Align(bw.BaseStream.Position, 0x10) - bw.BaseStream.Position);
			for (int i = 0; i < padLen; ++i) {
				bw.Write((byte)0xCC);
			}
			PatchWithCurrPos32(bw, 0xC);
			long extHeadPos = bw.BaseStream.Position;
			bw.Write((int)0); // +00 extHeadSize
			bw.Write((int)0); // +04 texNameListPos
			Patch32(bw, extHeadPos, (int)(bw.BaseStream.Position - extHeadPos));
			PatchWithCurrPos32(bw, extHeadPos + 4);
			long texNameListPos = bw.BaseStream.Position;
			for (int i = 0; i < textures.Count; ++i) {
				bw.Write((int)0);
			}
			for (int i = 0; i < textures.Count; ++i) {
				PatchWithCurrPos32(bw, texNameListPos + i*4);
				string texName = textures[i].fname;
				texName = texName.Replace(".dds", "");
				texName = s_prefix + texName;
				foreach (char c in texName) {
					bw.Write((byte)c);
				}
				bw.Write((byte)0);
			}
		}

		PatchWithCurrPos32(bw, dataSizePos);

		bw.Close();

		return 0;
	}

}

