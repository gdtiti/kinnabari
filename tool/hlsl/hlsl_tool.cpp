// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <d3dx9.h>

enum E_PARAMTYPE {
	E_PARAMTYPE_FVEC = 0,
	E_PARAMTYPE_IVEC = 1,
	E_PARAMTYPE_FLOAT = 2,
	E_PARAMTYPE_INT = 3,
	E_PARAMTYPE_SMP = 4,
	E_PARAMTYPE_BOOL = 5
};

class SymTbl {
private:
	char* mpMem;
	char* mpWk;
	int mSize;

public:
	SymTbl(int size) : mSize(size) {
		mpMem = new char[size];
		mpWk = mpMem;
	}

	~SymTbl() {
		delete [] mpMem;
	}

	int Add(char* sym) {
		int offs = Length();
		strcpy_s(mpWk, mSize, sym);
		mpWk += strlen(sym) + 1;
		return offs;
	}

	int Length() {
		return (int)(mpWk - mpMem);
	}

	char* Top() {return mpMem;}
};




void Bin_write(LPCSTR fname, void* pData, int len) {
	FILE* f;
	fopen_s(&f, fname, "w+b");
	if (f) {
		fwrite(pData, 1, len, f);
		fclose(f);
	}
}

struct PARAM_INFO {
/* 0 */	unsigned short name;
/* 2 */	unsigned short reg;
/* 4 */	unsigned short len;
/* 6 */	unsigned short type;
};

static struct PARAM_TBL {
	int nb_param;
	PARAM_INFO info[1024];
} s_tbl;

void main(int argc, char* argv[]) {
	int i, n;
	HRESULT hr;
	LPD3DXBUFFER pCode;
	LPD3DXBUFFER pErr;
	LPD3DXCONSTANTTABLE pTbl;
	char* src_name = "test.hlsl";
	char* cod_name = "test.cod";
	char* prm_name = "test.prm";
	char* profile = "ps_3_0";

	if (argc > 1) {
		src_name = argv[1];
	}
	if (argc > 2) {
		cod_name = argv[2];
	}
	if (argc > 3) {
		prm_name = argv[3];
	}
	if (argc > 4) {
		profile = argv[4];
	}

	hr = D3DXCompileShaderFromFile(src_name, NULL, NULL, "main", profile, D3DXSHADER_PREFER_FLOW_CONTROL, &pCode, &pErr, &pTbl);
	if (D3D_OK == hr) {
		D3DXCONSTANTTABLE_DESC desc;

		pTbl->GetDesc(&desc);
		n = desc.Constants;
		SymTbl* pSym = new SymTbl(2*1024*1024);
		s_tbl.nb_param = 0;

		printf("# const = %d\n", n);
		PARAM_INFO* pInfo = s_tbl.info;
		for (i = 0; i < n; ++i) {
			UINT count = 1;
			D3DXCONSTANT_DESC info[1];
			D3DXHANDLE hConst = pTbl->GetConstant(NULL, i);
			pTbl->GetConstantDesc(hConst, info, &count);
			printf("%s, reg = %d, len = %d\n", info[0].Name, info[0].RegisterIndex, info[0].RegisterCount);

			int sym_id = pSym->Add((char*)info[0].Name);
			pInfo->name = sym_id;
			pInfo->reg = info[0].RegisterIndex;
			pInfo->len = info[0].RegisterCount;
			switch (info[0].Type) {
				case D3DXPT_FLOAT:
					if (info[0].Class == D3DXPC_SCALAR) {
						pInfo->type = E_PARAMTYPE_FLOAT;
					} else {
						pInfo->type = E_PARAMTYPE_FVEC;
					}
					break;
				case D3DXPT_INT:
					if (info[0].Class == D3DXPC_SCALAR) {
						pInfo->type = E_PARAMTYPE_INT;
					} else {
						pInfo->type = E_PARAMTYPE_IVEC;
					}
					break;
				case D3DXPT_BOOL:
					pInfo->type = E_PARAMTYPE_BOOL;
					break;
				case D3DXPT_SAMPLER:
				case D3DXPT_SAMPLER1D:
				case D3DXPT_SAMPLER2D:
				case D3DXPT_SAMPLER3D:
				case D3DXPT_SAMPLERCUBE:
					pInfo->type = E_PARAMTYPE_SMP;
					break;
			}
			++pInfo;
			++s_tbl.nb_param;
		}


		int tbl_size = sizeof(int) + s_tbl.nb_param*sizeof(PARAM_INFO);
		int prm_size = tbl_size + pSym->Length();
		char* pPrm = new char[prm_size];
		memcpy(pPrm, &s_tbl, tbl_size);
		memcpy(pPrm + tbl_size, pSym->Top(), pSym->Length());

		Bin_write(cod_name, pCode->GetBufferPointer(), pCode->GetBufferSize());
		Bin_write(prm_name, pPrm, prm_size);

		delete pSym;

		if (pErr) {
			pErr->Release();
		}
		if (pTbl) {
			pTbl->Release();
		}
	}
}

