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

    Copyright (C) 2014 Fredrik Johansson

******************************************************************************/

#include "acb_poly.h"

/* TODO: move and document helper functions */

/* TODO: implement efficiently */
void
mag_sqrt(mag_t y, const mag_t x)
{
    if (mag_is_special(x))
    {
        mag_set(y, x);
    }
    else
    {
        arf_t t;
        arf_init(t);
        arf_set_mag(t, x);
        arf_sqrt(t, t, MAG_BITS, ARF_RND_UP);
        arf_get_mag(y, t);
        arf_clear(t);
    }
}

void
acb_get_mag(mag_t u, const acb_t z)
{
    if (arb_is_zero(acb_imagref(z)))
    {
        arb_get_mag(u, acb_realref(z));
    }
    else if (arb_is_zero(acb_realref(z)))
    {
        arb_get_mag(u, acb_imagref(z));
    }
    else
    {
        mag_t v;
        mag_init(v);

        arb_get_mag(u, acb_realref(z));
        arb_get_mag(v, acb_imagref(z));

        mag_mul(u, u, u);
        mag_addmul(u, v, v);
        mag_sqrt(u, u);

        mag_clear(v);
    }
}

void
mag_log_ui(mag_t t, ulong n)
{
    if (n <= 1)
    {
        if (n == 1)
            mag_zero(t);
        else
            mag_inf(t);
    }
    else
    {
        mag_set_ui(t, n-1);
        mag_log1p(t, t);
    }
}

/* TODO: needs reimplementing */
void mag_sub_lower(mag_t z, const mag_t x, const mag_t y);

long
arb_get_si_lower(const arb_t x)
{
    arf_t t;
    long v;

    arf_init(t);
    arf_set_mag(t, arb_radref(x));
    arf_sub(t, arb_midref(x), t, 2 * FLINT_BITS, ARF_RND_FLOOR);

    v = arf_get_si(t, ARF_RND_FLOOR);

    arf_clear(t);

    return v;
}

/* bound (1 + 1/m)^n, m > 0, n >= 0 */
void
mag_binpow_uiui(mag_t b, ulong m, ulong n)
{
    mag_t t;
    mag_init(t);

    /* bound by exp(n/m) <= 1 + (n/m) + (n/m)^2 */
    if (m > n)
    {
        mag_set_ui(t, n);   /* x = n/m */
        mag_div_ui(t, t, m);

        mag_mul(b, t, t);   /* x^2 */
        mag_add(b, b, t);   /* x */
        mag_one(t);
        mag_add(b, b, t);   /* 1 */
    }
    else
    {
        mag_one(b);
        mag_div_ui(b, b, m);
        mag_one(t);
        mag_add(t, t, b);
        mag_pow_ui(b, t, n);
    }

    mag_clear(t);
}

void
polylog_remainder_bound(mag_t u, const mag_t z, long sigma, ulong d, ulong N)
{
    mag_t TN, UN, t;

    if (N < 2)
    {
        mag_inf(u);
        return;
    }

    mag_init(TN);
    mag_init(UN);
    mag_init(t);

    if (mag_cmp_2exp_si(z, 0) >= 0)
    {
        mag_inf(u);
    }
    else
    {
        /* Bound T(N) */
        mag_pow_ui(TN, z, N);

        /* multiply by log(N)^d */
        if (d > 0)
        {
            mag_log_ui(t, N);
            mag_pow_ui(t, t, d);
            mag_mul(TN, TN, t);
        }

        /* multiply by 1/k^s */
        if (sigma > 0)
        {
            mag_set_ui_lower(t, N);
            mag_pow_ui_lower(t, t, sigma);
            mag_div(TN, TN, t);
        }
        else if (sigma < 0)
        {
            mag_set_ui(t, N);
            mag_pow_ui(t, t, -sigma);
            mag_mul(TN, TN, t);
        }

        /* Bound U(N) */
        mag_set(UN, z);

        /* multiply by (1 + 1/N)**S */
        if (sigma < 0)
        {
            mag_binpow_uiui(t, N, -sigma);
            mag_mul(UN, UN, t);
        }

        /* multiply by (1 + 1/(N log(N)))^d */
        if (d > 0)
        {
            ulong nl;

            /* rounds down */
            nl = mag_d_log_lower_bound(N) * N * (1 - 1e-13);

            mag_binpow_uiui(t, nl, d);
            mag_mul(UN, UN, t);
        }

        /* T(N) / (1 - U(N)) */
        if (mag_cmp_2exp_si(UN, 0) >= 0)
        {
            mag_inf(u);
        }
        else
        {
            mag_one(t);
            mag_sub_lower(t, t, UN);
            mag_div(u, TN, t);
        }
    }

    mag_clear(TN);
    mag_clear(UN);
    mag_clear(t);
}

