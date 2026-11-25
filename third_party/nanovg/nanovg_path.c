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
// Altered by the arisa project (2026): this file holds only the path
// flattening, stroke expansion, join/cap and fill expansion code taken from
// nanovg.c (commit ce3bf745eb2d2dbc14a50bf2446783f691ac4353). The NVGcontext
// was reduced to NVGpathContext, the command transform was removed, winding
// enforcement became optional, fill fringes can be forced to the half fringe
// used for convex shapes, and an nvgp* entry point API was added.
//
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "nanovg_path.h"

#define NVG_INIT_COMMANDS_SIZE 256
#define NVG_INIT_POINTS_SIZE 128
#define NVG_INIT_PATHS_SIZE 16
#define NVG_INIT_VERTS_SIZE 256
#define NVG_PI 3.14159265358979323846264338327f
#define NVG_NOTUSED(v) for (;;) { (void)(1 ? (void)0 : ( (void)(v) ) ); break; }

enum NVGcommands {
	NVG_MOVETO = 0,
	NVG_LINETO = 1,
	NVG_BEZIERTO = 2,
	NVG_CLOSE = 3,
	NVG_WINDING = 4,
};

enum NVGpointFlags
{
	NVG_PT_CORNER = 0x01,
	NVG_PT_LEFT = 0x02,
	NVG_PT_BEVEL = 0x04,
	NVG_PR_INNERBEVEL = 0x08,
};
struct NVGpoint {
	float x,y;
	float dx, dy;
	float len;
	float dmx, dmy;
	unsigned char flags;
};
typedef struct NVGpoint NVGpoint;

struct NVGpathCache {
	NVGpoint* points;
	int npoints;
	int cpoints;
	NVGpath* paths;
	int npaths;
	int cpaths;
	NVGvertex* verts;
	int nverts;
	int cverts;
	float bounds[4];
};
typedef struct NVGpathCache NVGpathCache;
struct NVGpathContext {
	float* commands;
	int ccommands;
	int ncommands;
	NVGpathCache* cache;
	float tessTol;
	float distTol;
	float fringeWidth;
};
typedef struct NVGpathContext NVGcontext;

static float nvg__sqrtf(float a) { return sqrtf(a); }
static float nvg__modf(float a, float b) { return fmodf(a, b); }
static float nvg__sinf(float a) { return sinf(a); }
static float nvg__cosf(float a) { return cosf(a); }
static float nvg__tanf(float a) { return tanf(a); }
static float nvg__atan2f(float a,float b) { return atan2f(a, b); }
static float nvg__acosf(float a) { return acosf(a); }

static int nvg__mini(int a, int b) { return a < b ? a : b; }
static int nvg__maxi(int a, int b) { return a > b ? a : b; }
static int nvg__clampi(int a, int mn, int mx) { return a < mn ? mn : (a > mx ? mx : a); }
static float nvg__minf(float a, float b) { return a < b ? a : b; }
static float nvg__maxf(float a, float b) { return a > b ? a : b; }
static float nvg__absf(float a) { return a >= 0.0f ? a : -a; }
static float nvg__signf(float a) { return a >= 0.0f ? 1.0f : -1.0f; }
static float nvg__clampf(float a, float mn, float mx) { return a < mn ? mn : (a > mx ? mx : a); }
static float nvg__cross(float dx0, float dy0, float dx1, float dy1) { return dx1*dy0 - dx0*dy1; }

static float nvg__normalize(float *x, float* y)
{
	float d = nvg__sqrtf((*x)*(*x) + (*y)*(*y));
	if (d > 1e-6f) {
		float id = 1.0f / d;
		*x *= id;
		*y *= id;
	}
	return d;
}

static void nvg__deletePathCache(NVGpathCache* c)
{
	if (c == NULL) return;
	if (c->points != NULL) free(c->points);
	if (c->paths != NULL) free(c->paths);
	if (c->verts != NULL) free(c->verts);
	free(c);
}

static NVGpathCache* nvg__allocPathCache(void)
{
	NVGpathCache* c = (NVGpathCache*)malloc(sizeof(NVGpathCache));
	if (c == NULL) goto error;
	memset(c, 0, sizeof(NVGpathCache));

	c->points = (NVGpoint*)malloc(sizeof(NVGpoint)*NVG_INIT_POINTS_SIZE);
	if (!c->points) goto error;
	c->npoints = 0;
	c->cpoints = NVG_INIT_POINTS_SIZE;

	c->paths = (NVGpath*)malloc(sizeof(NVGpath)*NVG_INIT_PATHS_SIZE);
	if (!c->paths) goto error;
	c->npaths = 0;
	c->cpaths = NVG_INIT_PATHS_SIZE;

	c->verts = (NVGvertex*)malloc(sizeof(NVGvertex)*NVG_INIT_VERTS_SIZE);
	if (!c->verts) goto error;
	c->nverts = 0;
	c->cverts = NVG_INIT_VERTS_SIZE;

	return c;
error:
	nvg__deletePathCache(c);
	return NULL;
}
static int nvg__ptEquals(float x1, float y1, float x2, float y2, float tol)
{
	float dx = x2 - x1;
	float dy = y2 - y1;
	return dx*dx + dy*dy < tol*tol;
}
static void nvg__appendCommands(NVGcontext* ctx, float* vals, int nvals)
{
	if (ctx->ncommands+nvals > ctx->ccommands) {
		float* commands;
		int ccommands = ctx->ncommands+nvals + ctx->ccommands/2;
		commands = (float*)realloc(ctx->commands, sizeof(float)*ccommands);
		if (commands == NULL) return;
		ctx->commands = commands;
		ctx->ccommands = ccommands;
	}

	memcpy(&ctx->commands[ctx->ncommands], vals, nvals*sizeof(float));

	ctx->ncommands += nvals;
}

