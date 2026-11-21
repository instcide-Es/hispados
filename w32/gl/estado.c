/* estado.c - HispaGL: glEnable/glDisable, lo que se fija con una llamada
 * (mezcla, profundidad, niebla, caras...), glGet*, los errores, glGetString,
 * glPushAttrib/glPopAttrib, y el arranque del estado de un contexto.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hgl.h"

Ctx *hgl_c;

const char hgl_extensiones[] =
	"GL_ARB_multitexture GL_SGIS_multitexture GL_EXT_texture_env_add GL_ARB_texture_env_add "
	"GL_EXT_bgra GL_EXT_compiled_vertex_array GL_EXT_texture_edge_clamp GL_SGIS_texture_edge_clamp "
	"GL_EXT_paletted_texture GL_EXT_shared_texture_palette GL_WIN_swap_hint";

void hgl_error(Ctx *C, int e)
{
	if (!C->error) C->error = e;
	hgl_traza("error de GL", (u32)e);
}

static const float negro[4] = { 0, 0, 0, 1 }, blanco[4] = { 1, 1, 1, 1 };
static void c4(float *d, const float *s) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; }
static void c4v(float *d, float a, float b, float c, float e) { d[0] = a; d[1] = b; d[2] = c; d[3] = e; }

void hgl_estado_ini(Ctx *C)
{
	int k, u;
	hgl_pon(&C->hab, 0, sizeof C->hab);
	C->hab.tramado = 1;
	c4v(C->cur.col, 1, 1, 1, 1);
	C->cur.nor[0] = C->cur.nor[1] = 0; C->cur.nor[2] = 1;
	for (u = 0; u < UNIDADES; u++) c4v(C->cur.tc[u], 0, 0, 0, 1);
	C->cur.borde = 1;
	c4v(C->cur.rpos, 0, 0, 0, 1); c4v(C->cur.rcol, 1, 1, 1, 1); c4v(C->cur.rtc, 0, 0, 0, 1);
	C->cur.rvalida = 1;
	/* las luces */
	for (k = 0; k < LUCES; k++) {
		Luz *l = &C->luz.l[k];
		c4(l->amb, negro);
		c4(k ? l->dif : l->dif, k ? negro : blanco);
		c4(l->esp, k ? negro : blanco);
		c4v(l->pos, 0, 0, 1, 0);
		l->dirn[0] = l->dirn[1] = 0; l->dirn[2] = 1;
		l->dir[0] = l->dir[1] = 0; l->dir[2] = -1;
		l->dir_o[0] = l->dir_o[1] = 0; l->dir_o[2] = -1;
		l->exp = 0; l->corte = 180; l->cos_corte = -1;
		l->att[0] = 1; l->att[1] = l->att[2] = 0;
	}
	for (k = 0; k < 2; k++) {
		Material *m = &C->luz.m[k];
		c4v(m->amb, 0.2f, 0.2f, 0.2f, 1);
		c4v(m->dif, 0.8f, 0.8f, 0.8f, 1);
		c4(m->esp, negro);
		c4(m->emi, negro);
		m->brillo = 0;
	}
	c4v(C->luz.amb, 0.2f, 0.2f, 0.2f, 1);
	C->luz.local = C->luz.dos_caras = 0;
	C->luz.cm_cara = GL_FRONT_AND_BACK; C->luz.cm_modo = GL_AMBIENT_AND_DIFFUSE;
	C->luz.sombreado = GL_SMOOTH;
	/* el color */
	C->colb.alfa_f = GL_ALWAYS; C->colb.alfa_ref = 0;
	C->colb.m_or = GL_ONE; C->colb.m_de = GL_ZERO;
	for (k = 0; k < 4; k++) C->colb.mascara[k] = 1;
	c4v(C->colb.borra, 0, 0, 0, 0);
	C->colb.dibuja = GL_BACK; C->colb.logica_op = GL_COPY;
	C->profb.f = GL_LESS; C->profb.escribe = 1; C->profb.borra = 1;
	C->nie.modo = GL_EXP; C->nie.dens = 1; C->nie.ini = 0; C->nie.fin = 1; c4v(C->nie.col, 0, 0, 0, 0);
	C->pol.cara = GL_BACK; C->pol.frente = GL_CCW; C->pol.modo[0] = C->pol.modo[1] = GL_FILL;
	C->pol.desp_f = C->pol.desp_u = 0;
	for (u = 0; u < UNIDADES; u++) {
		C->tex.env[u] = GL_MODULATE;
		c4v(C->tex.env_col[u], 0, 0, 0, 0);
		for (k = 0; k < 4; k++) {
			TexGen *g = &C->tex.gen[u][k];
			g->modo = GL_EYE_LINEAR;
			c4v(g->obj, k == 0, k == 1, 0, 0);
			c4v(g->ojo, k == 0, k == 1, 0, 0);
		}
	}
	C->tex.activa = 0;
	C->vis.x = C->vis.y = 0; C->vis.w = C->an; C->vis.h = C->al; C->vis.cerca = 0; C->vis.lejos = 1;
	C->tij.x = C->tij.y = 0; C->tij.w = C->an; C->tij.h = C->al;
	C->pix.zoom_x = C->pix.zoom_y = 1;
	for (k = 0; k < 4; k++) { C->pix.escala[k] = 1; C->pix.sesgo[k] = 0; }
	C->pix.leer = GL_BACK;
	C->emp.alin = C->des.alin = 4;
	C->pla.f = GL_ALWAYS; C->pla.masc = C->pla.escr = -1;
	C->pla.op[0] = C->pla.op[1] = C->pla.op[2] = GL_KEEP;
	C->punto = C->linea = 1;
	C->punteado_f = 1; C->punteado_p = 0xFFFF;
	hgl_pon(C->punteo, 0xFF, sizeof C->punteo);
	for (k = 0; k < 5; k++) C->sug[k] = GL_DONT_CARE;
	C->prim = -1;
	C->render_modo = GL_RENDER;
	for (k = 0; k < 4; k++) {
		C->arr.v.tam = 4; C->arr.v.tipo = GL_FLOAT;
	}
	C->arr.n.tipo = GL_FLOAT; C->arr.n.tam = 3;
	C->arr.c.tipo = GL_FLOAT; C->arr.c.tam = 4;
	for (u = 0; u < UNIDADES; u++) { C->arr.t[u].tipo = GL_FLOAT; C->arr.t[u].tam = 4; }
	hgl_estado_cambia(C);
}

