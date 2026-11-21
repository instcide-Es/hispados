/*
 * [HISPA] Lo que le falta a libcss 0.9.2 para la web de hoy (el Navegador
 * completo de Hispa-486, NAVEGA2.EXE: docs/486/NAVEGADOR2.md): leer
 *   row-gap, gap y grid-gap (row-gap y column-gap),
 *   border-radius y border-*-radius (un radio por esquina: el primero de
 *     cada una; la parte de después de «/», la de las elipses, se lee y no
 *     se guarda),
 *   box-shadow (la primera sombra: x, y, difuminado, extensión, color, inset),
 *   grid-template-columns y grid-column (se guarda el texto del valor, con
 *     un espacio entre cada parte: lo lee layout_grid.c de NetSurf).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

/* inherit, initial, revert, unset: para todas las propiedades de la lista */
static css_error flag_todas(css_language *c, const parserutils_vector *vector,
		int32_t *ctx, css_style *result, const opcode_t *ops, int n,
		bool *hecho)
{
	const css_token *token = parserutils_vector_peek(vector, *ctx);
	enum flag_value flag_value;
	css_error error = CSS_OK;
	int i;

	*hecho = false;
	if (token == NULL)
		return CSS_INVALID;
	flag_value = get_css_flag_value(c, token);
	if (flag_value == FLAG_VALUE__NONE)
		return CSS_OK;
	for (i = 0; i < n && error == CSS_OK; i++)
		error = css_stylesheet_style_flag_value(result, flag_value, ops[i]);
	if (error == CSS_OK)
		parserutils_vector_iterate(vector, ctx);
	*hecho = true;
	return error;
}

/* una longitud (o un porcentaje, si «pct»), no negativa */
static css_error longitud(css_language *c, const parserutils_vector *vector,
		int32_t *ctx, bool pct, css_fixed *len, uint32_t *unit)
{
	int32_t orig_ctx = *ctx;
	css_error error;

	consumeWhitespace(vector, ctx);
	error = css__parse_unit_specifier(c, vector, ctx, UNIT_PX, len, unit);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}
	if ((*unit & (pct ? (UNIT_LENGTH | UNIT_PCT) : UNIT_LENGTH)) == 0 ||
			*len < 0) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}
	return CSS_OK;
}

static bool es_ident(css_language *c, const css_token *t, int cual)
{
	bool match;
	return t != NULL && t->type == CSS_TOKEN_IDENT &&
		lwc_string_caseless_isequal(t->idata, c->strings[cual],
				&match) == lwc_error_ok && match;
}

/* ---- row-gap, gap, grid-gap ---- */

/* un hueco: «normal» o una longitud o porcentaje */
static css_error hueco(css_language *c, const parserutils_vector *vector,
		int32_t *ctx, bool *normal, css_fixed *len, uint32_t *unit)
{
	const css_token *token;

	consumeWhitespace(vector, ctx);
	token = parserutils_vector_peek(vector, *ctx);
	if (es_ident(c, token, NORMAL)) {
		parserutils_vector_iterate(vector, ctx);
		*normal = true;
		return CSS_OK;
	}
	*normal = false;
	return longitud(c, vector, ctx, true, len, unit);
}

static css_error pon_hueco(css_style *result, opcode_t op, bool normal,
		css_fixed len, uint32_t unit)
{
	css_error error;
	assert(ROW_GAP_SET == (enum op_row_gap) COLUMN_GAP_SET);
	if (normal)
		return css__stylesheet_style_appendOPV(result, op, 0,
				ROW_GAP_NORMAL);
	error = css__stylesheet_style_appendOPV(result, op, 0, ROW_GAP_SET);
	if (error == CSS_OK)
		error = css__stylesheet_style_vappend(result, 2, len, unit);
	return error;
}

css_error css__parse_row_gap(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	static const opcode_t ops[] = { CSS_PROP_ROW_GAP };
	int32_t orig_ctx = *ctx;
	css_fixed len = 0;
	uint32_t unit = 0;
	bool hecho, normal;
	css_error error = flag_todas(c, vector, ctx, result, ops, 1, &hecho);

	if (hecho || error != CSS_OK)
		return error;
	error = hueco(c, vector, ctx, &normal, &len, &unit);
	if (error == CSS_OK)
		error = pon_hueco(result, CSS_PROP_ROW_GAP, normal, len, unit);
	if (error != CSS_OK)
		*ctx = orig_ctx;
	return error;
}

