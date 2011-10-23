// Copyright (C) 2010-2011 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE "Test module for benchmarking proto operators"

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/test/unit_test.hpp>

#include "common/Core.hpp"
#include "common/Root.hpp"
#include "common/Log.hpp"

#include "common/PE/all_reduce.hpp"
#include "common/PE/debug.hpp"
#include "common/PE/Comm.hpp"

#include "math/MatrixTypes.hpp"

#include "mesh/Domain.hpp"
#include "mesh/Mesh.hpp"
#include "mesh/MeshTransformer.hpp"
#include "mesh/Region.hpp"
#include "mesh/Elements.hpp"
#include "mesh/MeshWriter.hpp"
#include "mesh/ElementData.hpp"
#include "mesh/FieldManager.hpp"
#include "mesh/Geometry.hpp"

#include "mesh/Integrators/Gauss.hpp"
#include "mesh/LagrangeP0/Hexa.hpp"
#include "mesh/LagrangeP1/Hexa3D.hpp"

#include "mesh/BlockMesh/BlockData.hpp"

#include "physics/PhysModel.hpp"

#include "solver/CModel.hpp"
#include "solver/CSolver.hpp"
#include "solver/Tags.hpp"

#include "solver/Actions/CForAllElements.hpp"
#include "solver/Actions/CComputeVolume.hpp"

#include "solver/Actions/Proto/CProtoAction.hpp"
#include "solver/Actions/Proto/ElementLooper.hpp"
#include "solver/Actions/Proto/Expression.hpp"
#include "solver/Actions/Proto/Functions.hpp"
#include "solver/Actions/Proto/NodeLooper.hpp"
#include "solver/Actions/Proto/Terminals.hpp"

#include "Tools/MeshGeneration/MeshGeneration.hpp"
#include "Tools/Testing/ProfiledTestFixture.hpp"
#include "Tools/Testing/TimedTestFixture.hpp"

using namespace cf3;
using namespace cf3::solver;
using namespace cf3::solver::Actions;
using namespace cf3::solver::Actions::Proto;
using namespace cf3::mesh;
using namespace cf3::common;

////////////////////////////////////////////////////

struct ProtoParallelFixture :
  //public Tools::Testing::ProfiledTestFixture,
  public Tools::Testing::TimedTestFixture
{
  ProtoParallelFixture() :
    root(Core::instance().root()),
    length(12.),
    half_height(0.5),
    width(6.)
  {
    int argc = boost::unit_test::framework::master_test_suite().argc;
    char** argv = boost::unit_test::framework::master_test_suite().argv;
    cf3_assert(argc == 4);
    x_segs = boost::lexical_cast<Uint>(argv[1]);
    y_segs = boost::lexical_cast<Uint>(argv[2]);
    z_segs = boost::lexical_cast<Uint>(argv[3]);
  }

  // Setup a model under root
  CModel& setup(const std::string& model_name)
  {
    CModel& model = Core::instance().root().create_component<CModel>(model_name);
    physics::PhysModel& phys_model = model.create_physics("cf3.physics.DynamicModel");
    Domain& dom = model.create_domain("Domain");
    CSolver& solver = model.create_solver("cf3.solver.CSimpleSolver");

    Mesh& mesh = dom.create_component<Mesh>("mesh");
    Mesh& serial_block_mesh = dom.create_component<Mesh>("serial_block_mesh"); // temporary mesh used for paralellization

    const Real ratio = 0.1;

    BlockMesh::BlockData& blocks = dom.create_component<BlockMesh::BlockData>("blocks");
    Tools::MeshGeneration::create_channel_3d(blocks, length, half_height, width, x_segs, y_segs/2, z_segs, ratio);

    BlockMesh::BlockData& parallel_blocks = dom.create_component<BlockMesh::BlockData>("parallel_blocks");
    BlockMesh::partition_blocks(blocks, PE::Comm::instance().size(), XX, parallel_blocks);

    BlockMesh::build_mesh(parallel_blocks, mesh);

    // Set up variables
    phys_model.variable_manager().create_descriptor("variables", "CellVolume, CellRank");

    // Create field
    boost_foreach(Entities& elements, mesh.topology().elements_range())
    {
      elements.create_space("elems_P0","cf3.mesh.LagrangeP0."+elements.element_type().shape_name());
    }

    return model;
  }

  Root& root;
  const Real length;
  const Real half_height;
  const Real width;
  typedef boost::mpl::vector2<LagrangeP1::Hexa3D, LagrangeP0::Hexa> ElementsT;

  Uint x_segs;
  Uint y_segs;
  Uint z_segs;
};


BOOST_AUTO_TEST_SUITE( ProtoParallelSuite )

//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( Initialize )
{
  PE::Comm::instance().init(boost::unit_test::framework::master_test_suite().argc, boost::unit_test::framework::master_test_suite().argv);
  //common::PE::wait_for_debugger(1);
}

BOOST_FIXTURE_TEST_CASE( SetupNoOverlap, ProtoParallelFixture )
{
  const Real rank = static_cast<Real>(PE::Comm::instance().rank());

  CModel& model = setup("NoOverlap");
  Mesh& mesh = model.domain().get_child("mesh").as_type<Mesh>();
  FieldGroup& elems_P0 = mesh.create_field_group("elems_P0",FieldGroup::Basis::ELEMENT_BASED);
  model.solver().field_manager().create_field("variables", elems_P0);

  MeshTerm<0, ScalarField> V("CellVolume", "variables");
  MeshTerm<1, ScalarField> R("CellRank", "variables");

  model.solver()
  << create_proto_action
  (
    "ComputeVolumeAndRank",
    elements_expression
    (
      ElementsT(),
      group <<
      (
        V = volume,
        R = rank
      )
    )
  );

  std::vector<URI> root_regions;
  root_regions.push_back(mesh.topology().uri());
  model.solver().configure_option_recursively(solver::Tags::regions(), root_regions);
}

