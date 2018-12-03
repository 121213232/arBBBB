/*
    Copyright (C) 2018 Fredrik Johansson

    This file is part of Arb.

    Arb is free software: you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License (LGPL) as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.  See <http://www.gnu.org/licenses/>.
*/

#include "acb_mat.h"

int main()
{
    slong iter;
    flint_rand_t state;

    flint_printf("eig_multiple_rump....");
    fflush(stdout);

    flint_randinit(state);

    for (iter = 0; iter < 2000 * arb_test_multiplier(); iter++)
    {
        acb_mat_t A, R;
        acb_ptr E, F;
        acb_t b;
        slong i, j, n, prec, count, count2;
        int result;

        n = n_randint(state, 8);
        prec = 2 + n_randint(state, 200);

        acb_init(b);
        acb_mat_init(A, n, n);
        acb_mat_init(R, n, n);
        E = _acb_vec_init(n);
        F = _acb_vec_init(n);

        if (n_randint(state, 10) != 0)
        {
            for (i = 0; i < n; i++)
                acb_randtest(E + i, state, prec, 2);
        }
        else
        {
            /* Randomly repeat eigenvalues. */
            for (i = 0; i < n; i++)
            {
                if (i == 0 || n_randint(state, 2))
                    acb_randtest(E + i, state, prec, 2);
                else
                    acb_set(E + i, E + n_randint(state, i));
            }
        }

        if (n_randint(state, 2))
        {
            for (i = 0; i < n; i++)
                acb_get_mid(E + i, E + i);
        }

        acb_mat_randtest_eig(A, state, E, prec);
        acb_mat_approx_eig_qr(F, NULL, R, A, NULL, 0, prec);

        /* Perturb F further. */
        if (n_randint(state, 10) == 0)
        {
            for (i = 0; i < n; i++)
            {
                acb_randtest(b, state, prec, 1);
                acb_mul_2exp_si(b, b, -n_randint(state, prec));
                acb_add(F + i, F + i, b, prec);
            }
        }

        /* Perturb R further. */
        if (n_randint(state, 10) == 0)
        {
            j = n_randint(state, n);

            for (i = 0; i < n; i++)
            {
                acb_randtest(b, state, prec, 1);
                acb_mul_2exp_si(b, b, -10 - n_randint(state, prec));
                acb_add(acb_mat_entry(R, i, j), acb_mat_entry(R, i, j), b, prec);
            }
        }

        result = acb_mat_eig_multiple_rump(F, A, E, R, prec);

        if (result)
        {
            count = 0;
            count2 = 0;

            for (i = 0; i < n; i++)
            {
                for (j = 0; j < n; j++)
                {
                    if (j == 0 || !acb_equal(F + j, F + j - 1))
                        count += acb_contains(F + j, E + i);
                }

                for (j = 0; j < n; j++)
                {
                    if (j == 0 || !acb_equal(F + j, F + j - 1))
                        count2 += acb_overlaps(F + j, E + i);
                }
            }

            if (count != n || count2 != n)
            {
                flint_printf("FAIL: count\n\n");
                flint_printf("A = \n"); acb_mat_printd(A, 20); flint_printf("\n\n");
                flint_printf("R = \n"); acb_mat_printd(R, 20); flint_printf("\n\n");
                flint_printf("count = %wd, count2 = %wd\n\n", count, count2);
                flint_printf("E = \n");
                for (j = 0; j < n; j++)
                {
                    acb_printd(E + j, 20);
                    flint_printf("\n");
                }
                flint_printf("F = \n");
                for (j = 0; j < n; j++)
                {
                    acb_printd(F + j, 20);
                    flint_printf("\n");
                }
                flint_abort();
            }
        }

        acb_mat_clear(A);
        acb_mat_clear(R);
        _acb_vec_clear(E, n);
        _acb_vec_clear(F, n);
        acb_clear(b);
    }

    flint_randclear(state);
    flint_cleanup();
    flint_printf("PASS\n");
    return EXIT_SUCCESS;
}
