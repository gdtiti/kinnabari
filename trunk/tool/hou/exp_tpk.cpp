// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable: 4996)

#include <UT/UT_VectorTypes.h>
#include <SYS/SYS_Types.h>
#include <UT/UT_Version.h>
#include <PRM/PRM_Include.h>
#include <OP/OP_Director.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Vector3.h>
#include <TIL/TIL_Plane.h>
#include <TIL/TIL_Sequence.h>
#include <TIL/TIL_Region.h>
#include <TIL/TIL_Raster.h>
#include <COP2/COP2_Node.h>
#include <COP2/COP2_Resolver.h>
#include <CMD/CMD_Args.h>
#include <CMD/CMD_Manager.h>

#include <intrin.h>
#include <windows.h>
#include <tchar.h>
#include <d3dx9.h>

enum eTexFmt {
	E_TEXFMT_INVALID = 0,
	E_TEXFMT_DXT1    = 1,
	E_TEXFMT_DXT3    = 2,
	E_TEXFMT_DXT5    = 3,
	E_TEXFMT_ARGB32  = 4
};

enum eTexKind {
	E_TEXKIND_COMMON = 0,
	E_TEXKIND_BASE   = 1,
	E_TEXKIND_BUMP   = 2,
	E_TEXKIND_MASK   = 3
};

struct sTGA {
	uint8 mHead[18];
	uint8 mPixels[1];

	void init(int w, int h) {
		::memset(this, 0, sizeof(sTGA));
		mHead[2] = 2;
		mHead[12] = w & 0xFF;
		mHead[13] = (w>>8) & 0xFF;
		mHead[14] = h & 0xFF;
		mHead[15] = (h>>8) & 0xFF;
		mHead[16] = 32;
		mHead[17] = 2<<4;
	}

	uint32* get_pixels() {
		return reinterpret_cast<uint32*>(mPixels);
	}

	int get_w() const {
		return mHead[12] + (mHead[13]<<8);
	}

	int get_h() const {
		return mHead[14] + (mHead[15]<<8);
	}

	int get_size() {
		return calc_size(get_w(), get_h());
	}

	static int calc_size(int w, int h) {
		return 18 + w*h*4;
	}
	static sTGA* create(int w, int h) {
		int size = calc_size(w, h);
		sTGA* pTGA = reinterpret_cast<sTGA*>(::malloc(size));
		if (pTGA) {
			pTGA->init(w, h);
		}
		return pTGA;
	}

	static void destroy(sTGA* pTGA) {
		if (pTGA) {
			::free(pTGA);
		}
	}
};

static void bin_save(const char* fname, void* pData, int len) {
	FILE* f;
	::fopen_s(&f, fname, "w+b");
	if (f) {
		::fwrite(pData, 1, len, f);
		::fclose(f);
	}
}

static inline uint32 align32(uint32 x, uint32 y) {
	return ((x + (y - 1)) & (~(y - 1)));
}

class cBin {
private:
	FILE* mpFile;

public:
	cBin() : mpFile(0) {
	}
	~cBin() {
		close();
	}

	void open(const char* fpath) {
		::fopen_s(&mpFile, fpath, "w+b");
	}

	void close() {
		if (mpFile) {
			::fclose(mpFile);
			mpFile = 0;
		}
	}

	void write(const void* pData, int size) {
		if (mpFile) {
			::fwrite(pData, 1, size, mpFile);
		}
	}

	void write_u32(uint32 val) {
		write(&val, 4);
	}

	void write_u16(uint16 val) {
		write(&val, 2);
	}

	void write_u8(uint8 val) {
		write(&val, 1);
	}

	uint32 get_pos() const {
		if (mpFile) {
			return (uint32)::ftell(mpFile);
		}
		return 0;
	}

	void seek(uint32 pos) {
		if (mpFile) {
			::fseek(mpFile, pos, SEEK_SET);
		}
	}

	void patch_u32(uint32 pos, uint32 val) {
		uint32 nowPos = get_pos();
		seek(pos);
		write_u32(val);
		seek(nowPos);
	}

	void patch_u16(uint32 pos, uint16 val) {
		uint32 nowPos = get_pos();
		seek(pos);
		write_u16(val);
		seek(nowPos);
	}

	void patch_u8(uint32 pos, uint8 val) {
		uint32 nowPos = get_pos();
		seek(pos);
		write_u8(val);
		seek(nowPos);
	}

	void patch_cur(uint32 pos) {
		patch_u32(pos, get_pos());
	}