static void nvg__clearPathCache(NVGcontext* ctx)
{
	ctx->cache->npoints = 0;
	ctx->cache->npaths = 0;
}

static NVGpath* nvg__lastPath(NVGcontext* ctx)
{
	if (ctx->cache->npaths > 0)
		return &ctx->cache->paths[ctx->cache->npaths-1];
	return NULL;
}

static void nvg__addPath(NVGcontext* ctx)
{
	NVGpath* path;
	if (ctx->cache->npaths+1 > ctx->cache->cpaths) {
		NVGpath* paths;
		int cpaths = ctx->cache->npaths+1 + ctx->cache->cpaths/2;
		paths = (NVGpath*)realloc(ctx->cache->paths, sizeof(NVGpath)*cpaths);
		if (paths == NULL) return;
		ctx->cache->paths = paths;
		ctx->cache->cpaths = cpaths;
	}
	path = &ctx->cache->paths[ctx->cache->npaths];
	memset(path, 0, sizeof(*path));
	path->first = ctx->cache->npoints;
	path->winding = NVG_CCW;

	ctx->cache->npaths++;
}

static NVGpoint* nvg__lastPoint(NVGcontext* ctx)
{
	if (ctx->cache->npoints > 0)
		return &ctx->cache->points[ctx->cache->npoints-1];
	return NULL;
}

static void nvg__addPoint(NVGcontext* ctx, float x, float y, int flags)
{
	NVGpath* path = nvg__lastPath(ctx);
	NVGpoint* pt;
	if (path == NULL) return;

	if (path->count > 0 && ctx->cache->npoints > 0) {
		pt = nvg__lastPoint(ctx);
		if (nvg__ptEquals(pt->x,pt->y, x,y, ctx->distTol)) {
			pt->flags |= flags;
			return;
		}
	}

	if (ctx->cache->npoints+1 > ctx->cache->cpoints) {
		NVGpoint* points;
		int cpoints = ctx->cache->npoints+1 + ctx->cache->cpoints/2;
		points = (NVGpoint*)realloc(ctx->cache->points, sizeof(NVGpoint)*cpoints);
		if (points == NULL) return;
		ctx->cache->points = points;
		ctx->cache->cpoints = cpoints;
	}

	pt = &ctx->cache->points[ctx->cache->npoints];
	memset(pt, 0, sizeof(*pt));
	pt->x = x;
	pt->y = y;
	pt->flags = (unsigned char)flags;

	ctx->cache->npoints++;
	path->count++;
}

static void nvg__closePath(NVGcontext* ctx)
{
	NVGpath* path = nvg__lastPath(ctx);
	if (path == NULL) return;
	path->closed = 1;
}

static void nvg__pathWinding(NVGcontext* ctx, int winding)
{
	NVGpath* path = nvg__lastPath(ctx);
	if (path == NULL) return;
	path->winding = winding;
}
static NVGvertex* nvg__allocTempVerts(NVGcontext* ctx, int nverts)
{
	if (nverts > ctx->cache->cverts) {
		NVGvertex* verts;
		int cverts = (nverts + 0xff) & ~0xff; // Round up to prevent allocations when things change just slightly.
		verts = (NVGvertex*)realloc(ctx->cache->verts, sizeof(NVGvertex)*cverts);
		if (verts == NULL) return NULL;
		ctx->cache->verts = verts;
		ctx->cache->cverts = cverts;
	}

	return ctx->cache->verts;
}

static float nvg__triarea2(float ax, float ay, float bx, float by, float cx, float cy)
{
	float abx = bx - ax;
	float aby = by - ay;
	float acx = cx - ax;
	float acy = cy - ay;
	return acx*aby - abx*acy;
}

static float nvg__polyArea(NVGpoint* pts, int npts)
{
	int i;
	float area = 0;
	for (i = 2; i < npts; i++) {
		NVGpoint* a = &pts[0];
		NVGpoint* b = &pts[i-1];
		NVGpoint* c = &pts[i];
		area += nvg__triarea2(a->x,a->y, b->x,b->y, c->x,c->y);
	}
	return area * 0.5f;
}

static void nvg__polyReverse(NVGpoint* pts, int npts)
{
	NVGpoint tmp;
	int i = 0, j = npts-1;
	while (i < j) {
		tmp = pts[i];
		pts[i] = pts[j];
		pts[j] = tmp;
		i++;
		j--;
	}
}


static void nvg__vset(NVGvertex* vtx, float x, float y, float u, float v)
{
	vtx->x = x;
	vtx->y = y;
	vtx->u = u;
	vtx->v = v;
}

