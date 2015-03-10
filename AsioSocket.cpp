/* 
 * File:   AsioSocket.cpp
 * Author: mihiranad
 * 
 * Created on April 27, 2014, 11:47 PM
 */

#include "AsioSocket.h"

#include "SocketHandler.h"

using namespace modt_socket;

void AsioSocket::SetCallbacks() {

    set_callback(boost::bind(&SocketHandler::OnRead, _handler, read_buffer_, &read_bytes_, &bytes_to_read, &error_code_),
            boost::bind(&SocketHandler::OnAsyncWrite, _handler, &write_bytes_, &error_code_),
            boost::bind(&SocketHandler::OnConnectionStatus, _handler, &connection_status_, &error_code_),
            boost::bind(&SocketHandler::OnDisconnect, _handler));
}

void AsioSocket::start(tcp::endpoint ep) {
    // Start the connect actor.
    start_connect(ep);

    // Start the deadline actor. You will note that we're not setting any
    // particular deadline here. Instead, the connect and input actors will
    // update the deadline prior to each asynchronous operation.
    deadline_.async_wait(boost::bind(&AsioSocket::check_deadline, this));

}

void AsioSocket::stop() {

    abort();
    _ondisconnect();
    MODT_LOG_DEBUG(g_Logger, "AsioSocket::stop()", "Stopped the socket object and deadline canceled");

}

void AsioSocket::abort() {

    stopped_ = true;
    //socket_.cancel();
    socket_.close();
    deadline_.cancel();
    read_timer_.cancel();
    write_timer_.cancel();
    MODT_LOG_DEBUG(g_Logger, "AsioSocket::abort()", "Aborted the socket object and deadline canceled");

}

size_t AsioSocket::blocking_write(const std::string& msg) {

    if (stopped_) // if the service is stopped don't attempt to send because the socket is closed....
        return 0;

    MODT_LOG_INFO(g_Logger, "AsioSocket::blocking_write()", "Sending message : " << msg);

    size_t bytes_written = 0;
    try {
        bytes_written = socket_.write_some(boost::asio::buffer(msg)); // synchronous write operation
    } catch (std::exception& e) {
        MODT_LOG_ERROR(g_Logger, "AsioSocket::blocking_write()", e.what());
    }

    return bytes_written;

}

void AsioSocket::start_connect(tcp::endpoint ep) {

    MODT_LOG_DEBUG(g_Logger, "AsioSocket::start_connect()", "Attempting connection to " << ep.address().to_string() << ":" << ep.port());

    // Set a deadline for the connect operation.
    deadline_.expires_from_now(boost::posix_time::seconds(CONNECT_TIMEOUT));

    // Start the asynchronous connect operation.
    socket_.async_connect(ep,
            boost::bind(&AsioSocket::handle_connect,
            this, _1, ep));

}

void AsioSocket::handle_connect(const boost::system::error_code& ec,
        tcp::endpoint ep) {

    if (stopped_)
        return;

    error_code_ = ec;

    // The async_connect() function automatically opens the socket at the start
    // of the asynchronous operation. If the socket is closed at this time then
    // the timeout handler must have run first...
    if (!socket_.is_open()) {

        connection_status_ = false;

        MODT_LOG_ERROR(g_Logger, "AsioSocket::handle_connect()", "Connection timed out....");
        _onconnect();

    }// Check if the connect operation failed before the deadline expired.
    else if (ec) {

        connection_status_ = false;

        // We need to close the socket used in the previous connection attempt
        // before starting a new one.
        socket_.close();
        MODT_LOG_ERROR(g_Logger, "AsioSocket::handle_connect()", "Connection error....");

        _onconnect();

    }// Otherwise we have successfully established a connection.
    else {

        connection_status_ = true;
        MODT_LOG_DEBUG(g_Logger, "AsioSocket::handle_connect()", "Connection successful....");

        _onconnect();
        connect_deadline_passed = true;

        // Start the input actor.....
        // This will read the number of bytes requested via the read queue and callback _onread
        start_read();

        // Start the write actor.....
        // This will write the string requested via the write queue and callback _onwrite
        start_write();

    }
}

