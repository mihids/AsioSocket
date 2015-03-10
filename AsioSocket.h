/* 
 * File:   AsioSocket.h
 * Author: mihiranad
 *
 * Created on April 27, 2014, 11:47 PM
 */

#ifndef ASIOSOCKET_H
#define	ASIOSOCKET_H

#define CONNECT_TIMEOUT 10 // seconds
#define READ_TIMEOUT 10 // seconds
#define READ_WAIT 5 // milliseconds
#define WRITE_WAIT 5 // milliseconds
#define BUFF_SIZE 65000 // read buffer size, max tcp messsage size allocated, suggested on stack overflow

#include <iostream>

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include <boost/bind.hpp>

#include "SharedQueue.h"

#include "ModtLogHandlers.h"

extern modt_log::LogSink g_Logger;

using boost::asio::deadline_timer;
using boost::asio::ip::tcp;

using namespace modt_log;

namespace modt_socket {

    typedef boost::function< void() > callback;

    struct WriteMsg { // this is used in the write queue for convenience 

        std::string msg;

    };

    class SocketHandler; // forward declaration....

    class AsioSocket : public boost::enable_shared_from_this<AsioSocket> {
    public:

        AsioSocket(boost::asio::io_service& io_service,
                SynchronisedQueue<size_t>* read_queue,
                SynchronisedQueue<WriteMsg>* write_queue,
                SocketHandler* handler)
        : stopped_(false),
        connection_status_(false),
        connect_deadline_passed(false),
        write_bytes_(0),
        read_bytes_(0),
        socket_(io_service),
        deadline_(io_service),
        read_timer_(io_service, boost::posix_time::seconds(1)),
        write_timer_(io_service, boost::posix_time::seconds(1)),
        _read_queue(read_queue),
        _write_queue(write_queue),
        _handler(handler) {

            SetCallbacks();
            MODT_LOG_INFO(g_Logger, "AsioSocket::AsioSocket()", "Created AsioSocket Object [" << this << "]");

        }

        virtual ~AsioSocket() {
            MODT_LOG_INFO(g_Logger, "AsioSocket::~AsioSocket()", "Destruct AsioSocket Object [" << this << "]");
        }

        // synchronous write operation
        size_t blocking_write(const std::string &msg);
        void handle_blocking_connect(tcp::endpoint ep, boost::system::error_code& ec);

        // Called by the user of the AsioSocket class to initiate the connection process.
        void start(tcp::endpoint ep);
        void abort();

    private:

        // callable within the class, e.g. _onread()
        callback _onread;
        callback _onasyncwrite;
        callback _onconnect;
        callback _ondisconnect;

        void set_callback(callback onread,
                callback onasyncwrite,
                callback onconnect,
                callback ondisconnect) {

            _onread = onread;
            _onasyncwrite = onasyncwrite;
            _onconnect = onconnect;
            _ondisconnect = ondisconnect;
        }

        // called to explicitly close the socket and stop the thread, i.e the io_service....
        void stop();

        // This function terminates all the actors to shut down the connection. It
        // may be called by the user of the AsioSocket class, or by the class itself in
        // response to graceful termination or an unrecoverable error.

        void SetCallbacks();

        void start_connect(tcp::endpoint ep);

        void handle_connect(const boost::system::error_code& ec,
                tcp::endpoint ep);

        void start_read();

        void handle_read(const boost::system::error_code& ec, size_t bytes);

        void start_write();

        void handle_write(const boost::system::error_code& ec, size_t bytes);

        void check_deadline();

        // member variables  

        bool stopped_; // indicates if the service is stopped or not
        bool connection_status_;
        bool connect_deadline_passed;

        size_t write_bytes_;
        size_t read_bytes_;
        size_t bytes_to_read;

        tcp::socket socket_; // underlying asio socket....

        deadline_timer deadline_;
        deadline_timer read_timer_;
        deadline_timer write_timer_;

        // these queues are used to parse the write and read objects
        SynchronisedQueue<size_t>* _read_queue;
        SynchronisedQueue<WriteMsg>* _write_queue;

        SocketHandler* _handler; // used to invoke callbacks....

        char read_buffer_[BUFF_SIZE]; // this buffer gets filled when reading from socket....

        boost::system::error_code error_code_;

    };

}

#endif	/* ASIOSOCKET_H */