static void nvg__tesselateBezier(NVGcontext* ctx,
								 float x1, float y1, float x2, float y2,
								 float x3, float y3, float x4, float y4,
								 int level, int type)
{
	float x12,y12,x23,y23,x34,y34,x123,y123,x234,y234,x1234,y1234;
	float dx,dy,d2,d3;

	if (level > 10) return;

	x12 = (x1+x2)*0.5f;
	y12 = (y1+y2)*0.5f;
	x23 = (x2+x3)*0.5f;
	y23 = (y2+y3)*0.5f;
	x34 = (x3+x4)*0.5f;
	y34 = (y3+y4)*0.5f;
	x123 = (x12+x23)*0.5f;
	y123 = (y12+y23)*0.5f;

	dx = x4 - x1;
	dy = y4 - y1;
	d2 = nvg__absf(((x2 - x4) * dy - (y2 - y4) * dx));
	d3 = nvg__absf(((x3 - x4) * dy - (y3 - y4) * dx));

	if ((d2 + d3)*(d2 + d3) < ctx->tessTol * (dx*dx + dy*dy)) {
		nvg__addPoint(ctx, x4, y4, type);
		return;
	}

/*	if (nvg__absf(x1+x3-x2-x2) + nvg__absf(y1+y3-y2-y2) + nvg__absf(x2+x4-x3-x3) + nvg__absf(y2+y4-y3-y3) < ctx->tessTol) {
		nvg__addPoint(ctx, x4, y4, type);
		return;
	}*/

	x234 = (x23+x34)*0.5f;
	y234 = (y23+y34)*0.5f;
	x1234 = (x123+x234)*0.5f;
	y1234 = (y123+y234)*0.5f;

	nvg__tesselateBezier(ctx, x1,y1, x12,y12, x123,y123, x1234,y1234, level+1, 0);
	nvg__tesselateBezier(ctx, x1234,y1234, x234,y234, x34,y34, x4,y4, level+1, type);
}

static void nvg__flattenPaths(NVGcontext* ctx, int enforceWinding)
{
	NVGpathCache* cache = ctx->cache;
//	NVGstate* state = nvg__getState(ctx);
	NVGpoint* last;
	NVGpoint* p0;
	NVGpoint* p1;
	NVGpoint* pts;
	NVGpath* path;
	int i, j;
	float* cp1;
	float* cp2;
	float* p;
	float area;

	if (cache->npaths > 0)
		return;

	// Flatten
	i = 0;
	while (i < ctx->ncommands) {
		int cmd = (int)ctx->commands[i];
		switch (cmd) {
		case NVG_MOVETO:
			nvg__addPath(ctx);
			p = &ctx->commands[i+1];
			nvg__addPoint(ctx, p[0], p[1], NVG_PT_CORNER);
			i += 3;
			break;
		case NVG_LINETO:
			p = &ctx->commands[i+1];
			nvg__addPoint(ctx, p[0], p[1], NVG_PT_CORNER);
			i += 3;
			break;
		case NVG_BEZIERTO:
			last = nvg__lastPoint(ctx);
			if (last != NULL) {
				cp1 = &ctx->commands[i+1];
				cp2 = &ctx->commands[i+3];
				p = &ctx->commands[i+5];
				nvg__tesselateBezier(ctx, last->x,last->y, cp1[0],cp1[1], cp2[0],cp2[1], p[0],p[1], 0, NVG_PT_CORNER);
			}
			i += 7;
			break;
		case NVG_CLOSE:
			nvg__closePath(ctx);
			i++;
			break;
		case NVG_WINDING:
			nvg__pathWinding(ctx, (int)ctx->commands[i+1]);
			i += 2;
			break;
		default:
			i++;
		}
	}

	cache->bounds[0] = cache->bounds[1] = 1e6f;
	cache->bounds[2] = cache->bounds[3] = -1e6f;

	// Calculate the direction and length of line segments.
	for (j = 0; j < cache->npaths; j++) {
		path = &cache->paths[j];
		pts = &cache->points[path->first];

		// If the first and last points are the same, remove the last, mark as closed path.
		p0 = &pts[path->count-1];
		p1 = &pts[0];
		if (nvg__ptEquals(p0->x,p0->y, p1->x,p1->y, ctx->distTol)) {
			path->count--;
			p0 = &pts[path->count-1];
			path->closed = 1;
		}

		// Enforce winding.
		if (enforceWinding && path->count > 2) {
			area = nvg__polyArea(pts, path->count);
			if (path->winding == NVG_CCW && area < 0.0f)
				nvg__polyReverse(pts, path->count);
			if (path->winding == NVG_CW && area > 0.0f)
				nvg__polyReverse(pts, path->count);
		}

		for(i = 0; i < path->count; i++) {
			// Calculate segment direction and length
			p0->dx = p1->x - p0->x;
			p0->dy = p1->y - p0->y;
			p0->len = nvg__normalize(&p0->dx, &p0->dy);
			// Update bounds
			cache->bounds[0] = nvg__minf(cache->bounds[0], p0->x);
			cache->bounds[1] = nvg__minf(cache->bounds[1], p0->y);
			cache->bounds[2] = nvg__maxf(cache->bounds[2], p0->x);
			cache->bounds[3] = nvg__maxf(cache->bounds[3], p0->y);
			// Advance
			p0 = p1++;
		}
	}
}
static int nvg__curveDivs(float r, float arc, float tol)
{
	float da = acosf(r / (r + tol)) * 2.0f;
	return nvg__maxi(2, (int)ceilf(arc / da));
}