	void align(int x = 0x10) {
		uint32 pos0 = get_pos();
		uint32 pos1 = align32(pos0, x);
		uint32 n = pos1 - pos0;
		for (uint32 i = 0; i < n; ++i) {
			write_u8(0xFF);
		}
	}

	void write_str(const char* pStr) {
		if (!pStr) return;
		int len = ::strlen(pStr);
		write(pStr, len+1);
	}
};

class cDDS {
public:
	struct sPixelFormat {
		uint32 mSize;
		uint32 mFlags;
		uint32 mFourCC;
		uint32 mBitCount;
		uint32 mMaskR;
		uint32 mMaskG;
		uint32 mMaskB;
		uint32 mMaskA;

		bool is_dxt() const {
			return !!(mFlags & 4);
		}

		int get_num_chan() const {
			int n = 0;
			if (is_dxt()) {
				switch (mFourCC) {
					case D3DFMT_DXT1:
						n = 3;
						break;
					case D3DFMT_DXT3:
					case D3DFMT_DXT5:
						n = 4;
						break;
				}
			} else {
				n = mBitCount / 4;
			}
			return n;
		}

		int calc_size(int w, int h) {
			int size = 0;
			if (is_dxt()) {
				switch (mFourCC) {
					case D3DFMT_DXT1:
						size = ((w+3)/4) * ((h+3)/4) * 8;
						break;
					case D3DFMT_DXT3:
					case D3DFMT_DXT5:
						size = ((w+3)/4) * ((h+3)/4) * 16;
						break;
				}
			} else {
				return w*h*get_num_chan();
			}
			return size;
		}
	};
	struct sHead {
		uint32 mSize;
		uint32 mFlags;
		uint32 mHeight;
		uint32 mWidth;
		uint32 mPitchLin;
		uint32 mDepth;
		uint32 mMipMapCount;
		uint32 mReserved1[11];
		sPixelFormat mFormat;
		uint32 mCaps1;
		uint32 mCaps2;
		uint32 mReserved2[3];

		int get_mip_num() const {
			int n = mMipMapCount;
			if (0 == n) ++n;
			return n;
		}
	};

private:
	sHead* mpHead;
	void* mpData;
	int32 mDataSize;

public:
	cDDS() : mpData(0), mDataSize(0), mpHead(0) {
	}

	void set_data(void* pData, int size) {
		int magic = *reinterpret_cast<uint32*>(pData);
		if (magic != MAKEFOURCC('D','D','S',' ')) {
			cerr << "Internal error: not a DDS." << endl;
			return;
		}
		mpData = pData;
		mDataSize = size;
		mpHead = reinterpret_cast<sHead*>((uint8*)mpData + 4);
	}

	int get_mip_num() const {
		return mpHead ? mpHead->get_mip_num() : 0;
	}

	uint8* get_pix_top() const {
		if (mpHead) {
			return reinterpret_cast<uint8*>(mpHead) + mpHead->mSize;
		}
		return 0;
	}

	int get_w() const {
		return mpHead ? mpHead->mWidth : 0;
	}

	int get_h() const {
		return mpHead ? mpHead->mHeight : 0;
	}

	int calc_size(int w, int h) const {
		if (mpHead) {
			return mpHead->mFormat.calc_size(w, h);
		}
		return 0;
	}
};

struct sTexInfo {
	COP2_Node* mpNode;
	char* mpName;
	int mFormat;
	bool mAlpha;
	int mMipNum;
	uint32 mMipOffs[16];
	uint16 mWidth;
	uint16 mHeight;

	sTexInfo() : mpNode(0), mpName(0), mFormat(E_TEXFMT_ARGB32), mAlpha(false), mMipNum(0), mWidth(0), mHeight(0) {}

	const char* get_name() const {
		return (const char*)mpName;
	}

	eTexKind get_kind() const {
		UT_String name(get_name());
		if (name.startsWith("BASE_") || name.endsWith("_BASE")) {
			return E_TEXKIND_BASE;
		}
		if (name.startsWith("BUMP_") || name.endsWith("_BUMP")) {
			return E_TEXKIND_BUMP;
		}
		if (name.startsWith("MASK_") || name.endsWith("_MASK")) {
			return E_TEXKIND_MASK;
		}
		return E_TEXKIND_COMMON;
	}

