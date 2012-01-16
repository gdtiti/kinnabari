// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

typedef row_major float3x4 wmtx_t;

extern float4x4 g_view_proj : register(c0);
extern float4 g_vtx_param; // depth_bias, n_scale, n_bias
extern float3 g_pos_scale;
extern float3 g_pos_base;
extern wmtx_t g_world;
extern wmtx_t g_skin[32];

struct VTX {
	float4 pos : POSITION;
	float4 nrm : NORMAL;
	float4 tng : TANGENT;
	float4 clr : COLOR;
	float4 tex : TEXCOORD0;
	int4   idx : BLENDINDICES0;
	float4 wgt : BLENDWEIGHT0;
};

struct PIX {
	float4 tex;
	float3 wpos;
	float3 wnrm;
	float3 wtng;
	float3 wbnm;
};


float4 Calc_cpos(float3 wpos) {
	float depth_bias = g_vtx_param.x;
	float4 cpos = mul(float4(wpos.xyz, 1), g_view_proj);
	float cw = cpos.w;
	return float4(cpos.x, cpos.y, cpos.z - cw*depth_bias, cw);
}

wmtx_t Get_wmtx(VTX vtx) {
	return g_world;
}

wmtx_t Get_wmtx_skin(VTX vtx) {
	wmtx_t wm0 = g_skin[vtx.idx[0]] * vtx.wgt[0];
	wmtx_t wm1 = g_skin[vtx.idx[1]] * vtx.wgt[1];
	wmtx_t wm2 = g_skin[vtx.idx[2]] * vtx.wgt[2];
	wmtx_t wm3 = g_skin[vtx.idx[3]] * vtx.wgt[3];
	return (wm0 + wm1 + wm2 + wm3);
}

float3 Decode_nvec(float3 vec) {
	float scale = g_vtx_param.y;
	float bias = g_vtx_param.z;
	return vec*scale + bias;
}

float3 Decode_pos(float3 pos, bool rel_flg) {
	if (rel_flg) {
		pos = (pos * g_pos_scale) + g_pos_base;
	}
	return pos;
}

float4 Xform(VTX vtx, wmtx_t wm, out PIX pix, bool rel_flg) {
	pix = (PIX)0;
	float3 wpos = mul(wm, float4(Decode_pos(vtx.pos.xyz, rel_flg), 1));
	float3 wnrm = normalize(mul(wm, float4(Decode_nvec(vtx.nrm.xyz), 0)).xyz);
	pix.wpos = wpos;
	pix.wnrm = wnrm;
	pix.tex = vtx.tex;
	return Calc_cpos(wpos);
}

void Xform_tangent(VTX vtx, wmtx_t wm, float binorm_factor, inout PIX pix) {
	pix.wtng = normalize(mul(wm, float4(Decode_nvec(vtx.tng.xyz), 0)).xyz);
	pix.wbnm = normalize(cross(pix.wtng, pix.wnrm)) * binorm_factor;
}
