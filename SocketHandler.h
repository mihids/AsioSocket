/* 
 * File:   SocketHandler.h
 * Author: mihiranad
 *
 * Created on April 27, 2014, 11:55 PM
 */

#ifndef SOCKETHANDLER_H
#define	SOCKETHANDLER_H

#include "AsioSocket.h"

#include "ModtLogHandlers.h"

extern modt_log::LogSink g_Logger;

using namespace modt_log;

namespace modt_socket {

    struct IOServiceWrapper { // this is required to control the life span of the io_service object....

        boost::asio::io_service io_service;
        
        IOServiceWrapper() {
            MODT_LOG_INFO(g_Logger, "IOServiceWrapper::IOServiceWrapper()", "Created IOServiceWrapper Object [" << this << "]");                        
        }
        
        virtual ~IOServiceWrapper() {
            MODT_LOG_INFO(g_Logger, "IOServiceWrapper::~IOServiceWrapper()", "Destruct IOServiceWrapper Object [" << this << "]");   
        }

    };

    class SocketHandler {
    public:

        // user interface for the socket handler
        void AsyncConnect(const char* ip, const char* port);
        void Disconnect();
        void Read(const size_t bytes);
        void AsyncWrite(const std::string msg);
        size_t Write(const std::string msg);
        boost::system::error_code Connect(const char* ip, const char* port);

        //callbacks provided to serve the interface requests 
        virtual void OnConnectionStatus(bool *isConnected, boost::system::error_code* ec) = 0;
        virtual void OnRead(const char *s, const size_t *bytes, const size_t *bytes_requested, boost::system::error_code *ec) = 0;
        virtual void OnAsyncWrite(const size_t *bytes, boost::system::error_code* ec) = 0;
        virtual void OnDisconnect() = 0;

        SocketHandler();        
        virtual ~SocketHandler();

    private:
        
        SocketHandler(const SocketHandler& orig);

        void ConfigureConnection(const char* ip, const char* port);
        
        //boost::asio::io_service io_service;
        SynchronisedQueue<size_t> read_queue;
        SynchronisedQueue<WriteMsg> write_queue;

        boost::scoped_ptr<IOServiceWrapper> io_service_wrapper;
        boost::scoped_ptr<boost::thread> thread;
        boost::scoped_ptr<AsioSocket> sock;

    };

}

#endif	/* SOCKETHANDLER_H */