	D3DFORMAT get_d3d_fmt() {
		D3DFORMAT fmt = D3DFMT_A8R8G8B8;
		switch (mFormat) {
			case E_TEXFMT_ARGB32:
				fmt = D3DFMT_A8R8G8B8;
				break;
			case E_TEXFMT_DXT1:
				fmt = D3DFMT_DXT1;
				break;
			case E_TEXFMT_DXT3:
				fmt = D3DFMT_DXT3;
				break;
			case E_TEXFMT_DXT5:
				fmt = D3DFMT_DXT5;
				break;
			default:
				break;
		}
		return fmt;
	}
};

class cCOP2TEX {
private:
	IDirect3D9* mpD3D;
	IDirect3DDevice9* mpDev;
	COP2_Node* mpCOP;
	UT_String* mpOutPath;
	sTexInfo* mpInfo;
	char* mpStrMem;
	int mTexNum;
	int mStrMemSize;
	bool mDumpDDS;
	bool mDumpTGA;
	cBin mBin;

	void set_defaults() {
		mDumpDDS = false;
		mDumpTGA = false;
	}
	bool init_device(CMD_Args& args) {
		mpD3D = ::Direct3DCreate9(D3D_SDK_VERSION);
		if (!mpD3D) {
			args.err() << "Can't create D3D." << endl;
			return false;
		}
		HWND hWnd = ::GetDesktopWindow();
		UINT adapter = D3DADAPTER_DEFAULT;
		DWORD devFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED;
		D3DPRESENT_PARAMETERS presentParams;
		::memset(&presentParams, 0, sizeof(D3DPRESENT_PARAMETERS));
		presentParams.hDeviceWindow = hWnd;
		presentParams.Windowed = TRUE;
		presentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
		presentParams.BackBufferFormat = D3DFMT_A8R8G8B8;
		presentParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		presentParams.BackBufferWidth = 1;
		presentParams.BackBufferHeight = 1;
		presentParams.EnableAutoDepthStencil = TRUE;
		presentParams.AutoDepthStencilFormat = D3DFMT_D24S8;
		HRESULT hres;
		hres = mpD3D->CreateDevice(adapter, D3DDEVTYPE_HAL, hWnd, devFlags, &presentParams, &mpDev);
		if (FAILED(hres)) {
			args.err() << "Can't create D3D device." << endl;
			mpD3D->Release();
			mpD3D = 0;
			return false;
		}
		return true;
	}

	void write_mips(int idx, cDDS* pDDS) {
		sTexInfo* pInfo = &mpInfo[idx];
		int nmip = pDDS->get_mip_num();
		pInfo->mMipNum = nmip;
		uint8* pSrc = pDDS->get_pix_top();
		int w = pDDS->get_w();
		int h = pDDS->get_h();
		pInfo->mWidth = (uint16)w;
		pInfo->mHeight = (uint16)h;
		for (int i = 0; i < nmip; ++i) {
			int size = pDDS->calc_size(w, h);
			mBin.align(16);
			pInfo->mMipOffs[i] = mBin.get_pos();
			mBin.write(pSrc, size);
			pSrc += size;
			w >>= 1;
			if (0 == w) w = 1;
			h >>= 1;
			if (0 == h) h = 1;
		}
	}

