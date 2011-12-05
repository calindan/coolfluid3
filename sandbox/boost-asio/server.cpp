// Copyright (C) 2010-2011 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <iostream>

#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "boost-asio/TCPConnection.hpp"

#include "common/CF.hpp" // for cf3::Uint

#include "common/XML/SignalFrame.hpp"

using namespace boost;
using namespace boost::asio::ip;

using namespace cf3::common::XML;

//////////////////////////////////////////////////////////////////////////////

class TCPServer
{
public:

  TCPServer(asio::io_service& io_service, int port)
    : m_acceptor(io_service, tcp::endpoint(tcp::v4(), port))
  {
    start_accept();
  }

private:

  void start_accept()
  {
    TCPConnection::Ptr new_connection = TCPConnection::create(m_acceptor.get_io_service());

    m_acceptor.async_accept( new_connection->socket(),
                             boost::bind( &TCPServer::handle_accept,
                                          this,
                                          new_connection,
                                          asio::placeholders::error));
  }

  void handle_accept( TCPConnection::Ptr new_connection, const system::error_code& error)
  {
    if (!error)
    {
      SignalFrame frame("message", "cpath:/", "cpath:/");

      std::cout << "New client connected from " << new_connection->socket().remote_endpoint() << std::endl;

      frame.set_option<std::string>("text", std::string("Welcome to the server!") );

      new_connection->send( frame,
                            boost::bind( &TCPServer::handle_send,
                                         this,
                                         asio::placeholders::error
                                        )
                           );
      start_accept();
    }
    else
      std::cerr << "Failed to open a network connection: " << error.message() << std::endl;
  }

  void handle_send( const system::error_code & error )
  {
    if( error )
      CFerror << error.message() << CFendl;
    else
      CFinfo << "Message sent" << CFendl;
  }

  tcp::acceptor m_acceptor;
};

//////////////////////////////////////////////////////////////////////////////

int main()
{
  asio::io_service io_service;

  try
  {
    TCPServer server(io_service, 7171);
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
