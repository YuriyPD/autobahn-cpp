///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) Crossbar.io Technologies GmbH and contributors and contributors.
//
// Boost Software License - Version 1.0 - August 17th, 2003
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////

#include "wamp_websocket_transport.hpp"

#include <boost/system/error_code.hpp>
#include <websocketpp/client.hpp>

namespace autobahn {

    template <class Config>
    inline wamp_websocketpp_websocket_transport<Config>::wamp_websocketpp_websocket_transport(
        client_type& client,
        const std::string& uri,
        bool debug_enabled)
        : wamp_websocket_transport(uri, debug_enabled)
        , m_client(client)
        , m_hdl()
        , m_open(false)
        , m_done(false)
    {
        // Bind the handlers we are using
        using websocketpp::lib::placeholders::_1;
        using websocketpp::lib::placeholders::_2;
        using websocketpp::lib::bind;
        m_client.set_open_handler(bind(&wamp_websocketpp_websocket_transport<Config>::on_ws_open, this, _1));
        m_client.set_close_handler(bind(&wamp_websocketpp_websocket_transport<Config>::on_ws_close, this, _1));
        m_client.set_fail_handler(bind(&wamp_websocketpp_websocket_transport<Config>::on_ws_fail, this, _1));
//        m_client.set_message_handler(bind(&wamp_websocketpp_websocket_transport<Config>::on_ws_message, this, std::placeholders::_1, std::placeholders::_2));
        m_client.set_message_handler(bind(&wamp_websocketpp_websocket_transport<Config>::on_ws_message, this, _1, _2));

        if(!debug_enabled) {
            m_client.clear_access_channels(websocketpp::log::alevel::all);
        }
    }

    template <class Config>
    inline wamp_websocketpp_websocket_transport<Config>::~wamp_websocketpp_websocket_transport()
    {
        
    }

    template <class Config>
    inline bool wamp_websocketpp_websocket_transport<Config>::is_open() const
    {
        return m_open;
    }

	template <class Config>
	inline bool wamp_websocketpp_websocket_transport<Config>::is_connected() const
	{
		return is_open() && !m_done;
	}

    // The open handler will signal that we are ready to start sending telemetry
    template <class Config>
    inline void wamp_websocketpp_websocket_transport<Config>::on_ws_open(websocketpp::connection_hdl) {
        scoped_lock guard(m_lock);
        m_open = true;

        //No handshake for websockets beyond declaring sub-protocol
        m_connect.set_value();

    }

    template <class Config>
    inline void wamp_websocketpp_websocket_transport<Config>::on_ws_close(websocketpp::connection_hdl hdl) {
        //Log "Connection closed!");

        scoped_lock guard(m_lock);
        m_done = true;
    }

    template <class Config>
    inline void wamp_websocketpp_websocket_transport<Config>::on_ws_fail(websocketpp::connection_hdl hdl) {
        //Log "Connection failed!");
        if (!m_open)
            m_connect.set_exception(boost::copy_exception(network_error("failed to connect")));

        scoped_lock guard(m_lock);
        m_done = true;
    }

    template <class Config>
    inline void wamp_websocketpp_websocket_transport<Config>::on_ws_message(websocketpp::connection_hdl, typename client_type::message_ptr msg) {
        if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
            receive_message(msg->get_payload());
        }
        else {
            //m_messages.push_back("<< " + websocketpp::utility::to_hex(msg->get_payload()));
        }
    }

    template <class Config>
    inline void wamp_websocketpp_websocket_transport<Config>::async_connect(const std::string& uri, boost::promise<void>& connect_promise)
    {
        websocketpp::lib::error_code ec;
        typename client_type::connection_ptr con = m_client.get_connection(uri, ec);
        if (ec) {
            //Log  "Get Connection Error: " + ec.message());
            connect_promise.set_exception(boost::copy_exception(websocketpp::lib::system_error(ec.value(), ec.category(), "connect")));
            return;
        }

        //TODO: need to abstract encoding and get subprotocol
        con->add_subprotocol("wamp.2.msgpack");

        // Grab a handle for this connection so we can talk to it in a thread
        // safe manor after the event loop starts.
        m_hdl = con->get_handle();

        // Queue the connection. No DNS queries or network connections will be
        // made until the io_service event loop is run.
        m_client.connect(con);
    }

    template <class Config>
    inline void wamp_websocketpp_websocket_transport<Config>::write(void const * payload, size_t len)
    {
        websocketpp::lib::error_code ec;
        m_client.send(m_hdl, payload, len, websocketpp::frame::opcode::binary, ec);
    }

    template <class Config>
    inline void wamp_websocketpp_websocket_transport<Config>::close()
    {
        m_client.close(m_hdl, websocketpp::close::status::normal, "disconnect");
    }

} // namespace autobahn