	void cvt_tex(int idx, CMD_Args& args) {
		sTexInfo* pInfo = &mpInfo[idx];
		if (!pInfo || !pInfo->mpNode) {
			return;
		}
		UT_String copPath;
		pInfo->mpNode->getFullPath(copPath);
		TIL_CopResolver* pRsl = TIL_CopResolver::getResolver();
		int rid;
		TIL_Raster* pRast = COP2_Resolver::getRaster(copPath, rid);
		if (!pRast) {
			args.err() << "Can't resolve raster for " << pInfo->get_name() << "." << endl;
			return;
		}
		if (pRast->getFormat() != PXL_INT8) {
			args.out() << "Warning: data format is not INT8 for " << pInfo->get_name() << "." << endl;
			TIL_CopResolver::doneWithRaster(pRast);
			pRast = pRsl->getNodeRaster(copPath, "C", "A", false, 0, PXL_INT8);
			if (!pRast) {
				args.err() << "Can't convert raster for " << pInfo->get_name() << "." << endl;
				return;
			}
		}
		int w = pRast->getXres();
		int h = pRast->getYres();
		sTGA* pTGA = sTGA::create(w, h);
		if (pTGA) {
			/* NOTE: pDst writes below are unaligned. */
			int x, y;
			uint32* pDst = pTGA->get_pixels();
			uint32* pSrc = reinterpret_cast<uint32*>(pRast->getPixels());
			pSrc += (h-1)*w;
			bool alphaFlg = false;
			if (pInfo->get_kind() == E_TEXKIND_MASK) {
				pInfo->mFormat = E_TEXFMT_DXT1;
				for (y = 0; y < h; ++y) {
					for (x = 0; x < w; ++x) {
						uint32 c = *pSrc++;
						c = _rotl(c, 8);
						c |= 0xFF;
						c = _byteswap_ulong(c);
						*pDst++ = c;
					}
					pSrc -= w*2;
				}
			} else {
				for (y = 0; y < h; ++y) {
					for (x = 0; x < w; ++x) {
						uint32 c = *pSrc++;
						c = _rotl(c, 8);
						alphaFlg |= !!((uint8)c != 0xFF);
						c = _byteswap_ulong(c);
						*pDst++ = c;
					}
					pSrc -= w*2;
				}
				if (alphaFlg) {
					pInfo->mFormat = E_TEXFMT_DXT5;
				} else {
					pInfo->mFormat = E_TEXFMT_DXT1;
				}
			}
			pInfo->mAlpha = alphaFlg;
			if (mDumpTGA) {
				UT_String pathTGA = *mpOutPath;
				pathTGA += pInfo->get_name();
				pathTGA += ".tga";
				args.out() << "TGA: " << pathTGA << endl;
				bin_save(pathTGA, pTGA, pTGA->get_size());
			}

			D3DXIMAGE_INFO info;
			IDirect3DTexture9* pTex = 0;
			HRESULT hres = ::D3DXCreateTextureFromFileInMemoryEx(mpDev, pTGA, pTGA->get_size(), 0, 0, 0, 0, pInfo->get_d3d_fmt(), D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_FILTER_BOX, 0, &info, NULL, &pTex);
			if (SUCCEEDED(hres)) {
				LPD3DXBUFFER pBuf = 0;
				hres = ::D3DXSaveTextureToFileInMemory(&pBuf, D3DXIFF_DDS, pTex, NULL);
				if (SUCCEEDED(hres)) {
					if (mDumpDDS) {
						UT_String pathDDS = *mpOutPath;
						pathDDS += pInfo->get_name();
						pathDDS += ".dds";
						args.out() << "DDS: " << pathDDS << endl;
						bin_save(pathDDS, pBuf->GetBufferPointer(), pBuf->GetBufferSize());
					}
					cDDS dds;
					dds.set_data(pBuf->GetBufferPointer(), pBuf->GetBufferSize());
					write_mips(idx, &dds);
					pBuf->Release();
				} else {
					args.err() << "Can't create DDS for " << pInfo->get_name() << "." << endl;
				}
				pTex->Release();
			} else {
				args.err() << "Can't create texture for " << pInfo->get_name() << "." << endl;
			}
		} else {
			args.err() << "Can't create TGA for " << pInfo->get_name() << "." << endl;
		}

		sTGA::destroy(pTGA);
		TIL_CopResolver::doneWithRaster(pRast);
	}

public:
	cCOP2TEX() : mpD3D(0), mpDev(0), mpCOP(0), mpOutPath(0), mpInfo(0), mTexNum(0), mpStrMem(0), mStrMemSize(0) {
		set_defaults();
	}

	~cCOP2TEX() {
		if (mpDev) {
			mpDev->Release();
			mpDev = 0;
		}
		if (mpD3D) {
			mpD3D->Release();
			mpD3D = 0;
		}
		if (mpOutPath) {
			delete mpOutPath;
			mpOutPath = 0;
		}
		if (mpInfo) {
			delete [] mpInfo;
			mpInfo = 0;
		}
		if (mpStrMem) {
			delete [] mpStrMem;
			mpStrMem = 0;
		}
	}

