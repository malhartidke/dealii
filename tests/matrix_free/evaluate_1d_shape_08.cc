// ---------------------------------------------------------------------
//
// Copyright (C) 2018 - 2020 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------



// check the correctness of the 1d evaluation functions used in FEEvaluation,
// path evaluate_evenodd, when using a double array for coefficients but
// VectorizedArray for the input and output vector

#include <deal.II/base/vectorization.h>

#include <deal.II/matrix_free/tensor_product_kernels.h>

#include <iostream>

#include "../tests.h"


template <int M, int N, int type, bool add>
void
test()
{
  deallog << "Test " << M << " x " << N << std::endl;
  double shape[M][N];
  for (unsigned int i = 0; i < (M + 1) / 2; ++i)
    for (unsigned int j = 0; j < N; ++j)
      {
        shape[i][j] = -1. + 2. * random_value<double>();
        if (type == 1)
          shape[M - 1 - i][N - 1 - j] = -shape[i][j];
        else
          shape[M - 1 - i][N - 1 - j] = shape[i][j];
      }
  if (type == 0 && M % 2 == 1 && N % 2 == 1)
    {
      for (unsigned int i = 0; i < M; ++i)
        shape[i][N / 2] = 0.;
      shape[M / 2][N / 2] = 0.9;
    }
  if (type == 1 && M % 2 == 1 && N % 2 == 1)
    shape[M / 2][N / 2] = 0.;

  // create symmetrized shape array exactly as expected by the evenodd
  // function
  AlignedVector<double> shape_sym(M * ((N + 1) / 2));
  for (unsigned int i = 0; i < M / 2; ++i)
    for (unsigned int q = 0; q < (N + 1) / 2; ++q)
      {
        shape_sym[i * ((N + 1) / 2) + q] =
          0.5 * (shape[i][q] + shape[i][N - 1 - q]);
        shape_sym[(M - 1 - i) * ((N + 1) / 2) + q] =
          0.5 * (shape[i][q] - shape[i][N - 1 - q]);
      }
  if (M % 2 == 1)
    for (unsigned int q = 0; q < (N + 1) / 2; ++q)
      shape_sym[(M - 1) / 2 * ((N + 1) / 2) + q] = shape[(M - 1) / 2][q];

  VectorizedArray<double> x[N], x_ref[N], y[M], y_ref[M];
  for (unsigned int i = 0; i < N; ++i)
    for (unsigned int v = 0; v < VectorizedArray<double>::size(); ++v)
      x[i][v] = random_value<double>();

  // compute reference
  for (unsigned int i = 0; i < M; ++i)
    {
      y[i]     = 1.;
      y_ref[i] = add ? y[i] : VectorizedArray<double>();
      for (unsigned int j = 0; j < N; ++j)
        y_ref[i] += shape[i][j] * x[j];
    }

  // apply function for tensor product
  internal::EvaluatorTensorProduct<internal::evaluate_evenodd,
                                   1,
                                   M,
                                   N,
                                   VectorizedArray<double>,
                                   double>
    evaluator(shape_sym, shape_sym, shape_sym);
  if (type == 0)
    evaluator.template values<0, false, add>(x, y);
  if (type == 1)
    evaluator.template gradients<0, false, add>(x, y);
  if (type == 2)
    evaluator.template hessians<0, false, add>(x, y);

  deallog << "Errors no transpose: ";
  for (unsigned int i = 0; i < M; ++i)
    {
      deallog << y[i][0] - y_ref[i][0] << ' ';
      for (unsigned int v = 1; v < VectorizedArray<double>::size(); ++v)
        AssertThrow(std::abs(y[i][v] - y_ref[i][v]) < 1e-12,
                    ExcInternalError());
    }
  deallog << std::endl;

  for (unsigned int i = 0; i < M; ++i)
    for (unsigned int v = 0; v < VectorizedArray<double>::size(); ++v)
      y[i][v] = random_value<double>();

  // compute reference
  for (unsigned int i = 0; i < N; ++i)
    {
      x[i]     = 2.;
      x_ref[i] = add ? x[i] : VectorizedArray<double>();
      for (unsigned int j = 0; j < M; ++j)
        x_ref[i] += shape[j][i] * y[j];
    }

  // apply function for tensor product
  if (type == 0)
    evaluator.template values<0, true, add>(y, x);
  if (type == 1)
    evaluator.template gradients<0, true, add>(y, x);
  if (type == 2)
    evaluator.template hessians<0, true, add>(y, x);

  deallog << "Errors transpose:    ";
  for (unsigned int i = 0; i < N; ++i)
    {
      deallog << x[i][0] - x_ref[i][0] << ' ';
      for (unsigned int v = 1; v < VectorizedArray<double>::size(); ++v)
        AssertThrow(std::abs(x[i][v] - x_ref[i][v]) < 1e-12,
                    ExcInternalError());
    }
  deallog << std::endl;
}

int
main()
{
  initlog();

  deallog.push("values");
  test<4, 4, 0, false>();
  test<3, 3, 0, false>();
  test<4, 3, 0, false>();
  test<3, 4, 0, false>();
  test<3, 5, 0, false>();
  deallog.pop();

  deallog.push("gradients");
  test<4, 4, 1, false>();
  test<3, 3, 1, false>();
  test<4, 3, 1, false>();
  test<3, 4, 1, false>();
  test<3, 5, 1, false>();
  deallog.pop();

  deallog.push("hessians");
  test<4, 4, 2, false>();
  test<3, 3, 2, false>();
  test<4, 3, 2, false>();
  test<3, 4, 2, false>();
  test<3, 5, 2, false>();
  deallog.pop();

  deallog.push("add");

  deallog.push("values");
  test<4, 4, 0, true>();
  test<3, 3, 0, true>();
  test<4, 3, 0, true>();
  test<3, 4, 0, true>();
  test<3, 5, 0, true>();
  deallog.pop();

  deallog.push("gradients");
  test<4, 4, 1, true>();
  test<3, 3, 1, true>();
  test<4, 3, 1, true>();
  test<3, 4, 1, true>();
  test<3, 5, 1, true>();
  deallog.pop();

  deallog.push("hessians");
  test<4, 4, 2, true>();
  test<3, 3, 2, true>();
  test<4, 3, 2, true>();
  test<3, 4, 2, true>();
  test<3, 5, 2, true>();
  deallog.pop();

  deallog.pop();

  return 0;
}
