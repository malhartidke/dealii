// ---------------------------------------------------------------------
//
// Copyright (C) 2016 - 2018 by the deal.II authors
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


// Test the combination of simple ChartManifolds: PolarManifold +
// Translation

#include "../tests.h"


// all include files you need here
#include <deal.II/grid/composition_manifold.h>
#include <deal.II/grid/manifold_lib.h>


int
main()
{
  initlog();
  std::ostream &out = deallog.get_file_stream();

  const int dim = 2, spacedim = 2;

  PolarManifold<1, 2>       F;
  FunctionManifold<2, 2, 2> G("x;y+1", "x;y-1");

  CompositionManifold<2, 2, 2, 2, 1> manifold(F, G);

  // Chart points.
  Point<2> cp[2];
  cp[0][0] = 1.0;
  cp[1][0] = 1.0;
  cp[1][1] = numbers::PI / 2;

  // Spacedim points
  std::vector<Point<spacedim>> sp(2);

  // Weights
  std::vector<double> w(2);

  sp[0] = manifold.push_forward(cp[0]);
  sp[1] = manifold.push_forward(cp[1]);

  for (unsigned int d = 0; d < 2; ++d)
    if (cp[d].distance(manifold.pull_back(sp[d])) > 1e-10)
      deallog << "Error!" << std::endl;

  unsigned int n_intermediates = 16;

  out << "set size ratio -1" << std::endl
      << "plot '-' with vectors " << std::endl;

  Point<2> center(0.0, 1.0);

  out << center << ' ' << sp[0] << std::endl
      << center << ' ' << sp[1] << std::endl;

  for (unsigned int i = 0; i < n_intermediates + 1; ++i)
    {
      w[0] = 1.0 - (double)i / ((double)n_intermediates);
      w[1] = 1.0 - w[0];

      Point<spacedim> ip =
        manifold.get_new_point(make_array_view(sp), make_array_view(w));
      Tensor<1, spacedim> t1 = manifold.get_tangent_vector(ip, sp[0]);
      Tensor<1, spacedim> t2 = manifold.get_tangent_vector(ip, sp[1]);

      out << ip << ' ' << t2 << std::endl;
    }

  out << 'e' << std::endl;

  return 0;
}