/* gap: <fila> [<columna>] (grid-gap, el nombre de antes, igual) */
css_error css__parse_gap(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	static const opcode_t ops[] = { CSS_PROP_ROW_GAP, CSS_PROP_COLUMN_GAP };
	int32_t orig_ctx = *ctx, antes;
	css_fixed len[2] = { 0, 0 };
	uint32_t unit[2] = { 0, 0 };
	bool normal[2], hecho;
	const css_token *token;
	css_error error = flag_todas(c, vector, ctx, result, ops, 2, &hecho);

	if (hecho || error != CSS_OK)
		return error;
	error = hueco(c, vector, ctx, &normal[0], &len[0], &unit[0]);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}
	normal[1] = normal[0]; len[1] = len[0]; unit[1] = unit[0];
	antes = *ctx;
	consumeWhitespace(vector, ctx);
	token = parserutils_vector_peek(vector, *ctx);
	if (token != NULL && !tokenIsChar(token, '!')) {
		error = hueco(c, vector, ctx, &normal[1], &len[1], &unit[1]);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}
	} else {
		*ctx = antes;
	}
	error = pon_hueco(result, CSS_PROP_ROW_GAP, normal[0], len[0], unit[0]);
	if (error == CSS_OK)
		error = pon_hueco(result, CSS_PROP_COLUMN_GAP, normal[1],
				len[1], unit[1]);
	if (error != CSS_OK)
		*ctx = orig_ctx;
	return error;
}

/* ---- border-radius ---- */

static css_error pon_radio(css_style *result, opcode_t op, css_fixed len,
		uint32_t unit)
{
	css_error error = css__stylesheet_style_appendOPV(result, op, 0,
			BORDER_RADIUS_SET);
	if (error == CSS_OK)
		error = css__stylesheet_style_vappend(result, 2, len, unit);
	return error;
}

/* una esquina: «r» o «rx ry» (se guarda rx) */
static css_error esquina(css_language *c, const parserutils_vector *vector,
		int32_t *ctx, css_style *result, opcode_t op)
{
	const opcode_t ops[] = { op };
	int32_t orig_ctx = *ctx, antes;
	css_fixed len = 0, len2;
	uint32_t unit = 0, unit2;
	bool hecho;
	css_error error = flag_todas(c, vector, ctx, result, ops, 1, &hecho);

	if (hecho || error != CSS_OK)
		return error;
	error = longitud(c, vector, ctx, true, &len, &unit);
	if (error != CSS_OK)
		return error;
	antes = *ctx;
	if (longitud(c, vector, ctx, true, &len2, &unit2) != CSS_OK)
		*ctx = antes;
	error = pon_radio(result, op, len, unit);
	if (error != CSS_OK)
		*ctx = orig_ctx;
	return error;
}

css_error css__parse_border_top_left_radius(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	return esquina(c, vector, ctx, result, CSS_PROP_BORDER_TOP_LEFT_RADIUS);
}

css_error css__parse_border_top_right_radius(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	return esquina(c, vector, ctx, result, CSS_PROP_BORDER_TOP_RIGHT_RADIUS);
}

css_error css__parse_border_bottom_right_radius(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	return esquina(c, vector, ctx, result,
			CSS_PROP_BORDER_BOTTOM_RIGHT_RADIUS);
}

css_error css__parse_border_bottom_left_radius(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	return esquina(c, vector, ctx, result,
			CSS_PROP_BORDER_BOTTOM_LEFT_RADIUS);
}

/* border-radius: 1 a 4 radios (arriba-izq., arriba-der., abajo-der.,
 * abajo-izq., como margin) [ / 1 a 4 más, que no se guardan ] */
