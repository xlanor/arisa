//
// Copyright (c) 2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//
// Altered by the arisa project (2026): declarations for the path flattening,
// stroke and fill expansion subset of nanovg in nanovg_path.c.
//
#ifndef NANOVG_PATH_H
#define NANOVG_PATH_H

#ifdef __cplusplus
extern "C" {
#endif

enum NVGwinding {
	NVG_CCW = 1,
	NVG_CW = 2,
};

enum NVGlineCap {
	NVG_BUTT,
	NVG_ROUND,
	NVG_SQUARE,
	NVG_BEVEL,
	NVG_MITER,
};

struct NVGvertex {
	float x,y,u,v;
};
typedef struct NVGvertex NVGvertex;

struct NVGpath {
	int first;
	int count;
	unsigned char closed;
	int nbevel;
	NVGvertex* fill;
	int nfill;
	NVGvertex* stroke;
	int nstroke;
	int winding;
	int convex;
};
typedef struct NVGpath NVGpath;

typedef struct NVGpathContext NVGpathContext;

NVGpathContext* nvgpCreate(float tessTol, float distTol, float fringeWidth);
void nvgpDelete(NVGpathContext* ctx);
void nvgpBeginPath(NVGpathContext* ctx);
void nvgpMoveTo(NVGpathContext* ctx, float x, float y);
void nvgpLineTo(NVGpathContext* ctx, float x, float y);
void nvgpBezierTo(NVGpathContext* ctx, float c1x, float c1y, float c2x, float c2y, float x, float y);
void nvgpClosePath(NVGpathContext* ctx);
void nvgpFlatten(NVGpathContext* ctx, int enforceWinding);
int nvgpPathCount(const NVGpathContext* ctx);
const NVGpath* nvgpPath(const NVGpathContext* ctx, int index);
void nvgpPoint(const NVGpathContext* ctx, int index, float* x, float* y);
int nvgpExpandStroke(NVGpathContext* ctx, float width, int lineCap, int lineJoin, float miterLimit);
int nvgpExpandFill(NVGpathContext* ctx, int antiAlias, int halfFringe);

#ifdef __cplusplus
}
#endif

#endif