static void nvg__chooseBevel(int bevel, NVGpoint* p0, NVGpoint* p1, float w,
							float* x0, float* y0, float* x1, float* y1)
{
	if (bevel) {
		*x0 = p1->x + p0->dy * w;
		*y0 = p1->y - p0->dx * w;
		*x1 = p1->x + p1->dy * w;
		*y1 = p1->y - p1->dx * w;
	} else {
		*x0 = p1->x + p1->dmx * w;
		*y0 = p1->y + p1->dmy * w;
		*x1 = p1->x + p1->dmx * w;
		*y1 = p1->y + p1->dmy * w;
	}
}

static NVGvertex* nvg__roundJoin(NVGvertex* dst, NVGpoint* p0, NVGpoint* p1,
								 float lw, float rw, float lu, float ru, int ncap,
								 float fringe)
{
	int i, n;
	float dlx0 = p0->dy;
	float dly0 = -p0->dx;
	float dlx1 = p1->dy;
	float dly1 = -p1->dx;
	NVG_NOTUSED(fringe);

	if (p1->flags & NVG_PT_LEFT) {
		float lx0,ly0,lx1,ly1,a0,a1;
		nvg__chooseBevel(p1->flags & NVG_PR_INNERBEVEL, p0, p1, lw, &lx0,&ly0, &lx1,&ly1);
		a0 = atan2f(-dly0, -dlx0);
		a1 = atan2f(-dly1, -dlx1);
		if (a1 > a0) a1 -= NVG_PI*2;

		nvg__vset(dst, lx0, ly0, lu,1); dst++;
		nvg__vset(dst, p1->x - dlx0*rw, p1->y - dly0*rw, ru,1); dst++;

		n = nvg__clampi((int)ceilf(((a0 - a1) / NVG_PI) * ncap), 2, ncap);
		for (i = 0; i < n; i++) {
			float u = i/(float)(n-1);
			float a = a0 + u*(a1-a0);
			float rx = p1->x + cosf(a) * rw;
			float ry = p1->y + sinf(a) * rw;
			nvg__vset(dst, p1->x, p1->y, 0.5f,1); dst++;
			nvg__vset(dst, rx, ry, ru,1); dst++;
		}

		nvg__vset(dst, lx1, ly1, lu,1); dst++;
		nvg__vset(dst, p1->x - dlx1*rw, p1->y - dly1*rw, ru,1); dst++;

	} else {
		float rx0,ry0,rx1,ry1,a0,a1;
		nvg__chooseBevel(p1->flags & NVG_PR_INNERBEVEL, p0, p1, -rw, &rx0,&ry0, &rx1,&ry1);
		a0 = atan2f(dly0, dlx0);
		a1 = atan2f(dly1, dlx1);
		if (a1 < a0) a1 += NVG_PI*2;

		nvg__vset(dst, p1->x + dlx0*rw, p1->y + dly0*rw, lu,1); dst++;
		nvg__vset(dst, rx0, ry0, ru,1); dst++;

		n = nvg__clampi((int)ceilf(((a1 - a0) / NVG_PI) * ncap), 2, ncap);
		for (i = 0; i < n; i++) {
			float u = i/(float)(n-1);
			float a = a0 + u*(a1-a0);
			float lx = p1->x + cosf(a) * lw;
			float ly = p1->y + sinf(a) * lw;
			nvg__vset(dst, lx, ly, lu,1); dst++;
			nvg__vset(dst, p1->x, p1->y, 0.5f,1); dst++;
		}

		nvg__vset(dst, p1->x + dlx1*rw, p1->y + dly1*rw, lu,1); dst++;
		nvg__vset(dst, rx1, ry1, ru,1); dst++;

	}
	return dst;
}