long
polylog_choose_terms(mag_t err, long sigma, const mag_t z, long d, long prec)
{
    long N;

    for (N = 3; ; N = FLINT_MAX(N+3, N*1.1))
    {
        polylog_remainder_bound(err, z, sigma, d, N);

        /* TODO: do something else when |Li_s(z)| is very small/very large? */
        if (mag_cmp_2exp_si(err, -prec) < 0)
            break;

        if (N > 100 * prec)
        {
            N = 3;
            mag_inf(err);
            break;
        }
    }

    return N;
}

int
polylog_is_real(const acb_t s, const acb_t z)
{
    if (!arb_is_zero(acb_imagref(s)))
    {
        return 0;
    }
    else if (!arb_is_zero(acb_imagref(z)))
    {
        return 0;
    }
    else
    {
        fmpz_t one;
        int res;

        fmpz_init(one);
        fmpz_one(one);

        if (arb_contains_fmpz(acb_realref(z), one))
            res = 0;
        else
            res = (arf_cmp_2exp_si(arb_midref(acb_realref(z)), 0) < 0);

        fmpz_clear(one);

        return res;
    }
}

void
_acb_poly_polylog_cpx_zeta(acb_ptr w, const acb_t s, const acb_t z, long len, long prec)
{
    acb_ptr e1, e2, z1, z2, e1z1, e2z2;
    acb_t t, u, v;
    long k, len2;
    int deflate_zeta, deflate_gamma;

    if (!acb_is_finite(s) || !acb_is_finite(z))
    {
        _acb_vec_indeterminate(w, len);
        return;
    }

    acb_init(t);
    acb_init(u);
    acb_init(v);

    /* v = 1-s */
    acb_one(v);
    acb_sub(v, v, s, prec);

    /* pole of zeta */
    deflate_zeta = acb_is_one(v);

    /* poles of gamma at nonpositive integer v */
    deflate_gamma = (arb_is_zero(acb_imagref(v)) &&
            arb_is_int(acb_realref(v)) &&
            arf_sgn(arb_midref(acb_realref(v))) <= 0);

    len2 = len + deflate_gamma;

    e1 = _acb_vec_init(len + 1);
    e2 = _acb_vec_init(len + 1);
    z1 = _acb_vec_init(len + 1);
    z2 = _acb_vec_init(len + 1);
    e1z1 = _acb_vec_init(len + 1);
    e2z2 = _acb_vec_init(len + 1);

    /* u = log(-z)/(pi*i) */
    acb_neg(t, z);
    acb_log(t, t, prec);
    acb_const_pi(u, prec);
    acb_mul_onei(u, u);
    acb_div(u, t, u, prec);

    /* z1 = zeta(v+x, 1/2 + log(-z)/(2*pi*i)) */
    acb_one(t);
    acb_add(t, t, u, prec);
    acb_mul_2exp_si(t, t, -1);
    _acb_poly_zeta_cpx_series(z1, v, t, deflate_zeta, len2, prec);

    /* z2 = zeta(v+x, 1/2 - log(-z)/(2*pi*i)) */
    acb_one(t);
    acb_sub(t, t, u, prec);
    acb_mul_2exp_si(t, t, -1);
    _acb_poly_zeta_cpx_series(z2, v, t, deflate_zeta, len2, prec);

    /* e1 = (i/(2pi))^(v+x) */
    acb_onei(t);
    acb_const_pi(u, prec);
    acb_div(t, t, u, prec);
    acb_mul_2exp_si(t, t, -1);
    _acb_poly_acb_pow_cpx(e1, t, v, len + (deflate_zeta || deflate_gamma), prec);

    /* e2 = (1/(2 pi i))^(v+x) */
    acb_conj(t, t);
    _acb_poly_acb_pow_cpx(e2, t, v, len + (deflate_zeta || deflate_gamma), prec);

    _acb_poly_mullow(e1z1, e1, len2, z1, len2, len2, prec);
    _acb_poly_mullow(e2z2, e2, len2, z2, len2, len2, prec);
    _acb_vec_add(z1, e1z1, e2z2, len2, prec);

    if (deflate_gamma)
    {
        /* gamma(v+x) = pi/sin(pi(v+x)) * 1/gamma(1-v-x) */

        /* TODO: write a csc function? */
        acb_zero(e1);
        acb_const_pi(e1 + 1, prec);
        acb_mul_2exp_si(e2, v, -1);
        if (!arb_is_int(acb_realref(e2)))
            acb_neg(e1 + 1, e1 + 1);
        _acb_poly_sin_series(e2, e1, 2, len2, prec);
        _acb_poly_inv_series(e1, e2 + 1, len, len, prec);
        acb_const_pi(e2, prec);
        _acb_vec_scalar_mul(e1, e1, len, e2, prec);

        acb_set(z2, s);
        acb_set_si(z2 + 1, -1);
        _acb_poly_rgamma_series(e2, z2, 2, len, prec);
        _acb_poly_mullow(z2, e1, len, e2, len, len, prec);

        _acb_poly_mullow(w, z1 + 1, len, z2, len, len, prec);
    }
    else
    {
        if (deflate_zeta)
        {
            for (k = 0; k < len; k++)
            {
                arb_mul_2exp_si(acb_realref(e1 + k + 1), acb_realref(e1 + k + 1), 1);
                arb_add(acb_realref(z1 + k), acb_realref(z1 + k), acb_realref(e1 + k + 1), prec);
            }

        }

        /* gamma(v+x) */
        acb_set(e1, v);
        if (len > 1)
            acb_one(e1 + 1);
        _acb_poly_gamma_series(z2, e1, FLINT_MIN(len, 2), len, prec);

        _acb_poly_mullow(w, z2, len, z1, len, len, prec);
    }

    /* correct signs (from s -> 1-s) */
    for (k = 1; k < len; k += 2)
        acb_neg(w + k, w + k);

    _acb_vec_clear(e1, len + 1);
    _acb_vec_clear(e2, len + 1);
    _acb_vec_clear(z1, len + 1);
    _acb_vec_clear(z2, len + 1);
    _acb_vec_clear(e1z1, len + 1);
    _acb_vec_clear(e2z2, len + 1);

    acb_clear(t);
    acb_clear(u);
    acb_clear(v);
}

