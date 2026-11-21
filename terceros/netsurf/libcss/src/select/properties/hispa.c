/*
 * [HISPA] La selección (cascada, valores iniciales, herencia) de lo que se
 * añade a libcss para el Navegador completo de Hispa-486 (NAVEGA2.EXE,
 * docs/486/NAVEGADOR2.md): row-gap, border-*-radius, box-shadow,
 * grid-template-columns y grid-column. Ver src/parse/properties/hispa.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

/* ---- row-gap (como column-gap; «normal»: 0 en flex y grid) ---- */

css_error css__cascade_row_gap(uint32_t opv, css_style *style,
		css_select_state *state)
{
	return css__cascade_length_normal(opv, style, state, set_row_gap);
}

css_error css__set_row_gap_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_row_gap(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_row_gap(css_select_state *state)
{
	return set_row_gap(state->computed, CSS_ROW_GAP_NORMAL,
			0, CSS_UNIT_PX);
}

css_error css__copy_row_gap(const css_computed_style *from,
		css_computed_style *to)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_row_gap(from, &length, &unit);

	if (from == to)
		return CSS_OK;
	return set_row_gap(to, type, length, unit);
}

css_error css__compose_row_gap(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_row_gap(child, &length, &unit);

	return css__copy_row_gap(type == CSS_ROW_GAP_INHERIT ? parent : child,
			result);
}

/* ---- border-top_left-radius ---- */

css_error css__cascade_border_top_left_radius(uint32_t opv, css_style *style,
		css_select_state *state)
{
	return css__cascade_length(opv, style, state, set_border_top_left_radius);
}

css_error css__set_border_top_left_radius_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_border_top_left_radius(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_border_top_left_radius(css_select_state *state)
{
	return set_border_top_left_radius(state->computed, CSS_BORDER_RADIUS_SET,
			0, CSS_UNIT_PX);
}

css_error css__copy_border_top_left_radius(const css_computed_style *from,
		css_computed_style *to)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_border_top_left_radius(from, &length, &unit);

	if (from == to)
		return CSS_OK;
	return set_border_top_left_radius(to, type, length, unit);
}

css_error css__compose_border_top_left_radius(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_border_top_left_radius(child, &length, &unit);

	return css__copy_border_top_left_radius(
			type == CSS_BORDER_RADIUS_INHERIT ? parent : child,
			result);
}

/* ---- border-top_right-radius ---- */

css_error css__cascade_border_top_right_radius(uint32_t opv, css_style *style,
		css_select_state *state)
{
	return css__cascade_length(opv, style, state, set_border_top_right_radius);
}

css_error css__set_border_top_right_radius_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_border_top_right_radius(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_border_top_right_radius(css_select_state *state)
{
	return set_border_top_right_radius(state->computed, CSS_BORDER_RADIUS_SET,
			0, CSS_UNIT_PX);
}

css_error css__copy_border_top_right_radius(const css_computed_style *from,
		css_computed_style *to)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_border_top_right_radius(from, &length, &unit);

	if (from == to)
		return CSS_OK;
	return set_border_top_right_radius(to, type, length, unit);
}

css_error css__compose_border_top_right_radius(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_border_top_right_radius(child, &length, &unit);

	return css__copy_border_top_right_radius(
			type == CSS_BORDER_RADIUS_INHERIT ? parent : child,
			result);
}

/* ---- border-bottom_right-radius ---- */

css_error css__cascade_border_bottom_right_radius(uint32_t opv, css_style *style,
		css_select_state *state)
{
	return css__cascade_length(opv, style, state, set_border_bottom_right_radius);
}

css_error css__set_border_bottom_right_radius_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_border_bottom_right_radius(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_border_bottom_right_radius(css_select_state *state)
{
	return set_border_bottom_right_radius(state->computed, CSS_BORDER_RADIUS_SET,
			0, CSS_UNIT_PX);
}

css_error css__copy_border_bottom_right_radius(const css_computed_style *from,
		css_computed_style *to)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_border_bottom_right_radius(from, &length, &unit);

	if (from == to)
		return CSS_OK;
	return set_border_bottom_right_radius(to, type, length, unit);
}

css_error css__compose_border_bottom_right_radius(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_border_bottom_right_radius(child, &length, &unit);

	return css__copy_border_bottom_right_radius(
			type == CSS_BORDER_RADIUS_INHERIT ? parent : child,
			result);
}

/* ---- border-bottom_left-radius ---- */

css_error css__cascade_border_bottom_left_radius(uint32_t opv, css_style *style,
		css_select_state *state)
{
	return css__cascade_length(opv, style, state, set_border_bottom_left_radius);
}

css_error css__set_border_bottom_left_radius_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_border_bottom_left_radius(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_border_bottom_left_radius(css_select_state *state)
{
	return set_border_bottom_left_radius(state->computed, CSS_BORDER_RADIUS_SET,
			0, CSS_UNIT_PX);
}

css_error css__copy_border_bottom_left_radius(const css_computed_style *from,
		css_computed_style *to)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_border_bottom_left_radius(from, &length, &unit);

	if (from == to)
		return CSS_OK;
	return set_border_bottom_left_radius(to, type, length, unit);
}

css_error css__compose_border_bottom_left_radius(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_border_bottom_left_radius(child, &length, &unit);

	return css__copy_border_bottom_left_radius(
			type == CSS_BORDER_RADIUS_INHERIT ? parent : child,
			result);
}

/* ---- box-shadow ---- */