static NVGvertex* nvg__bevelJoin(NVGvertex* dst, NVGpoint* p0, NVGpoint* p1,
										float lw, float rw, float lu, float ru, float fringe)
{
	float rx0,ry0,rx1,ry1;
	float lx0,ly0,lx1,ly1;
	float dlx0 = p0->dy;
	float dly0 = -p0->dx;
	float dlx1 = p1->dy;
	float dly1 = -p1->dx;
	NVG_NOTUSED(fringe);

	if (p1->flags & NVG_PT_LEFT) {
		nvg__chooseBevel(p1->flags & NVG_PR_INNERBEVEL, p0, p1, lw, &lx0,&ly0, &lx1,&ly1);

		nvg__vset(dst, lx0, ly0, lu,1); dst++;
		nvg__vset(dst, p1->x - dlx0*rw, p1->y - dly0*rw, ru,1); dst++;

		if (p1->flags & NVG_PT_BEVEL) {
			nvg__vset(dst, lx0, ly0, lu,1); dst++;
			nvg__vset(dst, p1->x - dlx0*rw, p1->y - dly0*rw, ru,1); dst++;

			nvg__vset(dst, lx1, ly1, lu,1); dst++;
			nvg__vset(dst, p1->x - dlx1*rw, p1->y - dly1*rw, ru,1); dst++;
		} else {
			rx0 = p1->x - p1->dmx * rw;
			ry0 = p1->y - p1->dmy * rw;

			nvg__vset(dst, p1->x, p1->y, 0.5f,1); dst++;
			nvg__vset(dst, p1->x - dlx0*rw, p1->y - dly0*rw, ru,1); dst++;

			nvg__vset(dst, rx0, ry0, ru,1); dst++;
			nvg__vset(dst, rx0, ry0, ru,1); dst++;

			nvg__vset(dst, p1->x, p1->y, 0.5f,1); dst++;
			nvg__vset(dst, p1->x - dlx1*rw, p1->y - dly1*rw, ru,1); dst++;
		}

		nvg__vset(dst, lx1, ly1, lu,1); dst++;
		nvg__vset(dst, p1->x - dlx1*rw, p1->y - dly1*rw, ru,1); dst++;

	} else {
		nvg__chooseBevel(p1->flags & NVG_PR_INNERBEVEL, p0, p1, -rw, &rx0,&ry0, &rx1,&ry1);

		nvg__vset(dst, p1->x + dlx0*lw, p1->y + dly0*lw, lu,1); dst++;
		nvg__vset(dst, rx0, ry0, ru,1); dst++;

		if (p1->flags & NVG_PT_BEVEL) {
			nvg__vset(dst, p1->x + dlx0*lw, p1->y + dly0*lw, lu,1); dst++;
			nvg__vset(dst, rx0, ry0, ru,1); dst++;

			nvg__vset(dst, p1->x + dlx1*lw, p1->y + dly1*lw, lu,1); dst++;
			nvg__vset(dst, rx1, ry1, ru,1); dst++;
		} else {
			lx0 = p1->x + p1->dmx * lw;
			ly0 = p1->y + p1->dmy * lw;

			nvg__vset(dst, p1->x + dlx0*lw, p1->y + dly0*lw, lu,1); dst++;
			nvg__vset(dst, p1->x, p1->y, 0.5f,1); dst++;

			nvg__vset(dst, lx0, ly0, lu,1); dst++;
			nvg__vset(dst, lx0, ly0, lu,1); dst++;

			nvg__vset(dst, p1->x + dlx1*lw, p1->y + dly1*lw, lu,1); dst++;
			nvg__vset(dst, p1->x, p1->y, 0.5f,1); dst++;
		}

		nvg__vset(dst, p1->x + dlx1*lw, p1->y + dly1*lw, lu,1); dst++;
		nvg__vset(dst, rx1, ry1, ru,1); dst++;
	}

	return dst;
}

static NVGvertex* nvg__buttCapStart(NVGvertex* dst, NVGpoint* p,
									float dx, float dy, float w, float d,
									float aa, float u0, float u1)
{
	float px = p->x - dx*d;
	float py = p->y - dy*d;
	float dlx = dy;
	float dly = -dx;
	nvg__vset(dst, px + dlx*w - dx*aa, py + dly*w - dy*aa, u0,0); dst++;
	nvg__vset(dst, px - dlx*w - dx*aa, py - dly*w - dy*aa, u1,0); dst++;
	nvg__vset(dst, px + dlx*w, py + dly*w, u0,1); dst++;
	nvg__vset(dst, px - dlx*w, py - dly*w, u1,1); dst++;
	return dst;
}

static NVGvertex* nvg__buttCapEnd(NVGvertex* dst, NVGpoint* p,
								  float dx, float dy, float w, float d,
								  float aa, float u0, float u1)
{
	float px = p->x + dx*d;
	float py = p->y + dy*d;
	float dlx = dy;
	float dly = -dx;
	nvg__vset(dst, px + dlx*w, py + dly*w, u0,1); dst++;
	nvg__vset(dst, px - dlx*w, py - dly*w, u1,1); dst++;
	nvg__vset(dst, px + dlx*w + dx*aa, py + dly*w + dy*aa, u0,0); dst++;
	nvg__vset(dst, px - dlx*w + dx*aa, py - dly*w + dy*aa, u1,0); dst++;
	return dst;
}


static NVGvertex* nvg__roundCapStart(NVGvertex* dst, NVGpoint* p,
									 float dx, float dy, float w, int ncap,
									 float aa, float u0, float u1)
{
	int i;
	float px = p->x;
	float py = p->y;
	float dlx = dy;
	float dly = -dx;
	NVG_NOTUSED(aa);
	for (i = 0; i < ncap; i++) {
		float a = i/(float)(ncap-1)*NVG_PI;
		float ax = cosf(a) * w, ay = sinf(a) * w;
		nvg__vset(dst, px - dlx*ax - dx*ay, py - dly*ax - dy*ay, u0,1); dst++;
		nvg__vset(dst, px, py, 0.5f,1); dst++;
	}
	nvg__vset(dst, px + dlx*w, py + dly*w, u0,1); dst++;
	nvg__vset(dst, px - dlx*w, py - dly*w, u1,1); dst++;
	return dst;
}

