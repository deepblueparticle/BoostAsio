
#include <string>
#include <iostream>

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "common.hh"
#include "TcpServerMT.hh"

using namespace boost::asio::ip;
using namespace std;

//
// TcpSession
//

TcpSession::TcpSession(boost::asio::io_service &io_service, int sessionId) :
    socket_(io_service),
    sessionId_(sessionId)
{
}

TcpSession::~TcpSession()
{
  cout << "Destroying TcpSession[" << sessionId_ << "] with: " << socket_.remote_endpoint() << endl;
}

// static
TcpSession::shared_pointer TcpSession::create(boost::asio::io_service& io_service, int sessionId)
{
  return shared_pointer(new TcpSession(io_service, sessionId));
}

void TcpSession::start()
{
  // A connection was accepted, now kick-off a read
  socket_.async_read_some(boost::asio::buffer(data_, maxLength),
      boost::bind(&TcpSession::handleRead, shared_from_this(),
                  boost::asio::placeholders::error,
                  boost::asio::placeholders::bytes_transferred));
}

void TcpSession::handleRead(const boost::system::error_code& error, size_t bytes_transferred)
{
  // A read was performed, now prepare the response and kick-off a write to send it

  if(error == boost::asio::error::eof)
  {
    cout << "TcpSession::handleRead[" << sessionId_ << "] Client disconnected from server" << endl;
    // connection closed cleanly by peer
    // Returning will cause this TcpSession to be destroyed by the shared_ptr
    // since noone else has a reference to it
    return;
  }

  cout << "[" << boost::this_thread::get_id() << "] "
       << "TcpSession::handleRead[" << sessionId_ << "] errorCode " << error << " bytes read " << bytes_transferred << endl;

  if(!error)
  {
    message_.clear();
    string request(data_, bytes_transferred);
    cout << "\trequest:  " << request << endl;
    reverseWords(request, message_);
    boost::asio::async_write(socket_, boost::asio::buffer(message_.c_str(), bytes_transferred),
        boost::bind(&TcpSession::handleWrite,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  }
  else
  {
    throw boost::system::system_error(error);
  }
}

void TcpSession::handleWrite(const boost::system::error_code& error, size_t bytes_transferred)
{
  // A write was performed, now kick-off another read
  cout << "[" << boost::this_thread::get_id() << "] "
       << "TcpSession::handleWrite[" << sessionId_ << "] errorCode " << error << " bytes written " << bytes_transferred << endl;

  if (!error)
  {
    cout << "\tresponse: " << message_ << endl;
    socket_.async_read_some(boost::asio::buffer(data_, maxLength),
        boost::bind(&TcpSession::handleRead, shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  }
  else
  {
    throw boost::system::system_error(error);
  }
}

//
// TcpServerMT
//

TcpServerMT::TcpServerMT(boost::asio::io_service& io_service_acceptor,
                         boost::asio::io_service& io_service_comm,
                         uint16_t tcpListenPort) :
      io_service_comm_(io_service_comm),
      acceptor_(io_service_acceptor, tcp::endpoint(tcp::v4(), tcpListenPort))
{
}

void TcpServerMT::startAccept()
{
  static int sessionId = 0;
  TcpSession::shared_pointer newSession = TcpSession::create(io_service_comm_, sessionId++);

  // When the connection is accepted, a call to TcpServerMT::handleAccept will be made
  acceptor_.async_accept(newSession->socket(),
      boost::bind(&TcpServerMT::handleAccept, this, newSession,
                  boost::asio::placeholders::error));
}

void TcpServerMT::handleAccept(TcpSession::shared_pointer newSession, const boost::system::error_code& error)
{
  // A client connected

  if (!error)
  {
    cout << "[" << boost::this_thread::get_id() << "] "
         << "Accepted a client connection from: " << newSession->socket().remote_endpoint() << endl;

    // This will cause the connection to start reading and writing
    newSession->start();
  }

  startAccept();
}