css_error css__cascade_box_shadow(uint32_t opv, css_style *style,
		css_select_state *state)
{
	uint16_t value = CSS_BOX_SHADOW_INHERIT;
	css_fixed len[4] = { 0, 0, 0, 0 };
	uint32_t unit[4] = { UNIT_PX, UNIT_PX, UNIT_PX, UNIT_PX };
	css_color color = 0;
	int i;

	if (hasFlagValue(opv) == false) {
		switch (getValue(opv)) {
		case BOX_SHADOW_NONE:
			value = CSS_BOX_SHADOW_NONE;
			break;
		case BOX_SHADOW_SET:
		case BOX_SHADOW_INSET:
			value = getValue(opv) == BOX_SHADOW_INSET ?
					CSS_BOX_SHADOW_INSET : CSS_BOX_SHADOW_SET;
			for (i = 0; i < 4; i++) {
				len[i] = *((css_fixed *) style->bytecode);
				advance_bytecode(style, sizeof(css_fixed));
				unit[i] = *((uint32_t *) style->bytecode);
				advance_bytecode(style, sizeof(uint32_t));
			}
			color = *((css_color *) style->bytecode);
			advance_bytecode(style, sizeof(css_color));
			break;
		}
	}
	for (i = 0; i < 4; i++)
		unit[i] = css__to_css_unit(unit[i]);

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		return set_box_shadow(state->computed, value,
				len[0], unit[0], len[1], unit[1],
				len[2], unit[2], len[3], unit[3], color);
	}
	return CSS_OK;
}

css_error css__set_box_shadow_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	(void) hint;
	return set_box_shadow(style, CSS_BOX_SHADOW_NONE, 0, CSS_UNIT_PX,
			0, CSS_UNIT_PX, 0, CSS_UNIT_PX, 0, CSS_UNIT_PX, 0);
}

css_error css__initial_box_shadow(css_select_state *state)
{
	return set_box_shadow(state->computed, CSS_BOX_SHADOW_NONE,
			0, CSS_UNIT_PX, 0, CSS_UNIT_PX, 0, CSS_UNIT_PX,
			0, CSS_UNIT_PX, 0);
}

css_error css__copy_box_shadow(const css_computed_style *from,
		css_computed_style *to)
{
	css_fixed l[4];
	css_unit u[4];
	css_color color;
	uint8_t type = get_box_shadow(from, &l[0], &u[0], &l[1], &u[1],
			&l[2], &u[2], &l[3], &u[3], &color);

	if (from == to)
		return CSS_OK;
	return set_box_shadow(to, type, l[0], u[0], l[1], u[1], l[2], u[2],
			l[3], u[3], color);
}

css_error css__compose_box_shadow(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed l[4];
	css_unit u[4];
	css_color color;
	uint8_t type = get_box_shadow(child, &l[0], &u[0], &l[1], &u[1],
			&l[2], &u[2], &l[3], &u[3], &color);

	return css__copy_box_shadow(
			type == CSS_BOX_SHADOW_INHERIT ? parent : child, result);
}

/* ---- grid_template_columns: el texto del valor ---- */

css_error css__cascade_grid_template_columns(uint32_t opv, css_style *style,
		css_select_state *state)
{
	/* (los mismos valores que background-image: nada / una cadena) */
	return css__cascade_uri_none(opv, style, state, set_grid_template_columns);
}

css_error css__set_grid_template_columns_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	css_error error = set_grid_template_columns(style, hint->status, hint->data.string);

	if (hint->data.string != NULL)
		lwc_string_unref(hint->data.string);
	return error;
}

css_error css__initial_grid_template_columns(css_select_state *state)
{
	return set_grid_template_columns(state->computed, CSS_GRID_TEMPLATE_COLUMNS_NONE, NULL);
}

css_error css__copy_grid_template_columns(const css_computed_style *from,
		css_computed_style *to)
{
	lwc_string *s;
	uint8_t type = get_grid_template_columns(from, &s);

	if (from == to)
		return CSS_OK;
	return set_grid_template_columns(to, type, s);
}

css_error css__compose_grid_template_columns(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	lwc_string *s;
	uint8_t type = get_grid_template_columns(child, &s);

	return css__copy_grid_template_columns(type == CSS_GRID_TEMPLATE_COLUMNS_INHERIT ? parent : child,
			result);
}

/* ---- grid_column: el texto del valor ---- */

css_error css__cascade_grid_column(uint32_t opv, css_style *style,
		css_select_state *state)
{
	/* (los mismos valores que background-image: nada / una cadena) */
	return css__cascade_uri_none(opv, style, state, set_grid_column);
}

css_error css__set_grid_column_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	css_error error = set_grid_column(style, hint->status, hint->data.string);

	if (hint->data.string != NULL)
		lwc_string_unref(hint->data.string);
	return error;
}

css_error css__initial_grid_column(css_select_state *state)
{
	return set_grid_column(state->computed, CSS_GRID_COLUMN_AUTO, NULL);
}

css_error css__copy_grid_column(const css_computed_style *from,
		css_computed_style *to)
{
	lwc_string *s;
	uint8_t type = get_grid_column(from, &s);

	if (from == to)
		return CSS_OK;
	return set_grid_column(to, type, s);
}

css_error css__compose_grid_column(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	lwc_string *s;
	uint8_t type = get_grid_column(child, &s);

	return css__copy_grid_column(type == CSS_GRID_COLUMN_INHERIT ? parent : child,
			result);
}
