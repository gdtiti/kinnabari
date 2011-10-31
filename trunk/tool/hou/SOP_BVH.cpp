// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include <SOP/SOP_Node.h>
#include <UT/UT_VectorTypes.h>
#include <SYS/SYS_Types.h>
#include <UT/UT_Version.h>
#include <PRM/PRM_Include.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Vector3.h>
#include <GU/GU_BVLeafIterator.h>
#include <GU/GU_KDOPTree.h>

class SOP_BVH : public SOP_Node {
public:
	SOP_BVH(OP_Network* pNet, const char* name, OP_Operator* pOp) : SOP_Node(pNet, name, pOp) {}
	virtual ~SOP_BVH() {}

protected:
	virtual const char* inputLabel(unsigned idx) const;
	virtual OP_ERROR cookMySop(OP_Context& ctx);
};

typedef BV_KDOPNode<6> BVH_Node;

class BVH : public GU_AABBTree {
protected:
	UT_HashTable mTbl;
	UT_ValArray<BVH_Node*>* mpList;

	void mk_index(const BVH_Node* pNode) {
		if (!pNode) return;
		uint idx = mpList->entries();
		mpList->append(const_cast<BVH_Node*>(pNode));
		mTbl.addSymbol(UT_Hash_Const_Ptr(pNode), UT_Thing(idx));
		mk_index(pNode->myLeft);
		mk_index(pNode->myRight);
	}

	static GEO_AttributeHandle get_pnt_attr(GEO_Detail* gdp, UT_String name, GB_AttribType type, int size, void* pDef) {
		GEO_AttributeHandle ah = gdp->getPointAttribute(name);
		if (!ah.isAttributeValid()) {
			gdp->addPointAttrib(name, size, type, pDef);
			ah = gdp->getPointAttribute(name);
		}
		return ah;
	}

	static GEO_AttributeHandle get_pnt_attr_i(GEO_Detail* gdp, UT_String name) {
		static int def = 0;
		return get_pnt_attr(gdp, name, GB_ATTRIB_INT, sizeof(int), &def);
	}

	static GEO_AttributeHandle get_pnt_attr_f(GEO_Detail* gdp, UT_String name) {
		static float def = 0.0f;
		return get_pnt_attr(gdp, name, GB_ATTRIB_FLOAT, sizeof(float), &def);
	}

	static GEO_AttributeHandle get_pnt_attr_v(GEO_Detail* gdp, UT_String name) {
		static float def[] = {0.0f, 0.0f, 0.0f};
		return get_pnt_attr(gdp, name, GB_ATTRIB_FLOAT, 3*sizeof(float), def);
	}

	static void set_idx(GEO_Point* pPt, GEO_AttributeHandle hAttr, int idx) {
		hAttr.setElement(pPt);
		hAttr.setI(idx);
	}

public:
	void encode(GEO_Detail* gdp) {
		mpList = new UT_ValArray<BVH_Node*>(getNumLeaves()*2, 0);
		mk_index(getRoot());

		GEO_AttributeHandle hLeft = get_pnt_attr_i(gdp, "left");
		GEO_AttributeHandle hRight = get_pnt_attr_i(gdp, "right");
		GEO_AttributeHandle hPrim = get_pnt_attr_i(gdp, "prim");
		GEO_AttributeHandle hRad = get_pnt_attr_v(gdp, "bboxRad");

		int n = mpList->entries();
		for (int i = 0; i < n; ++i) {
			BVH_Node* pNode = (*mpList)[i];

			GEO_Point* pPt;
			pPt = gdp->insertPoint();
			float x = pNode->myExtents[0] + pNode->myExtents[1];
			float y = pNode->myExtents[2] + pNode->myExtents[3];
			float z = pNode->myExtents[4] + pNode->myExtents[5];
			pPt->setPos(UT_Vector3(x, y, z) * 0.5f);

			if (pNode->isLeaf()) {
				set_idx(pPt, hLeft, -1);
				set_idx(pPt, hRight, -1);
				set_idx(pPt, hPrim, pNode->myLeafId);
			} else {
				set_idx(pPt, hLeft, mTbl[UT_Hash_Const_Ptr(pNode->myLeft)]);
				set_idx(pPt, hRight, mTbl[UT_Hash_Const_Ptr(pNode->myRight)]);
				set_idx(pPt, hPrim, -1);
			}
			hRad.setElement(pPt);
			float dx = pNode->myExtents[1] - pNode->myExtents[0];
			float dy = pNode->myExtents[3] - pNode->myExtents[2];
			float dz = pNode->myExtents[5] - pNode->myExtents[4];
			UT_Vector3 bboxSize(dx, dy, dz);
			UT_Vector3 bboxRad = bboxSize * 0.5f;
			hRad.setF(bboxRad.x(), 0);
			hRad.setF(bboxRad.y(), 1);
			hRad.setF(bboxRad.z(), 2);
		}

		mTbl.clear();
		delete mpList;
	}
};

const char* SOP_BVH::inputLabel(unsigned idx) const {
	return "Input geometry";
}

OP_ERROR SOP_BVH::cookMySop(OP_Context& ctx) {
	if (lockInputs(ctx) >= UT_ERROR_ABORT) return error();
	duplicateSource(0, ctx);

	BVH* pBVH;
	GU_BVLeafIterator it(*gdp);
	pBVH = new BVH();

	pBVH->build(it);
	gdp->clearAndDestroy();
	pBVH->encode(gdp);

	delete pBVH;
	unlockInputs();
	return error();
}

static OP_Node* ctor(OP_Network* pNet, const char* name, OP_Operator* pOp) {
	return new SOP_BVH(pNet, name, pOp);
}

void newSopOperator(OP_OperatorTable* pTbl) {
	static PRM_Template tmpl[] = {PRM_Template()};
	pTbl->addOperator(new OP_Operator("bvh", "BVH", ctor, tmpl, 1, 1));
}