/* lo que se precalcula al cambiar el estado */
void hgl_estado_cambia(Ctx *C)
{
	int k, u;
	C->necesita_ojo = C->hab.luz || C->hab.niebla;
	for (k = 0; k < PLANOS; k++) if (C->hab.plano[k]) C->necesita_ojo = 1;
	for (u = 0; u < UNIDADES; u++)
		for (k = 0; k < 4; k++)
			if (C->hab.gen[u][k] && C->tex.gen[u][k].modo != GL_OBJECT_LINEAR) C->necesita_ojo = 1;
}

/* ---- glEnable y glDisable ---- */
static u8 *bandera(Ctx *C, u32 cap)
{
	int u = C->tex.activa;
	if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + LUCES) return &C->hab.luzi[cap - GL_LIGHT0];
	if (cap >= GL_CLIP_PLANE0 && cap < GL_CLIP_PLANE0 + PLANOS) return &C->hab.plano[cap - GL_CLIP_PLANE0];
	switch (cap) {
	case GL_ALPHA_TEST: return &C->hab.alfa;
	case GL_BLEND: return &C->hab.mezcla;
	case GL_CULL_FACE: return &C->hab.cara;
	case GL_DEPTH_TEST: return &C->hab.prof;
	case GL_FOG: return &C->hab.niebla;
	case GL_LIGHTING: return &C->hab.luz;
	case GL_COLOR_MATERIAL: return &C->hab.colmat;
	case GL_NORMALIZE: return &C->hab.normaliza;
	case GL_SCISSOR_TEST: return &C->hab.tijera;
	case GL_TEXTURE_1D: return &C->hab.tex1d[u];
	case GL_TEXTURE_2D: return &C->hab.tex2d[u];
	case GL_TEXTURE_GEN_S: return &C->hab.gen[u][0];
	case GL_TEXTURE_GEN_T: return &C->hab.gen[u][1];
	case GL_TEXTURE_GEN_R: return &C->hab.gen[u][2];
	case GL_TEXTURE_GEN_Q: return &C->hab.gen[u][3];
	case GL_POLYGON_OFFSET_FILL: return &C->hab.desplaza_r;
	case GL_POLYGON_OFFSET_LINE: return &C->hab.desplaza_l;
	case GL_POLYGON_OFFSET_POINT: return &C->hab.desplaza_p;
	case GL_POLYGON_STIPPLE: return &C->hab.punteado_p;
	case GL_LINE_STIPPLE: return &C->hab.punteado_l;
	case GL_POINT_SMOOTH: return &C->hab.suave_p;
	case GL_LINE_SMOOTH: return &C->hab.suave_l;
	case GL_POLYGON_SMOOTH: return &C->hab.suave_pol;
	case GL_STENCIL_TEST: return &C->hab.plantilla;
	case GL_COLOR_LOGIC_OP: return &C->hab.logica;
	case GL_DITHER: return &C->hab.tramado;
	case GL_AUTO_NORMAL: return &C->hab.autonormal;
	case GL_SHARED_TEXTURE_PALETTE_EXT: return &C->hab.autonormal + 0; /* (se acepta) */
	}
	return NULL;
}
static int es_cliente(u32 cap)
{
	return cap == GL_VERTEX_ARRAY || cap == GL_NORMAL_ARRAY || cap == GL_COLOR_ARRAY || cap == GL_INDEX_ARRAY ||
		cap == GL_TEXTURE_COORD_ARRAY || cap == GL_EDGE_FLAG_ARRAY;
}
static int *cliente(Ctx *C, u32 cap)
{
	switch (cap) {
	case GL_VERTEX_ARRAY: return &C->arr.v.si;
	case GL_NORMAL_ARRAY: return &C->arr.n.si;
	case GL_COLOR_ARRAY: return &C->arr.c.si;
	case GL_INDEX_ARRAY: return &C->arr.i.si;
	case GL_TEXTURE_COORD_ARRAY: return &C->arr.t[C->arr.tex_cli].si;
	case GL_EDGE_FLAG_ARRAY: return &C->arr.b.si;
	}
	return NULL;
}
static void i_habilita(u32 cap, u32 si)
{
	CTX();
	u8 *b;
	if (cap == GL_SHARED_TEXTURE_PALETTE_EXT) return;
	b = bandera(C, cap);
	if (!b) {
		if (es_cliente(cap)) { *cliente(C, cap) = si; return; }     /* (lo admiten algunos) */
		hgl_error(C, GL_INVALID_ENUM);
		return;
	}
	*b = (u8)si;
	/* con GL_COLOR_MATERIAL, el material toma ya el color de ahora */
	if (cap == GL_COLOR_MATERIAL && si) i_color(C->cur.col[0], C->cur.col[1], C->cur.col[2], C->cur.col[3]);
	hgl_estado_cambia(C);
}
void APIENTRY glEnable(GLenum cap) { CTX(); LISTA(i_habilita, 2, cap, 1); i_habilita(cap, 1); }
void APIENTRY glDisable(GLenum cap) { CTX(); LISTA(i_habilita, 2, cap, 0); i_habilita(cap, 0); }
GLboolean APIENTRY glIsEnabled(GLenum cap)
{
	CTX(0);
	u8 *b = bandera(C, cap);
	if (b) return *b;
	if (es_cliente(cap)) return (GLboolean)*cliente(C, cap);
	hgl_error(C, GL_INVALID_ENUM);
	return 0;
}
void APIENTRY glEnableClientState(GLenum a) { CTX(); int *p = cliente(C, a); if (p) *p = 1; else hgl_error(C, GL_INVALID_ENUM); }
void APIENTRY glDisableClientState(GLenum a) { CTX(); int *p = cliente(C, a); if (p) *p = 0; else hgl_error(C, GL_INVALID_ENUM); }

