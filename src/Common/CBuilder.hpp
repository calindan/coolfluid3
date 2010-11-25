// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.


#ifndef CF_Common_CBuilder_hpp
#define CF_Common_CBuilder_hpp

/////////////////////////////////////////////////////////////////////////////////

#include "Common/CFactories.hpp"
#include "Common/CRoot.hpp"
#include "Common/CLink.hpp"
#include "Common/Core.hpp"
#include "Common/CLibrary.hpp"
#include "Common/CLibraries.hpp"

#include "Common/Log.hpp" // temporary

/////////////////////////////////////////////////////////////////////////////////

namespace CF {
namespace Common {

/////////////////////////////////////////////////////////////////////////////////

/// @brief Component that builds other components
/// @author Tiago Quintino
class CBuilder : public Component
{
public:

  typedef boost::shared_ptr< CBuilder > Ptr;
  typedef boost::shared_ptr< CBuilder const> ConstPtr;

  /// @brief Contructor
  /// @param name of component
  CBuilder(const std::string& name) : Component(name)
  {
    BuildComponent<none>().build(this);
  }

  /// @brief Virtual destructor.
  virtual ~CBuilder() {}

  /// @returns the class name
  static std::string type_name() { return  "CBuilder"; }

  /// Returns the name of the type of what abstract type it builds.
  /// Should match the name of the CFactory holding the builder.
  /// @return name of type
  virtual std::string builder_abstract_type_name() const = 0;

  /// @return the name of the type of what concrete type it builds
  virtual std::string builder_concrete_type_name() const = 0;

  /// @return the name of the type of what concrete type it builds
  virtual Component::Ptr build ( const std::string& name ) const = 0;

}; // CBuilder

/////////////////////////////////////////////////////////////////////////////////

/// @brief Component that builds other components of a given abstract type
/// This is the actual builder for one concrete type.
/// @author Tiago Quintino
template < typename BASE, typename CONCRETE >
class CBuilderT : public CBuilder
{
public:

  typedef boost::shared_ptr< CBuilderT<BASE, CONCRETE> > Ptr;
  typedef boost::shared_ptr< CBuilderT<BASE, CONCRETE> const> ConstPtr;

  /// @brief Contructor
  /// @param name of component
  CBuilderT(const std::string& name) : CBuilder(name)
  {
    BuildComponent<no_props>().build(this);
  }

  /// @brief Virtual destructor.
  virtual ~CBuilderT() {};

  /// @returns the class name
  static std::string type_name() { return "CBuilderT<" + CONCRETE::type_name() + ">"; }

  /// builds the component cast to the correct base
  typename BASE::Ptr build_component_typed ( const std::string& name ) const
  {
    return typename BASE::Ptr ( new CONCRETE(name), Deleter<BASE>() );
  }

  /// Returns the name of the type of what abstract type it builds.
  /// Should match the name of the CFactory holding the builder.
  /// @return name of type
  virtual std::string builder_abstract_type_name() const { return  BASE::type_name(); }

  /// @return the name of the type of this factory
  virtual std::string builder_concrete_type_name() const { return CONCRETE::type_name(); }

  virtual Component::Ptr build ( const std::string& name ) const
  {
    return this->build_component_typed(name);
  }

  /// @name SIGNALS
  //@{

  /// creates a component from this component
  void build_component ( XmlNode& xml )
  {
    XmlParams params (xml);
    typename BASE::Ptr comp = this->build_component_typed ( params.get_option<std::string>("Component name") );
    CPath parent_path ( params.get_option<URI>("Parent component") );
    Component::Ptr parent = this->look_component( parent_path );
    parent->add_component( comp );
  }

  //@} END SIGNALS

  /// regists all the signals declared in this class
  virtual void regist_signals ( Component* self )
  {
      this->regist_signal ( "build_component" , "builds a component", "Build component" )->connect ( boost::bind ( &CBuilderT<BASE,CONCRETE>::build_component, self, _1 ) );
      this->signal("build_component").m_signature.template insert<std::string>("Component name", "Name for created component" )
                                                 .template insert<URI>("Parent component", "Path to component where place the newly built component");
  }

}; // CBuilderT

/////////////////////////////////////////////////////////////////////////////////

/// @brief Helper class to create the CBuilder and place it in the factory
/// @author Tiago Quintino
template <class CONCRETE, class BASE, class LIB >
struct ComponentBuilder
{
  /// @brief creates the CBuilder and places it into the correct factory
  ComponentBuilder( const std::string& name = std::string( LIB::library_namespace() + "." + CONCRETE::type_name()) )
  {
    // regist the concrete type in TypeInfo
    CF::TypeInfo::instance().regist<CONCRETE>( CONCRETE::type_name() );
    CF::TypeInfo::instance().regist< CBuilderT<BASE,CONCRETE> >(  CBuilderT<BASE,CONCRETE>::type_name() );

    // put builder in correct factory
    Common::CFactories::Ptr factories = Common::Core::instance().root()->get_child_type< CFactories >("Factories");
    Common::CFactory::Ptr   factory = factories->get_factory< BASE >();
    CBuilder::Ptr builder = factory->create_component_type< Common::CBuilderT<BASE,CONCRETE> >( name );

    // put a CLink to the builder in the respective CLibrary
    CLibrary::Ptr lib = Core::instance().libraries()->get_library<LIB>();
    CLink::Ptr liblink = lib->create_component_type<CLink>( name );
    liblink->link_to( builder );

    // give some info
    CFinfo << "lib [" << LIB::type_name() << "] : factory of \'" << BASE::type_name() << "\' registering builder of \'" << CONCRETE::type_name() << "\' with name \'" << name << "\'" << CFendl;
  }

}; // ComponentBuilder

/////////////////////////////////////////////////////////////////////////////////

} // Common
} // CF

/////////////////////////////////////////////////////////////////////////////////

#endif // CF_Common_CBuilder_hpp