css_error css__parse_border_radius(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	static const opcode_t ops[] = { CSS_PROP_BORDER_TOP_LEFT_RADIUS,
		CSS_PROP_BORDER_TOP_RIGHT_RADIUS,
		CSS_PROP_BORDER_BOTTOM_RIGHT_RADIUS,
		CSS_PROP_BORDER_BOTTOM_LEFT_RADIUS };
	static const int cual[4][4] = {
		{ 0, 0, 0, 0 }, { 0, 1, 0, 1 }, { 0, 1, 2, 1 }, { 0, 1, 2, 3 } };
	int32_t orig_ctx = *ctx, antes;
	css_fixed len[4];
	uint32_t unit[4];
	int n = 0, i;
	bool hecho;
	const css_token *token;
	css_error error = flag_todas(c, vector, ctx, result, ops, 4, &hecho);

	if (hecho || error != CSS_OK)
		return error;
	while (n < 4) {
		antes = *ctx;
		if (longitud(c, vector, ctx, true, &len[n], &unit[n]) != CSS_OK) {
			*ctx = antes;
			break;
		}
		n++;
	}
	if (n == 0) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}
	consumeWhitespace(vector, ctx);
	token = parserutils_vector_peek(vector, *ctx);
	if (token != NULL && tokenIsChar(token, '/')) {
		css_fixed l;
		uint32_t u;
		int m = 0;
		parserutils_vector_iterate(vector, ctx);
		while (m < 4) {
			antes = *ctx;
			if (longitud(c, vector, ctx, true, &l, &u) != CSS_OK) {
				*ctx = antes;
				break;
			}
			m++;
		}
		if (m == 0) {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}
	}
	for (i = 0; i < 4 && error == CSS_OK; i++)
		error = pon_radio(result, ops[i], len[cual[n - 1][i]],
				unit[cual[n - 1][i]]);
	if (error != CSS_OK)
		*ctx = orig_ctx;
	return error;
}

/* ---- box-shadow ---- */

/* salta hasta el final del valor (o hasta «!important») */
static void hasta_el_final(const parserutils_vector *vector, int32_t *ctx)
{
	const css_token *token;
	while ((token = parserutils_vector_peek(vector, *ctx)) != NULL &&
			!tokenIsChar(token, '!'))
		parserutils_vector_iterate(vector, ctx);
}

/* none | [inset? && <x> <y> <difuminado>? <extensión>? && <color>?]#
 * (se guarda la primera; las demás se leen y no se guardan) */
css_error css__parse_box_shadow(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	static const opcode_t ops[] = { CSS_PROP_BOX_SHADOW };
	int32_t orig_ctx = *ctx, antes;
	css_fixed len[4] = { 0, 0, 0, 0 };
	uint32_t unit[4] = { UNIT_PX, UNIT_PX, UNIT_PX, UNIT_PX };
	uint32_t color = 0xff000000;
	uint16_t cvalor = 0;
	bool inset = false, hay_color = false, hecho;
	int n = 0, i;
	const css_token *token;
	css_error error = flag_todas(c, vector, ctx, result, ops, 1, &hecho);

	if (hecho || error != CSS_OK)
		return error;
	consumeWhitespace(vector, ctx);
	token = parserutils_vector_peek(vector, *ctx);
	if (es_ident(c, token, NONE)) {
		parserutils_vector_iterate(vector, ctx);
		return css__stylesheet_style_appendOPV(result,
				CSS_PROP_BOX_SHADOW, 0, BOX_SHADOW_NONE);
	}
	for (;;) {
		consumeWhitespace(vector, ctx);
		token = parserutils_vector_peek(vector, *ctx);
		if (token == NULL || tokenIsChar(token, '!') ||
				tokenIsChar(token, ','))
			break;
		if (es_ident(c, token, INSET) && !inset) {
			parserutils_vector_iterate(vector, ctx);
			inset = true;
			continue;
		}
		antes = *ctx;
		if (n < 4 && (token->type == CSS_TOKEN_NUMBER ||
				token->type == CSS_TOKEN_DIMENSION)) {
			bool neg_ok = n < 2;
			css_fixed l;
			uint32_t u;
			error = css__parse_unit_specifier(c, vector, ctx, UNIT_PX,
					&l, &u);
			if (error == CSS_OK && (u & UNIT_LENGTH) != 0 &&
					(neg_ok || n == 3 || l >= 0)) {
				len[n] = l;
				unit[n] = u;
				n++;
				continue;
			}
			*ctx = antes;
		}
		if (!hay_color) {
			uint32_t col;
			uint16_t v;
			error = css__parse_colour_specifier(c, vector, ctx, &v, &col);
			if (error == CSS_OK) {
				hay_color = true;
				cvalor = v;
				color = col;
				continue;
			}
			*ctx = antes;
		}
		*ctx = orig_ctx;
		return CSS_INVALID;
	}
	if (n < 2) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}
	/* las otras sombras: se saltan */
	hasta_el_final(vector, ctx);
	/* sin color, el del texto (currentColor): aquí, negro al 25 % (lo más
	 * corriente en las sombras de Bootstrap es negro con poca opacidad) */
	if (!hay_color || cvalor != COLOR_SET)
		color = hay_color && cvalor == COLOR_TRANSPARENT ? 0 : 0x40000000;
	error = css__stylesheet_style_appendOPV(result, CSS_PROP_BOX_SHADOW, 0,
			inset ? BOX_SHADOW_INSET : BOX_SHADOW_SET);
	for (i = 0; i < 4 && error == CSS_OK; i++)
		error = css__stylesheet_style_vappend(result, 2, len[i], unit[i]);
	if (error == CSS_OK)
		error = css__stylesheet_style_append(result, color);
	if (error != CSS_OK)
		*ctx = orig_ctx;
	return error;
}