/* ---- lo que se fija con una llamada ---- */
static int es_func(u32 f) { return f >= GL_NEVER && f <= GL_ALWAYS; }
static void i_alfa_f(u32 f, float r) { CTX(); if (!es_func(f)) { hgl_error(C, GL_INVALID_ENUM); return; } C->colb.alfa_f = f; C->colb.alfa_ref = hgl_lim01(r); }
void APIENTRY glAlphaFunc(GLenum f, GLclampf r) { CTX(); LISTA(i_alfa_f, 2, f, F(r)); i_alfa_f(f, r); }
static void i_mezcla_f(u32 s, u32 d) { CTX(); C->colb.m_or = s; C->colb.m_de = d; }
void APIENTRY glBlendFunc(GLenum s, GLenum d) { CTX(); LISTA(i_mezcla_f, 2, s, d); i_mezcla_f(s, d); }
static void i_prof_f(u32 f) { CTX(); if (!es_func(f)) { hgl_error(C, GL_INVALID_ENUM); return; } C->profb.f = f; }
void APIENTRY glDepthFunc(GLenum f) { CTX(); LISTA(i_prof_f, 1, f); i_prof_f(f); }
static void i_prof_m(u32 m) { CTX(); C->profb.escribe = m != 0; }
void APIENTRY glDepthMask(GLboolean m) { CTX(); LISTA(i_prof_m, 1, m); i_prof_m(m); }
static void i_cara(u32 m) { CTX(); C->pol.cara = m; }
void APIENTRY glCullFace(GLenum m) { CTX(); LISTA(i_cara, 1, m); i_cara(m); }
static void i_frente(u32 m) { CTX(); C->pol.frente = m; }
void APIENTRY glFrontFace(GLenum m) { CTX(); LISTA(i_frente, 1, m); i_frente(m); }
static void i_pol_modo(u32 c, u32 m)
{
	CTX();
	if (m != GL_POINT && m != GL_LINE && m != GL_FILL) { hgl_error(C, GL_INVALID_ENUM); return; }
	if (c == GL_FRONT || c == GL_FRONT_AND_BACK) C->pol.modo[0] = m;
	if (c == GL_BACK || c == GL_FRONT_AND_BACK) C->pol.modo[1] = m;
}
void APIENTRY glPolygonMode(GLenum c, GLenum m) { CTX(); LISTA(i_pol_modo, 2, c, m); i_pol_modo(c, m); }
static void i_pol_desp(float f, float u) { CTX(); C->pol.desp_f = f; C->pol.desp_u = u; }
void APIENTRY glPolygonOffset(GLfloat f, GLfloat u) { CTX(); LISTA(i_pol_desp, 2, F(f), F(u)); i_pol_desp(f, u); }
static void i_sombra(u32 m) { CTX(); C->luz.sombreado = m == GL_FLAT ? GL_FLAT : GL_SMOOTH; }
void APIENTRY glShadeModel(GLenum m) { CTX(); LISTA(i_sombra, 1, m); i_sombra(m); }
static void i_borra_col(float r, float g, float b, float a) { CTX(); c4v(C->colb.borra, hgl_lim01(r), hgl_lim01(g), hgl_lim01(b), hgl_lim01(a)); }
void APIENTRY glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { CTX(); LISTA(i_borra_col, 4, F(r), F(g), F(b), F(a)); i_borra_col(r, g, b, a); }
static void i_borra_prof(float d) { CTX(); C->profb.borra = hgl_lim01(d); }
void APIENTRY glClearDepth(GLclampd d) { CTX(); LISTA(i_borra_prof, 1, F((float)d)); i_borra_prof((float)d); }
static void i_masc_col(u32 r, u32 g, u32 b, u32 a) { CTX(); C->colb.mascara[0] = r != 0; C->colb.mascara[1] = g != 0; C->colb.mascara[2] = b != 0; C->colb.mascara[3] = a != 0; }
void APIENTRY glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { CTX(); LISTA(i_masc_col, 4, r, g, b, a); i_masc_col(r, g, b, a); }
static void i_tijera(int x, int y, int w, int h) { CTX(); if (w < 0 || h < 0) { hgl_error(C, GL_INVALID_VALUE); return; } C->tij.x = x; C->tij.y = y; C->tij.w = w; C->tij.h = h; }
void APIENTRY glScissor(GLint x, GLint y, GLsizei w, GLsizei h) { CTX(); LISTA(i_tijera, 4, x, y, w, h); i_tijera(x, y, w, h); }
static void i_linea_a(float w) { CTX(); if (w <= 0) { hgl_error(C, GL_INVALID_VALUE); return; } C->linea = w; }
void APIENTRY glLineWidth(GLfloat w) { CTX(); LISTA(i_linea_a, 1, F(w)); i_linea_a(w); }
static void i_punto_t(float w) { CTX(); if (w <= 0) { hgl_error(C, GL_INVALID_VALUE); return; } C->punto = w; }
void APIENTRY glPointSize(GLfloat w) { CTX(); LISTA(i_punto_t, 1, F(w)); i_punto_t(w); }
static void i_punteado_l(int f, u32 p) { CTX(); C->punteado_f = f < 1 ? 1 : f > 256 ? 256 : f; C->punteado_p = (u16)p; }
void APIENTRY glLineStipple(GLint f, GLushort p) { CTX(); LISTA(i_punteado_l, 2, f, p); i_punteado_l(f, p); }
static void i_punteado_p(const u8 *m) { CTX(); hgl_copia(C->punteo, m, 128); }
void APIENTRY glPolygonStipple(const GLubyte *m)
{
	CTX();
	if (C->lista_n) { hgl_graba_v(C, (void *)i_punteado_p, m, 32); if (C->lista_modo == GL_COMPILE) return; }
	i_punteado_p(m);
}
void APIENTRY glGetPolygonStipple(GLubyte *m) { CTX(); hgl_copia(m, C->punteo, 128); }
static void i_logica(u32 o) { CTX(); C->colb.logica_op = o; }
void APIENTRY glLogicOp(GLenum o) { CTX(); LISTA(i_logica, 1, o); i_logica(o); }
static void i_dibuja_b(u32 m) { CTX(); C->colb.dibuja = m; }
void APIENTRY glDrawBuffer(GLenum m) { CTX(); LISTA(i_dibuja_b, 1, m); i_dibuja_b(m); }
static void i_lee_b(u32 m) { CTX(); C->pix.leer = m; }
void APIENTRY glReadBuffer(GLenum m) { CTX(); LISTA(i_lee_b, 1, m); i_lee_b(m); }
static void i_plan_f(u32 f, int r, u32 m) { CTX(); C->pla.f = f; C->pla.ref = r; C->pla.masc = m; }
void APIENTRY glStencilFunc(GLenum f, GLint r, GLuint m) { CTX(); LISTA(i_plan_f, 3, f, r, m); i_plan_f(f, r, m); }
static void i_plan_m(u32 m) { CTX(); C->pla.escr = m; }
void APIENTRY glStencilMask(GLuint m) { CTX(); LISTA(i_plan_m, 1, m); i_plan_m(m); }
static void i_plan_op(u32 a, u32 b, u32 c) { CTX(); C->pla.op[0] = a; C->pla.op[1] = b; C->pla.op[2] = c; }
void APIENTRY glStencilOp(GLenum a, GLenum b, GLenum c) { CTX(); LISTA(i_plan_op, 3, a, b, c); i_plan_op(a, b, c); }
static void i_plan_borra(int s) { CTX(); C->pla.borra = s; }
void APIENTRY glClearStencil(GLint s) { CTX(); LISTA(i_plan_borra, 1, s); i_plan_borra(s); }
static void i_sugiere(u32 t, u32 m)
{
	CTX();
	int k = t == GL_PERSPECTIVE_CORRECTION_HINT ? 0 : t == GL_POINT_SMOOTH_HINT ? 1 : t == GL_LINE_SMOOTH_HINT ? 2 :
		t == GL_POLYGON_SMOOTH_HINT ? 3 : t == GL_FOG_HINT ? 4 : -1;
	if (k < 0) return;                /* (las de otros: se aceptan) */
	C->sug[k] = m;
}
void APIENTRY glHint(GLenum t, GLenum m) { CTX(); LISTA(i_sugiere, 2, t, m); i_sugiere(t, m); }

