// ---------------------------------------------------------------------
//
// Copyright (C) 2003 - 2020 by the deal.II authors
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



// same as find_cell_1, but for the alternative algorithm
// take a 2d mesh and check that we can find an arbitrary point's cell
// in it

#include <deal.II/fe/mapping_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/manifold_lib.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>

#include "../tests.h"



void
check(Triangulation<2> &tria)
{
  MappingQ<2> map(3); // Let's take a higher order mapping

  Point<2> p(1. / 3., 1. / 2. - 1e-10); // avoid ambiguity for hypercube mesh

  std::pair<Triangulation<2>::active_cell_iterator, Point<2>> cell =
    GridTools::find_active_cell_around_point(map, tria, p);

  deallog << cell.first << std::endl;
  for (const unsigned int v : GeometryInfo<2>::vertex_indices())
    deallog << '<' << cell.first->vertex(v) << "> ";
  deallog << "[ " << cell.second << "] ";
  deallog << std::endl;

  Assert(p.distance(cell.first->center()) < cell.first->diameter() / 2,
         ExcInternalError());
}


int
main()
{
  initlog();

  {
    Triangulation<2> coarse_grid;
    GridGenerator::hyper_cube(coarse_grid);
    coarse_grid.refine_global(2);
    check(coarse_grid);
  }

  {
    Triangulation<2> coarse_grid;
    GridGenerator::hyper_ball(coarse_grid);
    static const SphericalManifold<2> boundary;
    coarse_grid.set_manifold(0, boundary);
    coarse_grid.refine_global(2);
    check(coarse_grid);
  }
}