/* ---- grid-template-columns y grid-column: el texto ---- */

/* junta las partes del valor en un texto («repeat( auto-fill , minmax(
 * 230px , 1fr ) )»: los paréntesis y las comas, sueltos) */
static css_error texto_del_valor(css_language *c,
		const parserutils_vector *vector, int32_t *ctx, opcode_t op,
		int nada, uint16_t op_nada, uint16_t op_si, css_style *result)
{
	const opcode_t ops[] = { op };
	int32_t orig_ctx = *ctx;
	char buf[512];
	size_t n = 0;
	const css_token *token;
	lwc_string *texto;
	uint32_t snumber;
	bool hecho;
	int partes = 0;
	css_error error = flag_todas(c, vector, ctx, result, ops, 1, &hecho);

	if (hecho || error != CSS_OK)
		return error;
	consumeWhitespace(vector, ctx);
	token = parserutils_vector_peek(vector, *ctx);
	if (es_ident(c, token, nada)) {
		parserutils_vector_iterate(vector, ctx);
		return css__stylesheet_style_appendOPV(result, op, 0, op_nada);
	}
	while ((token = parserutils_vector_peek(vector, *ctx)) != NULL &&
			!tokenIsChar(token, '!')) {
		const char *p = NULL;
		size_t l = 0;
		const char *despues = "";
		parserutils_vector_iterate(vector, ctx);
		switch (token->type) {
		case CSS_TOKEN_S:
			continue;
		case CSS_TOKEN_IDENT:
		case CSS_TOKEN_NUMBER:
		case CSS_TOKEN_DIMENSION:
			p = lwc_string_data(token->idata);
			l = lwc_string_length(token->idata);
			break;
		case CSS_TOKEN_PERCENTAGE:
			p = lwc_string_data(token->idata);
			l = lwc_string_length(token->idata);
			despues = "%";
			break;
		case CSS_TOKEN_FUNCTION:
			p = lwc_string_data(token->idata);
			l = lwc_string_length(token->idata);
			despues = "(";
			break;
		case CSS_TOKEN_CHAR:
			p = (const char *) token->data.data;
			l = token->data.len;
			break;
		default:
			/* (nombres de líneas [a], cadenas...: no se entienden) */
			*ctx = orig_ctx;
			return CSS_INVALID;
		}
		if (n + l + strlen(despues) + 2 >= sizeof buf) {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}
		if (n > 0)
			buf[n++] = ' ';
		memcpy(buf + n, p, l);
		n += l;
		memcpy(buf + n, despues, strlen(despues));
		n += strlen(despues);
		partes++;
	}
	if (partes == 0) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}
	if (lwc_intern_string(buf, n, &texto) != lwc_error_ok) {
		*ctx = orig_ctx;
		return CSS_NOMEM;
	}
	error = css__stylesheet_string_add(c->sheet, texto, &snumber);
	if (error == CSS_OK)
		error = css__stylesheet_style_appendOPV(result, op, 0, op_si);
	if (error == CSS_OK)
		error = css__stylesheet_style_append(result, snumber);
	if (error != CSS_OK)
		*ctx = orig_ctx;
	return error;
}

css_error css__parse_grid_template_columns(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	return texto_del_valor(c, vector, ctx, CSS_PROP_GRID_TEMPLATE_COLUMNS,
			NONE, GRID_TEMPLATE_COLUMNS_NONE,
			GRID_TEMPLATE_COLUMNS_SET, result);
}

css_error css__parse_grid_column(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	return texto_del_valor(c, vector, ctx, CSS_PROP_GRID_COLUMN,
			AUTO, GRID_COLUMN_AUTO, GRID_COLUMN_SET, result);
}