/* la niebla */
static void i_niebla(u32 p, float a, float b, float c, float d)
{
	CTX();
	switch (p) {
	case GL_FOG_MODE: C->nie.modo = (int)a; break;
	case GL_FOG_DENSITY: if (a < 0) { hgl_error(C, GL_INVALID_VALUE); return; } C->nie.dens = a; break;
	case GL_FOG_START: C->nie.ini = a; break;
	case GL_FOG_END: C->nie.fin = a; break;
	case GL_FOG_COLOR: c4v(C->nie.col, hgl_lim01(a), hgl_lim01(b), hgl_lim01(c), hgl_lim01(d)); break;
	case GL_FOG_INDEX: C->nie.indice = a; break;
	default: hgl_error(C, GL_INVALID_ENUM);
	}
}
void APIENTRY glFogf(GLenum p, GLfloat v) { CTX(); LISTA(i_niebla, 5, p, F(v), 0, 0, 0); i_niebla(p, v, 0, 0, 0); }
void APIENTRY glFogi(GLenum p, GLint v) { glFogf(p, (float)v); }
void APIENTRY glFogfv(GLenum p, const GLfloat *v)
{
	CTX();
	float a = v[0], b = 0, c = 0, d = 0;
	if (p == GL_FOG_COLOR) { b = v[1]; c = v[2]; d = v[3]; }
	LISTA(i_niebla, 5, p, F(a), F(b), F(c), F(d));
	i_niebla(p, a, b, c, d);
}
void APIENTRY glFogiv(GLenum p, const GLint *v)
{
	float f[4];
	if (p == GL_FOG_COLOR) {
		int k;
		for (k = 0; k < 4; k++) f[k] = (float)v[k] / 2147483647.0f;
	} else f[0] = (float)v[0];
	glFogfv(p, f);
}

/* los planos de recorte (en el espacio del ojo: por la inversa de la modelo-vista) */
static void i_plano(u32 p, float a, float b, float c, float d)
{
	CTX();
	Mat inv;
	float v[4] = { a, b, c, d };
	int k;
	if (p < GL_CLIP_PLANE0 || p >= GL_CLIP_PLANE0 + PLANOS) { hgl_error(C, GL_INVALID_ENUM); return; }
	if (!mat_inversa(&inv, &C->mv[C->nmv])) mat_identidad(&inv);
	/* plano_ojo = plano · M⁻¹ */
	for (k = 0; k < 4; k++)
		C->trf.plano[p - GL_CLIP_PLANE0][k] = v[0] * inv.m[k * 4] + v[1] * inv.m[k * 4 + 1] + v[2] * inv.m[k * 4 + 2] + v[3] * inv.m[k * 4 + 3];
}
void APIENTRY glClipPlane(GLenum p, const GLdouble *e)
{
	CTX();
	LISTA(i_plano, 5, p, F((float)e[0]), F((float)e[1]), F((float)e[2]), F((float)e[3]));
	i_plano(p, (float)e[0], (float)e[1], (float)e[2], (float)e[3]);
}
void APIENTRY glGetClipPlane(GLenum p, GLdouble *e)
{
	CTX();
	int k;
	if (p < GL_CLIP_PLANE0 || p >= GL_CLIP_PLANE0 + PLANOS) { hgl_error(C, GL_INVALID_ENUM); return; }
	for (k = 0; k < 4; k++) e[k] = C->trf.plano[p - GL_CLIP_PLANE0][k];
}

/* el almacén de los píxeles y la transferencia */
void APIENTRY glPixelStorei(GLenum p, GLint v)
{
	CTX();
	Almacen *a = (p >= GL_PACK_SWAP_BYTES && p <= GL_PACK_ALIGNMENT) ? &C->emp : &C->des;
	switch (p) {
	case GL_PACK_ALIGNMENT: case GL_UNPACK_ALIGNMENT:
		if (v != 1 && v != 2 && v != 4 && v != 8) { hgl_error(C, GL_INVALID_VALUE); return; }
		a->alin = v; break;
	case GL_PACK_ROW_LENGTH: case GL_UNPACK_ROW_LENGTH: a->fila = v; break;
	case GL_PACK_SKIP_PIXELS: case GL_UNPACK_SKIP_PIXELS: a->salta_p = v; break;
	case GL_PACK_SKIP_ROWS: case GL_UNPACK_SKIP_ROWS: a->salta_f = v; break;
	case GL_PACK_LSB_FIRST: case GL_UNPACK_LSB_FIRST: a->lsb = v; break;
	case GL_PACK_SWAP_BYTES: case GL_UNPACK_SWAP_BYTES: a->inv = v; break;
	default: hgl_error(C, GL_INVALID_ENUM);
	}
}
void APIENTRY glPixelStoref(GLenum p, GLfloat v) { glPixelStorei(p, (int)v); }
static void i_transf(u32 p, float v)
{
	CTX();
	switch (p) {
	case GL_RED_SCALE: C->pix.escala[0] = v; break;
	case GL_GREEN_SCALE: C->pix.escala[1] = v; break;
	case GL_BLUE_SCALE: C->pix.escala[2] = v; break;
	case GL_ALPHA_SCALE: C->pix.escala[3] = v; break;
	case GL_RED_BIAS: C->pix.sesgo[0] = v; break;
	case GL_GREEN_BIAS: C->pix.sesgo[1] = v; break;
	case GL_BLUE_BIAS: C->pix.sesgo[2] = v; break;
	case GL_ALPHA_BIAS: C->pix.sesgo[3] = v; break;
	default: break;                    /* (los demás se aceptan) */
	}
}
void APIENTRY glPixelTransferf(GLenum p, GLfloat v) { CTX(); LISTA(i_transf, 2, p, F(v)); i_transf(p, v); }
void APIENTRY glPixelTransferi(GLenum p, GLint v) { glPixelTransferf(p, (float)v); }
static void i_zoom(float x, float y) { CTX(); C->pix.zoom_x = x; C->pix.zoom_y = y; }
void APIENTRY glPixelZoom(GLfloat x, GLfloat y) { CTX(); LISTA(i_zoom, 2, F(x), F(y)); i_zoom(x, y); }
void APIENTRY glPixelMapfv(GLenum m, GLsizei n, const GLfloat *v) { (void)m; (void)n; (void)v; }
void APIENTRY glPixelMapuiv(GLenum m, GLsizei n, const GLuint *v) { (void)m; (void)n; (void)v; }
void APIENTRY glPixelMapusv(GLenum m, GLsizei n, const GLushort *v) { (void)m; (void)n; (void)v; }
void APIENTRY glGetPixelMapfv(GLenum m, GLfloat *v) { (void)m; v[0] = 0; }
void APIENTRY glGetPixelMapuiv(GLenum m, GLuint *v) { (void)m; v[0] = 0; }
void APIENTRY glGetPixelMapusv(GLenum m, GLushort *v) { (void)m; v[0] = 0; }

