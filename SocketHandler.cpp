/* 
 * File:   SocketHandler.cpp
 * Author: mihiranad
 * 
 * Created on April 27, 2014, 11:55 PM
 */

#include "SocketHandler.h"

using namespace modt_socket;

SocketHandler::SocketHandler() {

    MODT_LOG_DEBUG(g_Logger, "SocketHandler::SocketHandler()", "Creating socket handler : " << this);

}

SocketHandler::SocketHandler(const SocketHandler& orig) {

    io_service_wrapper.reset();
    thread.reset();
    sock.reset();

}

SocketHandler::~SocketHandler() {

    Disconnect();
    MODT_LOG_DEBUG(g_Logger, "SocketHandler::~SocketHandler()", "Destroying socket handler : " << this);

}

void SocketHandler::ConfigureConnection(const char* ip, const char* port) {

    //io_service1.reset()
    //every time a new connection is made, a new io_service object is generated ...    
    io_service_wrapper.reset(new IOServiceWrapper);

    read_queue.Clear(); // cancel all read requests
    write_queue.Clear(); // cancel all write requests

    sock.reset(new AsioSocket(io_service_wrapper.get()->io_service, &read_queue, &write_queue, this));
    tcp::endpoint ep(boost::asio::ip::address::from_string(ip), atoi(port)); // atoi?
    sock->start(ep);

    MODT_LOG_DEBUG(g_Logger, "SocketHandler::ConfigureConnection()", "Connection configured, new io_service object created, new asio socket created");

}

void SocketHandler::AsyncConnect(const char* ip, const char* port) {

    if (thread) {

        MODT_LOG_WARN(g_Logger, "SocketHandler::AsyncConnect()", "Connect attempted when the thread is already running, nothing will be done...");
        return; // running

    }

    ConfigureConnection(ip, port);
    //##thread.reset(new boost::thread(boost::bind(&boost::asio::io_service::run, &io_service)));
    thread.reset(new boost::thread(boost::bind(&boost::asio::io_service::run, &io_service_wrapper.get()->io_service)));
    //std::cout << "Started the new thread for connection from thread : " << thread.get()->get_id() << std::endl;
    MODT_LOG_DEBUG(g_Logger, "SocketHandler::AsyncConnect()", "A new thread is made for the new connection.... Thread ID : " << thread.get()->get_id());


    //boost::thread bt(boost::bind(&boost::asio::io_service::run, &io_service));
    //bt.detach();
    //there will be no access to this thread now... it's unleashed in the background    

}

void SocketHandler::Disconnect() { // should be called by the user probably on errors for sending/recieving....

    if (!thread) {

        MODT_LOG_WARN(g_Logger, "SocketHandler::Disconnect()", "Disconnect attempted when no thread is running, nothing will be done...");
        return; // stopped - no thread exists......    

    }

    io_service_wrapper.get()->io_service.stop(); // allows the thread to exit
    MODT_LOG_DEBUG(g_Logger, "SocketHandler::Disconnect()", "Stopped the current io_service");

    MODT_LOG_DEBUG(g_Logger, "SocketHandler::Disconnect()", "Thread joined. Thread ID : " << thread.get()->get_id());

    thread.get()->join(); // blocks the current thread until the service thread completes....    

    MODT_LOG_DEBUG(g_Logger, "SocketHandler::Disconnect()", "Thread join complete");

    io_service_wrapper.get()->io_service.reset();
    MODT_LOG_DEBUG(g_Logger, "SocketHandler::Disconnect()", "Reset the current io_service");

    thread.reset();
    MODT_LOG_DEBUG(g_Logger, "SocketHandler::Disconnect()", "Deleted the reference to the thread");

    //cancel all operation of timers, close the socket
    sock.get()->abort();
    //after this point no services are supposed to access the io_service object, everything is cancelled

    sock.reset(); // all the socket related services are destroyed, they won't exist anymore, so we can safely delete the io_service object
    MODT_LOG_DEBUG(g_Logger, "SocketHandler::Disconnect()", "Deleted the reference to the current asio scoket");

    // the disconnect destroys the ioservice wrapper shutting down all associated handlers....
    io_service_wrapper.reset();
    MODT_LOG_DEBUG(g_Logger, "SocketHandler::Disconnect()", "Deleted the reference to the current io_service");

    // all done, now the SocketHandler can be destroyed
}

void SocketHandler::Read(const size_t bytes) {

    read_queue.Enqueue(bytes);

}

void SocketHandler::AsyncWrite(const std::string msg) {

    WriteMsg t;
    t.msg = msg;
    write_queue.Enqueue(t);

}

size_t SocketHandler::Write(const std::string msg) {

    return sock->blocking_write(msg);

}

boost::system::error_code SocketHandler::Connect(const char* ip, const char* port) {

    boost::system::error_code error = boost::asio::error::already_started;
    
    if (thread) {

        MODT_LOG_WARN(g_Logger, "SocketHandler::Connect()", "Connect attempted when the thread is already running, nothing will be done...");
        return error; // running

    }

    io_service_wrapper.reset(new IOServiceWrapper);

    read_queue.Clear(); // cancel all read requests
    write_queue.Clear(); // cancel all write requests

    sock.reset(new AsioSocket(io_service_wrapper.get()->io_service, &read_queue, &write_queue, this));
    tcp::endpoint ep(boost::asio::ip::address::from_string(ip), atoi(port)); // atoi?

    // handle the sync connect    
    sock.get()->handle_blocking_connect(ep, error);
    if (error) {
        sock.reset();
        io_service_wrapper.reset();        
        return error;
    }

    thread.reset(new boost::thread(boost::bind(&boost::asio::io_service::run, &io_service_wrapper.get()->io_service)));
    //std::cout << "Started the new thread for connection from thread : " << thread.get()->get_id() << std::endl;
    MODT_LOG_DEBUG(g_Logger, "SocketHandler::Connect()", "A new thread is made for the new connection.... Thread ID : " << thread.get()->get_id());
    return error;

}