void AsioSocket::start_read() {

    // Set a deadline for the read operation.
    deadline_.expires_from_now(boost::posix_time::seconds(READ_TIMEOUT));

    // Start an asynchronous operation to serve the incoming read requests via the enqueues from socket handler...   
    bytes_to_read = 0;
    if (_read_queue->try_Dequeue(bytes_to_read)) {

        MODT_LOG_INFO(g_Logger, "AsioSocket::start_read()", "Read request : " << bytes_to_read << " bytes");
        memset(read_buffer_, 0, sizeof (read_buffer_)); // clean the buffer before the next read
        if (bytes_to_read > BUFF_SIZE) {

            bytes_to_read = BUFF_SIZE;
            MODT_LOG_ERROR(g_Logger, "AsioSocket::start_read()", "Attempt to read more than maximum buffer size...");

        }

        boost::asio::async_read(socket_, boost::asio::buffer(read_buffer_), boost::asio::transfer_exactly(bytes_to_read),
                boost::bind(&AsioSocket::handle_read, this, _1, _2));
    } else { // if no reads are requested at this time wait for READ_WAIT milliseconds

        read_timer_.expires_at(read_timer_.expires_at() + boost::posix_time::millisec(READ_WAIT));
        read_timer_.async_wait(boost::bind(&AsioSocket::start_read, this));
    }

}

void AsioSocket::handle_read(const boost::system::error_code& ec, size_t bytes) {

    if (stopped_)
        return;

    read_bytes_ = bytes;
    error_code_ = ec;

    //std::string msg(read_buffer_, bytes);
    //std::cout << msg << std::endl;

    if (ec) {
        MODT_LOG_ERROR(g_Logger, "AsioSocket::handle_read()", "Read Error : " << ec.message());
        stop();
        return;
    } // _onread will not be called in this instance....

    _onread(); //make the callback to notify user via the socket handler...

    // start reading again....
    start_read();

}

void AsioSocket::start_write() {

    if (stopped_)
        return;

    WriteMsg message_to_write;

    if (_write_queue->try_Dequeue(message_to_write)) {

        // Start an asynchronous operation to send the messages to the server...        
        MODT_LOG_INFO(g_Logger, "AsioSocket::start_write()", "Sending message : " << message_to_write.msg);
        boost::asio::async_write(socket_, boost::asio::buffer(message_to_write.msg),
                boost::bind(&AsioSocket::handle_write, this, _1, _2));

    } else { // if there's nothing to write at this time, wait WRITE_TIME before polling again...

        write_timer_.expires_at(write_timer_.expires_at() + boost::posix_time::millisec(WRITE_WAIT));
        write_timer_.async_wait(boost::bind(&AsioSocket::start_write, this));

    }

}

void AsioSocket::handle_write(const boost::system::error_code& ec, size_t bytes) {

    if (stopped_)
        return;

    write_bytes_ = bytes;
    error_code_ = ec;

    if (ec) {
        MODT_LOG_ERROR(g_Logger, "AsioSocket::handle_write()", "Write Error : " << ec.message());
        stop();
        return;
    } // _onaasyncwrite will not be called in this instance....

    _onasyncwrite(); // make the callback    

    start_write();

}

void AsioSocket::check_deadline() {

    if (stopped_)
        return;

    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if (deadline_.expires_at() <= deadline_timer::traits_type::now()) {
        // The deadline has passed. The socket is closed so that any outstanding
        // asynchronous operations are cancelled.
        if (connect_deadline_passed) { // then this is a read timeout...

            MODT_LOG_ERROR(g_Logger, "AsioSocket::check_deadline()", "Read time out...");
            //abort();
            // it's ok - don't do anything and return
            return;

        } else {

            MODT_LOG_ERROR(g_Logger, "AsioSocket::check_deadline()", "Connect time out...");
            abort();
            //stop(); // a connect or read timeout has occurred....

            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            deadline_.expires_at(boost::posix_time::pos_infin);
        }
    }

    // Put the actor back to sleep.
    deadline_.async_wait(boost::bind(&AsioSocket::check_deadline, this));
}

void AsioSocket::handle_blocking_connect(tcp::endpoint ep, boost::system::error_code& ec) {

    boost::system::error_code error = boost::asio::error::host_not_found;

    if (stopped_)
        return;

    MODT_LOG_DEBUG(g_Logger, "AsioSocket::handle_blocking_connect()", "Waiting for connection establishment");
    socket_.connect(ep, error);

    if (error) {
        MODT_LOG_DEBUG(g_Logger, "AsioSocket::handle_blocking_connect()", "Connection error : " << error.message());
    } else {
        MODT_LOG_DEBUG(g_Logger, "AsioSocket::handle_blocking_connect()", "Connection success");

        // Start the input actor.....
        // This will read the number of bytes requested via the read queue and callback _onread
        start_read();

        // Start the write actor.....
        // This will write the string requested via the write queue and callback _onwrite
        start_write();

    }

    ec = error;

}