static NVGvertex* nvg__roundCapEnd(NVGvertex* dst, NVGpoint* p,
								   float dx, float dy, float w, int ncap,
								   float aa, float u0, float u1)
{
	int i;
	float px = p->x;
	float py = p->y;
	float dlx = dy;
	float dly = -dx;
	NVG_NOTUSED(aa);
	nvg__vset(dst, px + dlx*w, py + dly*w, u0,1); dst++;
	nvg__vset(dst, px - dlx*w, py - dly*w, u1,1); dst++;
	for (i = 0; i < ncap; i++) {
		float a = i/(float)(ncap-1)*NVG_PI;
		float ax = cosf(a) * w, ay = sinf(a) * w;
		nvg__vset(dst, px, py, 0.5f,1); dst++;
		nvg__vset(dst, px - dlx*ax + dx*ay, py - dly*ax + dy*ay, u0,1); dst++;
	}
	return dst;
}


static void nvg__calculateJoins(NVGcontext* ctx, float w, int lineJoin, float miterLimit)
{
	NVGpathCache* cache = ctx->cache;
	int i, j;
	float iw = 0.0f;

	if (w > 0.0f) iw = 1.0f / w;

	// Calculate which joins needs extra vertices to append, and gather vertex count.
	for (i = 0; i < cache->npaths; i++) {
		NVGpath* path = &cache->paths[i];
		NVGpoint* pts = &cache->points[path->first];
		NVGpoint* p0 = &pts[path->count-1];
		NVGpoint* p1 = &pts[0];
		int nleft = 0;

		path->nbevel = 0;

		for (j = 0; j < path->count; j++) {
			float dlx0, dly0, dlx1, dly1, dmr2, cross, limit;
			dlx0 = p0->dy;
			dly0 = -p0->dx;
			dlx1 = p1->dy;
			dly1 = -p1->dx;
			// Calculate extrusions
			p1->dmx = (dlx0 + dlx1) * 0.5f;
			p1->dmy = (dly0 + dly1) * 0.5f;
			dmr2 = p1->dmx*p1->dmx + p1->dmy*p1->dmy;
			if (dmr2 > 0.000001f) {
				float scale = 1.0f / dmr2;
				if (scale > 600.0f) {
					scale = 600.0f;
				}
				p1->dmx *= scale;
				p1->dmy *= scale;
			}

			// Clear flags, but keep the corner.
			p1->flags = (p1->flags & NVG_PT_CORNER) ? NVG_PT_CORNER : 0;

			// Keep track of left turns.
			cross = p1->dx * p0->dy - p0->dx * p1->dy;
			if (cross > 0.0f) {
				nleft++;
				p1->flags |= NVG_PT_LEFT;
			}

			// Calculate if we should use bevel or miter for inner join.
			limit = nvg__maxf(1.01f, nvg__minf(p0->len, p1->len) * iw);
			if ((dmr2 * limit*limit) < 1.0f)
				p1->flags |= NVG_PR_INNERBEVEL;

			// Check to see if the corner needs to be beveled.
			if (p1->flags & NVG_PT_CORNER) {
				if ((dmr2 * miterLimit*miterLimit) < 1.0f || lineJoin == NVG_BEVEL || lineJoin == NVG_ROUND) {
					p1->flags |= NVG_PT_BEVEL;
				}
			}

			if ((p1->flags & (NVG_PT_BEVEL | NVG_PR_INNERBEVEL)) != 0)
				path->nbevel++;

			p0 = p1++;
		}

		path->convex = (nleft == path->count) ? 1 : 0;
	}
}


