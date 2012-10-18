/*=============================================================================

    This file is part of ARB.

    ARB is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    ARB is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ARB; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

=============================================================================*/
/******************************************************************************

    Copyright (C) 2012 Fredrik Johansson

******************************************************************************/

#include "fmprb_poly.h"

void
_fmprb_poly_interpolation_weights(fmprb_struct * w,
    fmprb_struct ** tree, long len, long prec)
{
    fmprb_struct * tmp;
    long i, n, height;

    if (len == 0)
        return;

    if (len == 1)
    {
        fmprb_one(w);
        return;
    }

    tmp = _fmprb_vec_init(len + 1);
    height = FLINT_CLOG2(len);
    n = 1L << (height - 1);

    _fmprb_poly_mul_monic(tmp, tree[height-1], n + 1,
                        tree[height-1] + (n + 1), (len - n + 1), prec);

    _fmprb_poly_derivative(tmp, tmp, len + 1, prec);
    _fmprb_poly_evaluate_vec_fast_precomp(w, tmp, len, tree, len, prec);

    for (i = 0; i < len; i++)
        fmprb_ui_div(w + i, 1UL, w + i, prec);

    _fmprb_vec_clear(tmp, len + 1);
}

void
_fmprb_poly_interpolate_fast_precomp(fmprb_struct * poly,
    const fmprb_struct * ys, fmprb_struct ** tree, const fmprb_struct * weights,
    long len, long prec)
{
    fmprb_struct *t, *u, *pa, *pb;
    long i, pow, left;

    if (len == 0)
        return;

    t = _fmprb_vec_init(len);
    u = _fmprb_vec_init(len);

    for (i = 0; i < len; i++)
        fmprb_mul(poly + i, weights + i, ys + i, prec);

    for (i = 0; i < FLINT_CLOG2(len); i++)
    {
        pow = (1L << i);
        pa = tree[i];
        pb = poly;
        left = len;

        while (left >= 2 * pow)
        {
            _fmprb_poly_mul(t, pa, pow + 1, pb + pow, pow, prec);
            _fmprb_poly_mul(u, pa + pow + 1, pow + 1, pb, pow, prec);
            _fmprb_vec_add(pb, t, u, 2 * pow, prec);

            left -= 2 * pow;
            pa += 2 * pow + 2;
            pb += 2 * pow;
        }

        if (left > pow)
        {
            _fmprb_poly_mul(t, pa, pow + 1, pb + pow, left - pow, prec);
            _fmprb_poly_mul(u, pb, pow, pa + pow + 1, left - pow + 1, prec);
            _fmprb_vec_add(pb, t, u, left, prec);
        }
    }

    _fmprb_vec_clear(t, len);
    _fmprb_vec_clear(u, len);
}

void
_fmprb_poly_interpolate_fast(fmprb_struct * poly,
    const fmprb_struct * xs, const fmprb_struct * ys, long len, long prec)
{
    fmprb_struct ** tree;
    fmprb_struct *w;

    tree = _fmprb_poly_tree_alloc(len);
    _fmprb_poly_tree_build(tree, xs, len, prec);

    w = _fmprb_vec_init(len);
    _fmprb_poly_interpolation_weights(w, tree, len, prec);

    _fmprb_poly_interpolate_fast_precomp(poly, ys, tree, w, len, prec);

    _fmprb_vec_clear(w, len);
    _fmprb_poly_tree_free(tree, len);
}

void
fmprb_poly_interpolate_fast(fmprb_poly_t poly,
        const fmprb_struct * xs, const fmprb_struct * ys, long n, long prec)
{
    if (n == 0)
    {
        fmprb_poly_zero(poly);
    }
    else
    {
        fmprb_poly_fit_length(poly, n);
        _fmprb_poly_set_length(poly, n);
        _fmprb_poly_interpolate_fast(poly->coeffs, xs, ys, n, prec);
        _fmprb_poly_normalise(poly);
    }
}