/* lo de los índices de color, la acumulación y la selección: se acepta sin
 * hacer nada (HispaGL es solo RGBA y no tiene búfer de acumulación) */
void APIENTRY glClearIndex(GLfloat c) { (void)c; }
void APIENTRY glIndexMask(GLuint m) { (void)m; }
void APIENTRY glClearAccum(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { (void)r; (void)g; (void)b; (void)a; }
void APIENTRY glAccum(GLenum o, GLfloat v) { (void)o; (void)v; }
static u32 *sel_buf; static int sel_tam, sel_n, sel_pila;
void APIENTRY glSelectBuffer(GLsizei n, GLuint *b) { sel_buf = b; sel_tam = n; }
void APIENTRY glFeedbackBuffer(GLsizei n, GLenum t, GLfloat *b) { (void)n; (void)t; (void)b; }
void APIENTRY glPassThrough(GLfloat t) { (void)t; }
void APIENTRY glInitNames(void) { sel_pila = 0; }
void APIENTRY glLoadName(GLuint n) { (void)n; }
void APIENTRY glPushName(GLuint n) { (void)n; sel_pila++; }
void APIENTRY glPopName(void) { if (sel_pila) sel_pila--; }
GLint APIENTRY glRenderMode(GLenum m)
{
	CTX(0);
	int r = C->render_modo == GL_SELECT ? sel_n : 0;
	if (m != GL_RENDER && m != GL_SELECT && m != GL_FEEDBACK) { hgl_error(C, GL_INVALID_ENUM); return 0; }
	(void)sel_buf; (void)sel_tam;
	C->render_modo = m;
	sel_n = 0;
	return r;
}

GLenum APIENTRY glGetError(void)
{
	CTX(GL_NO_ERROR);
	int e = C->error;
	C->error = 0;
	return e;
}

const GLubyte *APIENTRY glGetString(GLenum n)
{
	switch (n) {
	case GL_VENDOR: return (const GLubyte *)"Hispa-DOS";
	case GL_RENDERER: return (const GLubyte *)"HispaGL (por programa, 16 bits)";
	case GL_VERSION: return (const GLubyte *)"1.1 HispaGL 1.0";
	case GL_EXTENSIONS: return (const GLubyte *)hgl_extensiones;
	}
	if (hgl_c) hgl_error(hgl_c, GL_INVALID_ENUM);
	return NULL;
}

/* ---- glGet*: todo pasa por floats (y se dice si es un color) ---- */
static int pide(Ctx *C, u32 p, float *v, int *es_color)
{
	int k, u = C->tex.activa;
	*es_color = 0;
	if (p >= GL_LIGHT0 && p < GL_LIGHT0 + LUCES) { v[0] = C->hab.luzi[p - GL_LIGHT0]; return 1; }
	if (p >= GL_CLIP_PLANE0 && p < GL_CLIP_PLANE0 + PLANOS) { v[0] = C->hab.plano[p - GL_CLIP_PLANE0]; return 1; }
	switch (p) {
	case GL_CURRENT_COLOR: *es_color = 1; c4(v, C->cur.col); return 4;
	case GL_CURRENT_NORMAL: v[0] = C->cur.nor[0]; v[1] = C->cur.nor[1]; v[2] = C->cur.nor[2]; return 3;
	case GL_CURRENT_TEXTURE_COORDS: c4(v, C->cur.tc[u]); return 4;
	case GL_CURRENT_RASTER_POSITION: c4(v, C->cur.rpos); return 4;
	case GL_CURRENT_RASTER_COLOR: *es_color = 1; c4(v, C->cur.rcol); return 4;
	case GL_CURRENT_RASTER_POSITION_VALID: v[0] = C->cur.rvalida; return 1;
	case GL_CURRENT_RASTER_DISTANCE: v[0] = C->cur.rdist; return 1;
	case GL_CURRENT_RASTER_TEXTURE_COORDS: c4(v, C->cur.rtc); return 4;
	case GL_EDGE_FLAG: v[0] = C->cur.borde; return 1;
	case GL_MODELVIEW_MATRIX: for (k = 0; k < 16; k++) v[k] = C->mv[C->nmv].m[k]; return 16;
	case GL_PROJECTION_MATRIX: for (k = 0; k < 16; k++) v[k] = C->pr[C->npr].m[k]; return 16;
	case GL_TEXTURE_MATRIX: for (k = 0; k < 16; k++) v[k] = C->tx[u][C->ntx[u]].m[k]; return 16;
	case GL_MODELVIEW_STACK_DEPTH: v[0] = C->nmv + 1; return 1;
	case GL_PROJECTION_STACK_DEPTH: v[0] = C->npr + 1; return 1;
	case GL_TEXTURE_STACK_DEPTH: v[0] = C->ntx[u] + 1; return 1;
	case GL_MATRIX_MODE: v[0] = C->trf.modo; return 1;
	case GL_VIEWPORT: v[0] = C->vis.x; v[1] = C->vis.y; v[2] = C->vis.w; v[3] = C->vis.h; return 4;
	case GL_DEPTH_RANGE: v[0] = C->vis.cerca; v[1] = C->vis.lejos; return 2;
	case GL_SCISSOR_BOX: v[0] = C->tij.x; v[1] = C->tij.y; v[2] = C->tij.w; v[3] = C->tij.h; return 4;
	case GL_MAX_VIEWPORT_DIMS: v[0] = v[1] = 4096; return 2;
	case GL_MAX_TEXTURE_SIZE: v[0] = TAM_MAX; return 1;
	case GL_MAX_LIGHTS: v[0] = LUCES; return 1;
	case GL_MAX_CLIP_PLANES: v[0] = PLANOS; return 1;
	case GL_MAX_MODELVIEW_STACK_DEPTH: v[0] = PILA_MV; return 1;
	case GL_MAX_PROJECTION_STACK_DEPTH: v[0] = PILA_PR; return 1;
	case GL_MAX_TEXTURE_STACK_DEPTH: v[0] = PILA_TX; return 1;
	case GL_MAX_ATTRIB_STACK_DEPTH: case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH: v[0] = PILA_ATR; return 1;
	case GL_MAX_LIST_NESTING: v[0] = 64; return 1;
	case GL_MAX_NAME_STACK_DEPTH: v[0] = 64; return 1;
	case GL_MAX_EVAL_ORDER: v[0] = 8; return 1;
	case GL_MAX_PIXEL_MAP_TABLE: v[0] = 256; return 1;
	case GL_MAX_TEXTURE_UNITS_ARB: case GL_MAX_TEXTURES_SGIS: v[0] = UNIDADES; return 1;
	case GL_ACTIVE_TEXTURE_ARB: v[0] = GL_TEXTURE0_ARB + u; return 1;
	case GL_SELECTED_TEXTURE_SGIS: v[0] = GL_TEXTURE0_SGIS + u; return 1;
	case GL_CLIENT_ACTIVE_TEXTURE_ARB: v[0] = GL_TEXTURE0_ARB + C->arr.tex_cli; return 1;
	case GL_RED_BITS: case GL_BLUE_BITS: v[0] = 5; return 1;
	case GL_GREEN_BITS: v[0] = 6; return 1;
	case GL_ALPHA_BITS: case GL_STENCIL_BITS: case GL_INDEX_BITS: case GL_AUX_BUFFERS:
	case GL_ACCUM_RED_BITS: case GL_ACCUM_GREEN_BITS: case GL_ACCUM_BLUE_BITS: case GL_ACCUM_ALPHA_BITS:
		v[0] = 0; return 1;
	case GL_DEPTH_BITS: v[0] = 16; return 1;
	case GL_SUBPIXEL_BITS: v[0] = 4; return 1;
	case GL_DOUBLEBUFFER: v[0] = C->dobles; return 1;
	case GL_STEREO: case GL_INDEX_MODE: v[0] = 0; return 1;
	case GL_RGBA_MODE: v[0] = 1; return 1;
	case GL_COLOR_CLEAR_VALUE: *es_color = 1; c4(v, C->colb.borra); return 4;
	case GL_COLOR_WRITEMASK: for (k = 0; k < 4; k++) v[k] = C->colb.mascara[k]; return 4;
	case GL_DEPTH_CLEAR_VALUE: v[0] = C->profb.borra; return 1;
	case GL_DEPTH_FUNC: v[0] = C->profb.f; return 1;
	case GL_DEPTH_WRITEMASK: v[0] = C->profb.escribe; return 1;
	case GL_BLEND_SRC: v[0] = C->colb.m_or; return 1;
	case GL_BLEND_DST: v[0] = C->colb.m_de; return 1;
	case GL_ALPHA_TEST_FUNC: v[0] = C->colb.alfa_f; return 1;
	case GL_ALPHA_TEST_REF: v[0] = C->colb.alfa_ref; return 1;
	case GL_DRAW_BUFFER: v[0] = C->colb.dibuja; return 1;
	case GL_READ_BUFFER: v[0] = C->pix.leer; return 1;
	case GL_LOGIC_OP_MODE: v[0] = C->colb.logica_op; return 1;
	case GL_CULL_FACE_MODE: v[0] = C->pol.cara; return 1;
	case GL_FRONT_FACE: v[0] = C->pol.frente; return 1;
	case GL_POLYGON_MODE: v[0] = C->pol.modo[0]; v[1] = C->pol.modo[1]; return 2;
	case GL_POLYGON_OFFSET_FACTOR: v[0] = C->pol.desp_f; return 1;
	case GL_POLYGON_OFFSET_UNITS: v[0] = C->pol.desp_u; return 1;
	case GL_SHADE_MODEL: v[0] = C->luz.sombreado; return 1;
	case GL_LIGHT_MODEL_AMBIENT: *es_color = 1; c4(v, C->luz.amb); return 4;
	case GL_LIGHT_MODEL_LOCAL_VIEWER: v[0] = C->luz.local; return 1;
	case GL_LIGHT_MODEL_TWO_SIDE: v[0] = C->luz.dos_caras; return 1;
	case GL_COLOR_MATERIAL_FACE: v[0] = C->luz.cm_cara; return 1;
	case GL_COLOR_MATERIAL_PARAMETER: v[0] = C->luz.cm_modo; return 1;
	case GL_FOG_MODE: v[0] = C->nie.modo; return 1;
	case GL_FOG_DENSITY: v[0] = C->nie.dens; return 1;
	case GL_FOG_START: v[0] = C->nie.ini; return 1;
	case GL_FOG_END: v[0] = C->nie.fin; return 1;
	case GL_FOG_COLOR: *es_color = 1; c4(v, C->nie.col); return 4;
	case GL_FOG_INDEX: v[0] = C->nie.indice; return 1;
	case GL_POINT_SIZE: v[0] = C->punto; return 1;
	case GL_POINT_SIZE_RANGE: v[0] = 1; v[1] = 64; return 2;
	case GL_POINT_SIZE_GRANULARITY: case GL_LINE_WIDTH_GRANULARITY: v[0] = 1; return 1;
	case GL_LINE_WIDTH: v[0] = C->linea; return 1;
	case GL_LINE_WIDTH_RANGE: v[0] = 1; v[1] = 16; return 2;
	case GL_LINE_STIPPLE_PATTERN: v[0] = C->punteado_p; return 1;
	case GL_LINE_STIPPLE_REPEAT: v[0] = C->punteado_f; return 1;
	case GL_LIST_BASE: v[0] = C->lista_base; return 1;
	case GL_LIST_INDEX: v[0] = C->lista_n; return 1;
	case GL_LIST_MODE: v[0] = C->lista_modo; return 1;
	case GL_TEXTURE_BINDING_1D: v[0] = C->tex.lig1d[u] ? C->tex.lig1d[u]->nombre : 0; return 1;
	case GL_TEXTURE_BINDING_2D: v[0] = C->tex.lig2d[u] ? C->tex.lig2d[u]->nombre : 0; return 1;
	case GL_RENDER_MODE: v[0] = C->render_modo; return 1;
	case GL_ATTRIB_STACK_DEPTH: v[0] = C->natr; return 1;
	case GL_CLIENT_ATTRIB_STACK_DEPTH: v[0] = C->natrc; return 1;
	case GL_PERSPECTIVE_CORRECTION_HINT: v[0] = C->sug[0]; return 1;
	case GL_POINT_SMOOTH_HINT: v[0] = C->sug[1]; return 1;
	case GL_LINE_SMOOTH_HINT: v[0] = C->sug[2]; return 1;
	case GL_POLYGON_SMOOTH_HINT: v[0] = C->sug[3]; return 1;
	case GL_FOG_HINT: v[0] = C->sug[4]; return 1;
	case GL_STENCIL_FUNC: v[0] = C->pla.f; return 1;
	case GL_STENCIL_REF: v[0] = C->pla.ref; return 1;
	case GL_STENCIL_VALUE_MASK: v[0] = (float)(u32)C->pla.masc; return 1;
	case GL_STENCIL_WRITEMASK: v[0] = (float)(u32)C->pla.escr; return 1;
	case GL_STENCIL_FAIL: v[0] = C->pla.op[0]; return 1;
	case GL_STENCIL_PASS_DEPTH_FAIL: v[0] = C->pla.op[1]; return 1;
	case GL_STENCIL_PASS_DEPTH_PASS: v[0] = C->pla.op[2]; return 1;
	case GL_STENCIL_CLEAR_VALUE: v[0] = C->pla.borra; return 1;
	case GL_UNPACK_ALIGNMENT: v[0] = C->des.alin; return 1;
	case GL_UNPACK_ROW_LENGTH: v[0] = C->des.fila; return 1;
	case GL_UNPACK_SKIP_PIXELS: v[0] = C->des.salta_p; return 1;
	case GL_UNPACK_SKIP_ROWS: v[0] = C->des.salta_f; return 1;
	case GL_UNPACK_LSB_FIRST: v[0] = C->des.lsb; return 1;
	case GL_UNPACK_SWAP_BYTES: v[0] = C->des.inv; return 1;
	case GL_PACK_ALIGNMENT: v[0] = C->emp.alin; return 1;
	case GL_PACK_ROW_LENGTH: v[0] = C->emp.fila; return 1;
	case GL_PACK_SKIP_PIXELS: v[0] = C->emp.salta_p; return 1;
	case GL_PACK_SKIP_ROWS: v[0] = C->emp.salta_f; return 1;
	case GL_PACK_LSB_FIRST: v[0] = C->emp.lsb; return 1;
	case GL_PACK_SWAP_BYTES: v[0] = C->emp.inv; return 1;
	case GL_ZOOM_X: v[0] = C->pix.zoom_x; return 1;
	case GL_ZOOM_Y: v[0] = C->pix.zoom_y; return 1;
	case GL_VERTEX_ARRAY_SIZE: v[0] = C->arr.v.tam; return 1;
	case GL_VERTEX_ARRAY_TYPE: v[0] = C->arr.v.tipo; return 1;
	case GL_VERTEX_ARRAY_STRIDE: v[0] = C->arr.v.paso; return 1;
	case GL_NORMAL_ARRAY_TYPE: v[0] = C->arr.n.tipo; return 1;
	case GL_NORMAL_ARRAY_STRIDE: v[0] = C->arr.n.paso; return 1;
	case GL_COLOR_ARRAY_SIZE: v[0] = C->arr.c.tam; return 1;
	case GL_COLOR_ARRAY_TYPE: v[0] = C->arr.c.tipo; return 1;
	case GL_COLOR_ARRAY_STRIDE: v[0] = C->arr.c.paso; return 1;
	case GL_TEXTURE_COORD_ARRAY_SIZE: v[0] = C->arr.t[C->arr.tex_cli].tam; return 1;
	case GL_TEXTURE_COORD_ARRAY_TYPE: v[0] = C->arr.t[C->arr.tex_cli].tipo; return 1;
	case GL_TEXTURE_COORD_ARRAY_STRIDE: v[0] = C->arr.t[C->arr.tex_cli].paso; return 1;
	case GL_ARRAY_ELEMENT_LOCK_FIRST_EXT: v[0] = C->bloqueados_ini; return 1;
	case GL_ARRAY_ELEMENT_LOCK_COUNT_EXT: v[0] = C->bloqueados_n; return 1;
	}
	{
		u8 *b = bandera(C, p);
		if (b) { v[0] = *b; return 1; }
		if (es_cliente(p)) { v[0] = *cliente(C, p); return 1; }
	}
	hgl_error(C, GL_INVALID_ENUM);
	return 0;
}
void APIENTRY glGetFloatv(GLenum p, GLfloat *r)
{
	CTX();
	float v[16];
	int c, n = pide(C, p, v, &c), k;
	for (k = 0; k < n; k++) r[k] = v[k];
}
void APIENTRY glGetDoublev(GLenum p, GLdouble *r)
{
	CTX();
	float v[16];
	int c, n = pide(C, p, v, &c), k;
	for (k = 0; k < n; k++) r[k] = v[k];
}
void APIENTRY glGetIntegerv(GLenum p, GLint *r)
{
	CTX();
	float v[16];
	int c, n = pide(C, p, v, &c), k;
	for (k = 0; k < n; k++) {
		if (c) r[k] = (int)(v[k] * 2147483647.0f);
		else r[k] = hgl_red(v[k]);
	}
}
void APIENTRY glGetBooleanv(GLenum p, GLboolean *r)
{
	CTX();
	float v[16];
	int c, n = pide(C, p, v, &c), k;
	for (k = 0; k < n; k++) r[k] = v[k] != 0;
}
void APIENTRY glGetPointerv(GLenum p, GLvoid **r)
{
	CTX();
	switch (p) {
	case GL_VERTEX_ARRAY_POINTER: *r = (void *)C->arr.v.p; break;
	case GL_NORMAL_ARRAY_POINTER: *r = (void *)C->arr.n.p; break;
	case GL_COLOR_ARRAY_POINTER: *r = (void *)C->arr.c.p; break;
	case GL_INDEX_ARRAY_POINTER: *r = (void *)C->arr.i.p; break;
	case GL_TEXTURE_COORD_ARRAY_POINTER: *r = (void *)C->arr.t[C->arr.tex_cli].p; break;
	case GL_EDGE_FLAG_ARRAY_POINTER: *r = (void *)C->arr.b.p; break;
	case GL_SELECTION_BUFFER_POINTER: *r = sel_buf; break;
	default: *r = NULL; hgl_error(C, GL_INVALID_ENUM);
	}
}

/* ---- glPushAttrib y glPopAttrib ---- */
void APIENTRY glPushAttrib(GLbitfield m)
{
	CTX();
	Atrib *a;
	if (!C->atr) C->atr = hgl_pedir(sizeof(Atrib) * PILA_ATR);
	if (!C->atr) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	if (C->natr >= PILA_ATR) { hgl_error(C, GL_STACK_OVERFLOW); return; }
	a = &C->atr[C->natr++];
	a->mascara = m;
	a->hab = C->hab; a->cur = C->cur; a->luz = C->luz; a->col = C->colb; a->prof = C->profb;
	a->nie = C->nie; a->pol = C->pol; a->tex = C->tex; a->vis = C->vis; a->tij = C->tij; a->trf = C->trf;
	a->pix = C->pix; a->punto = C->punto; a->linea = C->linea; a->punteado_f = C->punteado_f;
	a->punteado_p = C->punteado_p; hgl_copia(a->sug, C->sug, sizeof a->sug); a->lista_base = C->lista_base;
	a->pla = C->pla; hgl_copia(a->punteo, C->punteo, 128);
}
void APIENTRY glPopAttrib(void)
{
	CTX();
	Atrib *a;
	u32 m;
	int k, u;
	if (!C->natr) { hgl_error(C, GL_STACK_UNDERFLOW); return; }
	a = &C->atr[--C->natr];
	m = a->mascara;
	if (m & GL_ENABLE_BIT) C->hab = a->hab;
	if (m & GL_CURRENT_BIT) C->cur = a->cur;
	if (m & GL_LIGHTING_BIT) {
		C->luz = a->luz;
		C->hab.luz = a->hab.luz; C->hab.colmat = a->hab.colmat;
		for (k = 0; k < LUCES; k++) C->hab.luzi[k] = a->hab.luzi[k];
	}
	if (m & GL_COLOR_BUFFER_BIT) {
		C->colb = a->col;
		C->hab.alfa = a->hab.alfa; C->hab.mezcla = a->hab.mezcla; C->hab.logica = a->hab.logica; C->hab.tramado = a->hab.tramado;
	}
	if (m & GL_DEPTH_BUFFER_BIT) { C->profb = a->prof; C->hab.prof = a->hab.prof; }
	if (m & GL_FOG_BIT) { C->nie = a->nie; C->hab.niebla = a->hab.niebla; }
	if (m & GL_POLYGON_BIT) {
		C->pol = a->pol;
		C->hab.cara = a->hab.cara; C->hab.desplaza_r = a->hab.desplaza_r; C->hab.desplaza_l = a->hab.desplaza_l;
		C->hab.desplaza_p = a->hab.desplaza_p; C->hab.punteado_p = a->hab.punteado_p; C->hab.suave_pol = a->hab.suave_pol;
	}
	if (m & GL_POLYGON_STIPPLE_BIT) hgl_copia(C->punteo, a->punteo, 128);
	if (m & GL_TEXTURE_BIT) {
		C->tex = a->tex;
		for (u = 0; u < UNIDADES; u++) {
			C->hab.tex1d[u] = a->hab.tex1d[u]; C->hab.tex2d[u] = a->hab.tex2d[u];
			for (k = 0; k < 4; k++) C->hab.gen[u][k] = a->hab.gen[u][k];
		}
	}
	if (m & GL_VIEWPORT_BIT) C->vis = a->vis;
	if (m & GL_SCISSOR_BIT) { C->tij = a->tij; C->hab.tijera = a->hab.tijera; }
	if (m & GL_TRANSFORM_BIT) {
		C->trf = a->trf;
		C->hab.normaliza = a->hab.normaliza;
		for (k = 0; k < PLANOS; k++) C->hab.plano[k] = a->hab.plano[k];
	}
	if (m & GL_PIXEL_MODE_BIT) C->pix = a->pix;
	if (m & GL_POINT_BIT) { C->punto = a->punto; C->hab.suave_p = a->hab.suave_p; }
	if (m & GL_LINE_BIT) {
		C->linea = a->linea; C->punteado_f = a->punteado_f; C->punteado_p = a->punteado_p;
		C->hab.suave_l = a->hab.suave_l; C->hab.punteado_l = a->hab.punteado_l;
	}
	if (m & GL_HINT_BIT) hgl_copia(C->sug, a->sug, sizeof a->sug);
	if (m & GL_LIST_BIT) C->lista_base = a->lista_base;
	if (m & GL_STENCIL_BUFFER_BIT) { C->pla = a->pla; C->hab.plantilla = a->hab.plantilla; }
	if (m & GL_EVAL_BIT) C->hab.autonormal = a->hab.autonormal;
	hgl_estado_cambia(C);
}
void APIENTRY glPushClientAttrib(GLbitfield m)
{
	CTX();
	AtribCli *a;
	if (!C->atrc) C->atrc = hgl_pedir(sizeof(AtribCli) * PILA_ATR);
	if (!C->atrc) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	if (C->natrc >= PILA_ATR) { hgl_error(C, GL_STACK_OVERFLOW); return; }
	a = &C->atrc[C->natrc++];
	a->mascara = m; a->emp = C->emp; a->des = C->des; a->arr = C->arr;
}
void APIENTRY glPopClientAttrib(void)
{
	CTX();
	AtribCli *a;
	if (!C->natrc) { hgl_error(C, GL_STACK_UNDERFLOW); return; }
	a = &C->atrc[--C->natrc];
	if (a->mascara & GL_CLIENT_PIXEL_STORE_BIT) { C->emp = a->emp; C->des = a->des; }
	if (a->mascara & GL_CLIENT_VERTEX_ARRAY_BIT) C->arr = a->arr;
}