	void init(CMD_Args& args) {
		mpCOP = 0;
		mpD3D = 0;
		mpDev = 0;
		if (args.argc() < 2) {
			args.err() << "cop2tex <path>" << endl;
			return;
		}
		if (!init_device(args)) {
			return;
		}
		set_defaults();
		if (args.found('d')) {
			mDumpDDS = true;
		}
		if (args.found('t')) {
			mDumpTGA = true;
		}
		UT_String outPath;
		if (args.found('o')) {
			outPath = args.argp('o');
			outPath += "/";
		} else {
			if (CMD_Manager::getContextExists()) {
				if (CMD_Manager::getContext()->getVariable("HIP", outPath)) {
					outPath += "/";
				}
			}
		}
		mpCOP = OPgetDirector()->findCOP2Node(args(1));
		if (!mpCOP) {
			args.err() << "Invalid COP2 path." << endl;
			return;
		}

		UT_String copPath;
		mpCOP->getFullPath(copPath);
		//args.out() << copPath << " : " << copPath.fileName() << " % " << mpCOP->getOperator()->getName() << endl;

		mpOutPath = new UT_String();
		(*mpOutPath) += outPath;

		int i;
		mTexNum = 0;
		mStrMemSize = 0;
		for (i = 0; i < mpCOP->nInputs(); ++i) {
			OP_Node* pTex = mpCOP->getInput(i);
			if (pTex) {
				//args.out() << pTex->getName() << endl;
				mStrMemSize += ::strlen(pTex->getName()) + 1;
				++mTexNum;
			}
		}
		mpInfo = new sTexInfo[mTexNum];
		mpStrMem = new char[mStrMemSize];
		::memset(mpStrMem, 0, mStrMemSize);
		char* pStr = mpStrMem;
		int n = 0;
		for (i = 0; i < mpCOP->nInputs(); ++i) {
			OP_Node* pTex = mpCOP->getInput(i);
			if (pTex) {
				sTexInfo* pInfo = &mpInfo[n];
				const char* pName = pTex->getName();
				size_t nlen = ::strlen(pName);
				::memcpy(pStr, pName, nlen);
				pInfo->mpName = pStr;
				pStr += nlen + 1;
				pInfo->mpNode = pTex->castToCOP2Node();
				pInfo->mFormat = E_TEXFMT_DXT5;
				++n;
			}
		}
	}

	void exec(CMD_Args& args) {
		if (!mpDev || !mpCOP) {
			return;
		}

		int i;
		UT_String path = *mpOutPath;
		path += mpCOP->getName();
		path += ".tpk";
		args.out() << "Saving " << path << "." << endl;
		mBin.open(path);
		mBin.write_u32(MAKEFOURCC('T','P','K',0));
		mBin.write_u32(mTexNum);
		mBin.write_u32(0); // +08 dataSize
		mBin.write_u32(0); // +0C extInfo

		for (i = 0; i < mTexNum; ++i) {
			mBin.write_u8(0);  // +00 fmt
			mBin.write_u8(0);  // +01 lvlNum
			mBin.write_u16(0); // +02 w
			mBin.write_u16(0); // +04 h
			mBin.write_u16(0); // +06 depth
			mBin.write_u32(0); // +08 attr
			mBin.write_u32(0); // +0C offsList
		}
		for (i = 0; i < mTexNum; ++i) {
			cvt_tex(i, args);
		}
		for (i = 0; i < mTexNum; ++i) {
			sTexInfo* pInfo = &mpInfo[i];
			int infoTop = 0x10 + (i*0x10);
			mBin.patch_u8(infoTop + 0, (uint8)pInfo->mFormat);
			mBin.patch_u8(infoTop + 1, (uint8)pInfo->mMipNum);
			mBin.patch_u16(infoTop + 2, pInfo->mWidth);
			mBin.patch_u16(infoTop + 4, pInfo->mHeight);
			mBin.patch_u32(infoTop + 8, pInfo->mAlpha ? 1 : 0);
			mBin.align(16);
			mBin.patch_cur(infoTop + 0xC);
			mBin.write(pInfo->mMipOffs, pInfo->mMipNum*4);
		}

		mBin.align(16);
		mBin.patch_cur(0xC); // extInfo
		int extHeadTop = mBin.get_pos();
		mBin.write_u32(0); // +00 headSize
		mBin.write_u32(0); // +04 nameListOffs
		mBin.patch_u32(extHeadTop, mBin.get_pos() - extHeadTop);
		mBin.align(16);
		mBin.patch_cur(extHeadTop + 4);
		int nameListTop = mBin.get_pos();
		for (i = 0; i < mTexNum; ++i) {
			mBin.write_u32(0);
		}
		for (i = 0; i < mTexNum; ++i) {
			sTexInfo* pInfo = &mpInfo[i];
			mBin.patch_cur(nameListTop + i*4);
			mBin.write_str(pInfo->mpName);
		}

		mBin.patch_cur(8); // dataSize
		mBin.close();
	}
};

static void cmd_cop2tex(CMD_Args& args) {
	cCOP2TEX cop2tex;

	cop2tex.init(args);
	cop2tex.exec(args);
}

void CMDextendLibrary(CMD_Manager* pMgr) {
	pMgr->installCommand("exp_tpk", "dto:", cmd_cop2tex);
}

