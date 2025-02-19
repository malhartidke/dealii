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


// Test interior mapping of flat manifold, where the
// flat manifold is implemented as a ChartManifold with identity
// pull-back and push-forward
//
// make the chart higher dimensional

#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_values.h>

#include "../tests.h"


// all include files you need here
#include <deal.II/base/quadrature_lib.h>

#include <deal.II/fe/mapping_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/manifold_lib.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>


template <int dim, int spacedim>
class MyFlatManifold : public ChartManifold<dim, spacedim, spacedim + 1>
{
public:
  std::unique_ptr<Manifold<dim, spacedim>>
  clone() const override
  {
    return std::unique_ptr<Manifold<dim, spacedim>>(new MyFlatManifold());
  }


  virtual Point<spacedim + 1>
  pull_back(const Point<spacedim> &space_point) const override
  {
    Point<spacedim + 1> p;
    for (unsigned int d = 0; d < spacedim; ++d)
      p[d] = space_point[d];
    return p;
  }


  virtual Point<spacedim>
  push_forward(const Point<spacedim + 1> &chart_point) const override
  {
    Point<spacedim> p;
    for (unsigned int d = 0; d < spacedim; ++d)
      p[d] = chart_point[d];
    return p;
  }

  virtual DerivativeForm<1, spacedim + 1, spacedim>
  push_forward_gradient(const Point<spacedim + 1> &chart_point) const override
  {
    DerivativeForm<1, spacedim + 1, spacedim> x;
    for (unsigned int d = 0; d < spacedim; ++d)
      x[d][d] = 1;
    return x;
  }
};


// Helper function
template <int dim, int spacedim>
void
test(unsigned int ref, const MappingQ<dim> &mapping)
{
  deallog << "Testing dim=" << dim << ", spacedim=" << spacedim << std::endl;

  MyFlatManifold<dim, spacedim> flat_manifold;
  Triangulation<dim, spacedim>  tria;
  tria.set_manifold(0, flat_manifold);
  GridGenerator::hyper_cube(tria);
  tria.refine_global(ref);

  typename Triangulation<dim, spacedim>::active_cell_iterator cell;

  QGauss<dim> quadrature(4);
  FE_Q<dim>   fe(2);

  FEValues<dim> fe_values(mapping,
                          fe,
                          quadrature,
                          update_gradients | update_values |
                            update_quadrature_points | update_JxW_values);

  for (cell = tria.begin_active(); cell != tria.end(); ++cell)
    {
      cell->set_all_manifold_ids(0);

      // check that FlatManifold returns the middle of the cell.
      deallog << "Cell: " << cell << std::endl;

      fe_values.reinit(cell);

      deallog << "  center: " << cell->center() << std::endl;
      for (unsigned int q = 0; q < quadrature.size(); ++q)
        {
          deallog << "  JxW(" << q << "): " << fe_values.JxW(q) << std::endl;
          deallog << "  p(" << q << "): " << fe_values.quadrature_point(q)
                  << std::endl;
          for (unsigned int i = 0; i < fe_values.dofs_per_cell; ++i)
            deallog << "  shape " << i << ',' << q << ": "
                    << fe_values.shape_value(i, q) << ' '
                    << fe_values.shape_grad(i, q) << std::endl;
        }

      if (cell->get_manifold().get_new_point_on_cell(cell).distance(
            cell->center()) > 1e-6)
        {
          deallog << "Default manifold: "
                  << cell->get_manifold().get_new_point_on_cell(cell)
                  << std::endl;
          deallog << "Center of cell  : " << cell->center() << std::endl;
        }
    }

  deallog << "OK" << std::endl << std::endl;
}

int
main()
{
  initlog();

  test<2, 2>(2, MappingQ<2>(4));


  return 0;
}