void
_acb_poly_polylog_cpx_small(acb_ptr w, const acb_t s, const acb_t z, long len, long prec)
{
    long k, N, sigma;
    int is_real;
    mag_t zmag, err, errf;
    acb_t a;

    acb_init(a);
    mag_init(zmag);
    mag_init(err);
    mag_init(errf);

    is_real = polylog_is_real(s, z);
    acb_get_mag(zmag, z);
    sigma = arb_get_si_lower(acb_realref(s));

    N = polylog_choose_terms(err, sigma, zmag, len - 1, prec);

    /* TODO: allow threading */
    acb_one(a);
    _acb_poly_powsum_series_naive(w, s, a, z, N - 1, len, prec);
    _acb_vec_scalar_mul(w, w, len, z, prec);

    for (k = 0; k < len; k++)
    {
        polylog_remainder_bound(err, zmag, sigma, k, N);
        mag_rfac_ui(errf, k);
        mag_mul(err, err, errf);

        if (is_real)
            arb_add_error_mag(acb_realref(w + k), err);
        else
            acb_add_error_mag(w + k, err);
    }

    acb_clear(a);
    mag_clear(zmag);
    mag_clear(err);
    mag_clear(errf);
}

void
_acb_poly_polylog_cpx(acb_ptr w, const acb_t s, const acb_t z, long len, long prec)
{
    mag_t zmag;

    mag_init(zmag);
    acb_get_mag(zmag, z);

    if (mag_cmp_2exp_si(zmag, -1) < 0)
        _acb_poly_polylog_cpx_small(w, s, z, len, prec);
    else
        _acb_poly_polylog_cpx_zeta(w, s, z, len, prec);

    mag_clear(zmag);
}