static int nvg__expandStroke(NVGcontext* ctx, float w, float fringe, int lineCap, int lineJoin, float miterLimit)
{
	NVGpathCache* cache = ctx->cache;
	NVGvertex* verts;
	NVGvertex* dst;
	int cverts, i, j;
	float aa = fringe;//ctx->fringeWidth;
	float u0 = 0.0f, u1 = 1.0f;
	int ncap = nvg__curveDivs(w, NVG_PI, ctx->tessTol);	// Calculate divisions per half circle.

	w += aa * 0.5f;

	// Disable the gradient used for antialiasing when antialiasing is not used.
	if (aa == 0.0f) {
		u0 = 0.5f;
		u1 = 0.5f;
	}

	nvg__calculateJoins(ctx, w, lineJoin, miterLimit);

	// Calculate max vertex usage.
	cverts = 0;
	for (i = 0; i < cache->npaths; i++) {
		NVGpath* path = &cache->paths[i];
		int loop = (path->closed == 0) ? 0 : 1;
		if (lineJoin == NVG_ROUND)
			cverts += (path->count + path->nbevel*(ncap+2) + 1) * 2; // plus one for loop
		else
			cverts += (path->count + path->nbevel*5 + 1) * 2; // plus one for loop
		if (loop == 0) {
			// space for caps
			if (lineCap == NVG_ROUND) {
				cverts += (ncap*2 + 2)*2;
			} else {
				cverts += (3+3)*2;
			}
		}
	}

	verts = nvg__allocTempVerts(ctx, cverts);
	if (verts == NULL) return 0;

	for (i = 0; i < cache->npaths; i++) {
		NVGpath* path = &cache->paths[i];
		NVGpoint* pts = &cache->points[path->first];
		NVGpoint* p0;
		NVGpoint* p1;
		int s, e, loop;
		float dx, dy;

		path->fill = 0;
		path->nfill = 0;

		// Calculate fringe or stroke
		loop = (path->closed == 0) ? 0 : 1;
		dst = verts;
		path->stroke = dst;

		if (loop) {
			// Looping
			p0 = &pts[path->count-1];
			p1 = &pts[0];
			s = 0;
			e = path->count;
		} else {
			// Add cap
			p0 = &pts[0];
			p1 = &pts[1];
			s = 1;
			e = path->count-1;
		}

		if (loop == 0) {
			// Add cap
			dx = p1->x - p0->x;
			dy = p1->y - p0->y;
			nvg__normalize(&dx, &dy);
			if (lineCap == NVG_BUTT)
				dst = nvg__buttCapStart(dst, p0, dx, dy, w, -aa*0.5f, aa, u0, u1);
			else if (lineCap == NVG_BUTT || lineCap == NVG_SQUARE)
				dst = nvg__buttCapStart(dst, p0, dx, dy, w, w-aa, aa, u0, u1);
			else if (lineCap == NVG_ROUND)
				dst = nvg__roundCapStart(dst, p0, dx, dy, w, ncap, aa, u0, u1);
		}

		for (j = s; j < e; ++j) {
			if ((p1->flags & (NVG_PT_BEVEL | NVG_PR_INNERBEVEL)) != 0) {
				if (lineJoin == NVG_ROUND) {
					dst = nvg__roundJoin(dst, p0, p1, w, w, u0, u1, ncap, aa);
				} else {
					dst = nvg__bevelJoin(dst, p0, p1, w, w, u0, u1, aa);
				}
			} else {
				nvg__vset(dst, p1->x + (p1->dmx * w), p1->y + (p1->dmy * w), u0,1); dst++;
				nvg__vset(dst, p1->x - (p1->dmx * w), p1->y - (p1->dmy * w), u1,1); dst++;
			}
			p0 = p1++;
		}

		if (loop) {
			// Loop it
			nvg__vset(dst, verts[0].x, verts[0].y, u0,1); dst++;
			nvg__vset(dst, verts[1].x, verts[1].y, u1,1); dst++;
		} else {
			// Add cap
			dx = p1->x - p0->x;
			dy = p1->y - p0->y;
			nvg__normalize(&dx, &dy);
			if (lineCap == NVG_BUTT)
				dst = nvg__buttCapEnd(dst, p1, dx, dy, w, -aa*0.5f, aa, u0, u1);
			else if (lineCap == NVG_BUTT || lineCap == NVG_SQUARE)
				dst = nvg__buttCapEnd(dst, p1, dx, dy, w, w-aa, aa, u0, u1);
			else if (lineCap == NVG_ROUND)
				dst = nvg__roundCapEnd(dst, p1, dx, dy, w, ncap, aa, u0, u1);
		}

		path->nstroke = (int)(dst - verts);

		verts = dst;
	}

	return 1;
}

static int nvg__expandFill(NVGcontext* ctx, float w, int lineJoin, float miterLimit, int halfFringe)
{
	NVGpathCache* cache = ctx->cache;
	NVGvertex* verts;
	NVGvertex* dst;
	int cverts, convex, i, j;
	float aa = ctx->fringeWidth;
	int fringe = w > 0.0f;

	nvg__calculateJoins(ctx, w, lineJoin, miterLimit);

	// Calculate max vertex usage.
	cverts = 0;
	for (i = 0; i < cache->npaths; i++) {
		NVGpath* path = &cache->paths[i];
		cverts += path->count + path->nbevel + 1;
		if (fringe)
			cverts += (path->count + path->nbevel*5 + 1) * 2; // plus one for loop
	}

	verts = nvg__allocTempVerts(ctx, cverts);
	if (verts == NULL) return 0;

	convex = halfFringe || (cache->npaths == 1 && cache->paths[0].convex);

	for (i = 0; i < cache->npaths; i++) {
		NVGpath* path = &cache->paths[i];
		NVGpoint* pts = &cache->points[path->first];
		NVGpoint* p0;
		NVGpoint* p1;
		float rw, lw, woff;
		float ru, lu;

		// Calculate shape vertices.
		woff = 0.5f*aa;
		dst = verts;
		path->fill = dst;

		if (fringe) {
			// Looping
			p0 = &pts[path->count-1];
			p1 = &pts[0];
			for (j = 0; j < path->count; ++j) {
				if (p1->flags & NVG_PT_BEVEL) {
					float dlx0 = p0->dy;
					float dly0 = -p0->dx;
					float dlx1 = p1->dy;
					float dly1 = -p1->dx;
					if (p1->flags & NVG_PT_LEFT) {
						float lx = p1->x + p1->dmx * woff;
						float ly = p1->y + p1->dmy * woff;
						nvg__vset(dst, lx, ly, 0.5f,1); dst++;
					} else {
						float lx0 = p1->x + dlx0 * woff;
						float ly0 = p1->y + dly0 * woff;
						float lx1 = p1->x + dlx1 * woff;
						float ly1 = p1->y + dly1 * woff;
						nvg__vset(dst, lx0, ly0, 0.5f,1); dst++;
						nvg__vset(dst, lx1, ly1, 0.5f,1); dst++;
					}
				} else {
					nvg__vset(dst, p1->x + (p1->dmx * woff), p1->y + (p1->dmy * woff), 0.5f,1); dst++;
				}
				p0 = p1++;
			}
		} else {
			for (j = 0; j < path->count; ++j) {
				nvg__vset(dst, pts[j].x, pts[j].y, 0.5f,1);
				dst++;
			}
		}

		path->nfill = (int)(dst - verts);
		verts = dst;

		// Calculate fringe
		if (fringe) {
			lw = w + woff;
			rw = w - woff;
			lu = 0;
			ru = 1;
			dst = verts;
			path->stroke = dst;

			// Create only half a fringe for convex shapes so that
			// the shape can be rendered without stenciling.
			if (convex) {
				lw = woff;	// This should generate the same vertex as fill inset above.
				lu = 0.5f;	// Set outline fade at middle.
			}

			// Looping
			p0 = &pts[path->count-1];
			p1 = &pts[0];

			for (j = 0; j < path->count; ++j) {
				if ((p1->flags & (NVG_PT_BEVEL | NVG_PR_INNERBEVEL)) != 0) {
					dst = nvg__bevelJoin(dst, p0, p1, lw, rw, lu, ru, ctx->fringeWidth);
				} else {
					nvg__vset(dst, p1->x + (p1->dmx * lw), p1->y + (p1->dmy * lw), lu,1); dst++;
					nvg__vset(dst, p1->x - (p1->dmx * rw), p1->y - (p1->dmy * rw), ru,1); dst++;
				}
				p0 = p1++;
			}

			// Loop it
			nvg__vset(dst, verts[0].x, verts[0].y, lu,1); dst++;
			nvg__vset(dst, verts[1].x, verts[1].y, ru,1); dst++;

			path->nstroke = (int)(dst - verts);
			verts = dst;
		} else {
			path->stroke = NULL;
			path->nstroke = 0;
		}
	}

	return 1;
}



