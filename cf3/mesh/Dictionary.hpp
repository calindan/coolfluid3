// Copyright (C) 2010-2011 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifndef cf3_mesh_Dictionary_hpp
#define cf3_mesh_Dictionary_hpp

#include <set>

#include "common/Table_fwd.hpp"
#include "common/EnumT.hpp"
#include "common/Component.hpp"

#include "mesh/LibMesh.hpp"

namespace cf3 {
namespace common {
  class Link;
  template <typename T> class List;
  template <typename T> class DynTable;
  namespace PE { class CommPattern; }
}
namespace math { class VariablesDescriptor; }
namespace mesh {

  class UnifiedData;
  class Mesh;
  class Field;
  class Region;
  class Elements;
  class Entities;
  class Space;

////////////////////////////////////////////////////////////////////////////////

/// Component that holds Fields of the same type (topology and space)
/// @author Willem Deconinck
class Mesh_API Dictionary : public common::Component {

public: // typedefs




  class Mesh_API Basis
  {
  public:

    /// Enumeration of the Shapes recognized in CF
    enum Type { INVALID=-1, POINT_BASED=0,  ELEMENT_BASED=1, CELL_BASED=2, FACE_BASED=3 };

    typedef common::EnumT< Basis > ConverterBase;

    struct Mesh_API Convert : public ConverterBase
    {
      /// constructor where all the converting maps are built
      Convert();
      /// get the unique instance of the converter class
      static Convert& instance();
    };

    static std::string to_str(Type type)
    {
      return Convert::instance().to_str(type);
    }

    static Type to_enum(const std::string& type)
    {
      return Convert::instance().to_enum(type);
    }

  };

public: // functions

  /// Contructor
  /// @param name of the component
  Dictionary ( const std::string& name );

  /// Virtual destructor
  virtual ~Dictionary();

  /// Get the class name
  static std::string type_name () { return "Dictionary"; }

  /// Create a new field in this group
  Field& create_field( const std::string& name, const std::string& variables_description = "scalar_same_name");

  /// Create a new field in this group
  Field& create_field( const std::string& name, math::VariablesDescriptor& variables_descriptor);

  /// Number of rows of contained fields
  virtual Uint size() const { return m_size; }

  /// Resize the contained fields
  void resize(const Uint size);

  /// Return the space of given entities
  const Space& space(const Entities& entities) const;

  /// Return the space of given entities
  const Handle< Space const>& space(const Handle< Entities const>& entities) const;

  /// Return the global index of every field row
  common::List<Uint>& glb_idx() const { return *m_glb_idx; }

  /// Return the rank of every field row
  common::List<Uint>& rank() const { return *m_rank; }

  /// Return the comm pattern valid for this field group. Created based on the glb_idx and rank if it didn't exist already
  common::PE::CommPattern& comm_pattern();

  /// Check if a field row is owned by this rank
  bool is_ghost(const Uint idx) const;

  /// @brief Check if all fields are compatible
  bool check_sanity(std::vector<std::string>& messages) const;
  bool check_sanity() const;

  std::vector<Handle< Entities > > entities_range();

  Field& field(const std::string& name);

//  UnifiedData& elements_lookup() const { return *m_elements_lookup; }

  void create_connectivity_in_space();

// deprecated
//  common::TableConstRow<Uint>::type indexes_for_element(const Entities& elements, const Uint idx) const;
// deprecated
//  common::TableConstRow<Uint>::type indexes_for_element(const Uint unified_element_idx) const;

  const Field& coordinates() const;

  Field& coordinates();

  common::DynTable<Uint>& glb_elem_connectivity();

  Basis::Type basis() const { return m_basis; }

  void signal_create_field ( common::SignalArgs& node );

  void signature_create_field ( common::SignalArgs& node);

  bool defined_for_entities(const Handle<Entities const>& entities) const;

  void add_space(const Handle<Space>& space);
  void update();

private: // functions

  bool has_coordinates() const;

  Field& create_coordinates();

  void config_space();

  void config_topology();

  void config_regions();

  void config_type();

  /// Triggered when the event mesh_changed
  void on_mesh_changed_event( common::SignalArgs& args );

protected:

  Basis::Type m_basis;

  Uint m_size;

  Handle<common::Link> m_topology;
  Handle<common::List<Uint> > m_glb_idx;
  Handle<common::List<Uint> > m_rank;
  Handle<UnifiedData> m_elements_lookup;
  Handle<Field> m_coordinates;
  Handle<common::DynTable<Uint> > m_glb_elem_connectivity;
  Handle<common::PE::CommPattern> m_comm_pattern;

private:

  std::map< Handle<Entities const> , Handle<Space const> > m_spaces_map;
  std::vector< Handle<Space   > > m_spaces;
  std::vector< Handle<Entities> > m_entities;
};

////////////////////////////////////////////////////////////////////////////////

} // mesh
} // cf3

#endif // cf3_mesh_Dictionary_hpp