////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_CASE( SimulateNoOverlap, ProtoParallelFixture )
{
  root.get_child("NoOverlap").as_type<CModel>().simulate();
}

////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_CASE( SetupOverlap, ProtoParallelFixture )
{
  CModel& model = setup("Overlap");
  Mesh& mesh = model.domain().get_child("mesh").as_type<Mesh>();

  const Real rank = static_cast<Real>(PE::Comm::instance().rank());

  MeshTerm<0, ScalarField> V("CellVolume", "variables");
  MeshTerm<1, ScalarField> R("CellRank", "variables");

  model.solver()
  << create_proto_action
  (
    "ComputeVolumeAndRank",
    elements_expression
    (
      ElementsT(),
      group <<
      (
        V = volume,
        R = rank
      )
    )
  );

  std::vector<URI> root_regions;
  root_regions.push_back(mesh.topology().uri());
  model.solver().configure_option_recursively(solver::Tags::regions(), root_regions);
}

BOOST_FIXTURE_TEST_CASE( BuildGlobalConn, ProtoParallelFixture )
{
  CModel& model = root.get_child("Overlap").as_type<CModel>();
  Mesh& mesh = model.domain().get_child("mesh").as_type<Mesh>();

  MeshTransformer& global_conn = model.domain().create_component("GlobalConnectivity", "cf3.mesh.actions.GlobalConnectivity").as_type<MeshTransformer>();
  global_conn.transform(mesh);
}

BOOST_FIXTURE_TEST_CASE( GrowOverlap, ProtoParallelFixture )
{
  CModel& model = root.get_child("Overlap").as_type<CModel>();
  Mesh& mesh = model.domain().get_child("mesh").as_type<Mesh>();

  MeshTransformer& grow_overlap = model.domain().create_component("GrowOverlap", "cf3.mesh.actions.GrowOverlap").as_type<MeshTransformer>();
  grow_overlap.transform(mesh);
}

BOOST_FIXTURE_TEST_CASE( CreateOverlapFields, ProtoParallelFixture )
{
  CModel& model = root.get_child("Overlap").as_type<CModel>();
  Mesh& mesh = model.domain().get_child("mesh").as_type<Mesh>();

  FieldGroup& elems_P0 = mesh.create_field_group("elems_P0",FieldGroup::Basis::ELEMENT_BASED);
  model.solver().field_manager().create_field("variables", elems_P0);
}

////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_CASE( SimulateOverlap, ProtoParallelFixture )
{
  root.get_child("Overlap").as_type<CModel>().simulate();
}

////////////////////////////////////////////////////////////////////////////////

// Check the volume results
BOOST_FIXTURE_TEST_CASE( CheckResultNoOverlap, ProtoParallelFixture )
{
  MeshTerm<0, ScalarField> V("CellVolume", "variables");

  const Real wanted_volume = width*length*half_height*2.;

  Mesh& mesh = find_component_recursively_with_name<Mesh>(root.get_child("NoOverlap"), "mesh");
  std::cout << "Checking volume for mesh " << mesh.uri().path() << std::endl;
  Real vol_check = 0;
  for_each_element< ElementsT >(mesh.topology(), vol_check += V);

  if(PE::Comm::instance().is_active())
  {
    Real total_volume_check;
    PE::all_reduce(PE::Comm::instance().communicator(), PE::plus(), &vol_check, 1, &total_volume_check);
    BOOST_CHECK_CLOSE(total_volume_check, wanted_volume, 1e-6);
  }

  MeshWriter& writer = root.create_component("Writer", "cf3.mesh.VTKXML.Writer").as_type<MeshWriter>();
  std::vector<Field::Ptr> fields;
  fields.push_back(find_component_ptr_recursively_with_name<Field>(mesh, "variables"));
  writer.set_fields(fields);
  writer.write_from_to(mesh, URI("utest-proto-parallel_output-" + mesh.parent().parent().name() + ".pvtu"));
}

// Check the volume results
BOOST_FIXTURE_TEST_CASE( CheckResultOverlap, ProtoParallelFixture )
{
  const Uint nb_procs = PE::Comm::instance().size();
  MeshTerm<0, ScalarField> V("CellVolume", "variables");

  const Real wanted_volume = width*length*half_height*2.;
  std::cout << "wanted_volume: " << wanted_volume << ", nb_procs: " << nb_procs << ", x_segs: " << x_segs << std::endl;
  const Real wanted_volume_overlap = wanted_volume + (nb_procs-1)*2.*wanted_volume/x_segs;

  Mesh& mesh = find_component_recursively_with_name<Mesh>(root.get_child("Overlap"), "mesh");
  Real vol_check = 0;
  for_each_element< ElementsT >(mesh.topology(), vol_check += V);

  if(PE::Comm::instance().is_active())
  {
    Real total_volume_check;
    PE::all_reduce(PE::Comm::instance().communicator(), PE::plus(), &vol_check, 1, &total_volume_check);
    BOOST_CHECK_CLOSE(total_volume_check, wanted_volume_overlap, 1e-6);
  }

  MeshWriter& writer = root.create_component("Writer", "cf3.mesh.VTKXML.Writer").as_type<MeshWriter>();
  std::vector<Field::Ptr> fields;
  fields.push_back(find_component_ptr_recursively_with_name<Field>(mesh, "variables"));
  writer.set_fields(fields);
  writer.write_from_to(mesh, URI("utest-proto-parallel_output-" + mesh.parent().parent().name() + ".pvtu"));
}


////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END()

////////////////////////////////////////////////////////////////////////////////