NVGpathContext* nvgpCreate(float tessTol, float distTol, float fringeWidth)
{
	NVGcontext* ctx = (NVGcontext*)malloc(sizeof(NVGcontext));
	if (ctx == NULL) return NULL;
	memset(ctx, 0, sizeof(NVGcontext));
	ctx->commands = (float*)malloc(sizeof(float)*NVG_INIT_COMMANDS_SIZE);
	ctx->cache = nvg__allocPathCache();
	if (ctx->commands == NULL || ctx->cache == NULL) {
		nvgpDelete(ctx);
		return NULL;
	}
	ctx->ccommands = NVG_INIT_COMMANDS_SIZE;
	ctx->tessTol = tessTol;
	ctx->distTol = distTol;
	ctx->fringeWidth = fringeWidth;
	return ctx;
}

void nvgpDelete(NVGpathContext* ctx)
{
	if (ctx == NULL) return;
	if (ctx->commands != NULL) free(ctx->commands);
	nvg__deletePathCache(ctx->cache);
	free(ctx);
}

void nvgpBeginPath(NVGpathContext* ctx)
{
	ctx->ncommands = 0;
	nvg__clearPathCache(ctx);
}

void nvgpMoveTo(NVGpathContext* ctx, float x, float y)
{
	float vals[] = { NVG_MOVETO, x, y };
	nvg__appendCommands(ctx, vals, 3);
}

void nvgpLineTo(NVGpathContext* ctx, float x, float y)
{
	float vals[] = { NVG_LINETO, x, y };
	nvg__appendCommands(ctx, vals, 3);
}

void nvgpBezierTo(NVGpathContext* ctx, float c1x, float c1y, float c2x, float c2y, float x, float y)
{
	float vals[] = { NVG_BEZIERTO, c1x, c1y, c2x, c2y, x, y };
	nvg__appendCommands(ctx, vals, 7);
}

void nvgpClosePath(NVGpathContext* ctx)
{
	float vals[] = { NVG_CLOSE };
	nvg__appendCommands(ctx, vals, 1);
}

void nvgpFlatten(NVGpathContext* ctx, int enforceWinding)
{
	nvg__clearPathCache(ctx);
	nvg__flattenPaths(ctx, enforceWinding);
}

int nvgpPathCount(const NVGpathContext* ctx)
{
	return ctx->cache->npaths;
}

const NVGpath* nvgpPath(const NVGpathContext* ctx, int index)
{
	return &ctx->cache->paths[index];
}

void nvgpPoint(const NVGpathContext* ctx, int index, float* x, float* y)
{
	*x = ctx->cache->points[index].x;
	*y = ctx->cache->points[index].y;
}

int nvgpExpandStroke(NVGpathContext* ctx, float width, int lineCap, int lineJoin, float miterLimit)
{
	return nvg__expandStroke(ctx, width * 0.5f, ctx->fringeWidth, lineCap, lineJoin, miterLimit);
}

int nvgpExpandFill(NVGpathContext* ctx, int antiAlias, int halfFringe)
{
	return nvg__expandFill(ctx, antiAlias ? ctx->fringeWidth : 0.0f, NVG_MITER, 2.4f, halfFringe);
}
