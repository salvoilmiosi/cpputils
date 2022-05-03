#ifndef __CONNECTION_SERVER_H__
#define __CONNECTION_SERVER_H__

#include <functional>
#include <set>

#include "connection.h"
#include "ansicvt.h"

namespace net {

template<typename ParentType, typename MessageTypes>
struct connection_server_client : connection<connection_server_client<ParentType, MessageTypes>, MessageTypes> {
    using base = connection<connection_server_client<ParentType, MessageTypes>, MessageTypes>;

    ParentType &parent;

    connection_server_client(ParentType &parent, boost::asio::ip::tcp::socket &&socket)
        : base(parent.m_ctx, std::move(socket)), parent(parent) {}
    
    void on_receive_message(typename MessageTypes::input_message &&msg) {
        parent.on_receive_message(this->weak_from_this(), std::move(msg));
    }

    void on_error(const std::error_code &ec) {
        parent.print_message(fmt::format("{} disconnected ({})", this->address_string(), ansi_to_utf8(ec.message())));
        parent.on_disconnect(this->weak_from_this());
    }

    void on_disconnect() {
        parent.print_message(fmt::format("{} disconnected", this->address_string()));
        parent.on_disconnect(this->weak_from_this());
    }
};

template<typename Derived, typename MessageTypes>
class connection_server {
public:
    using input_message = typename MessageTypes::input_message;
    using output_message = typename MessageTypes::output_message;
    using header_type = typename MessageTypes::header_type;

    using connection_type = connection_server_client<connection_server<Derived, MessageTypes>, MessageTypes>;
    using connection_handle = typename connection_type::handle;

    connection_server(boost::asio::io_context &ctx)
        : m_ctx(ctx), m_acceptor(ctx) {}

    bool start(uint16_t port) {
        try {
            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
            m_acceptor.open(endpoint.protocol());
            m_acceptor.bind(endpoint);
            m_acceptor.listen();
        } catch (const boost::system::system_error &error) {
            print_error(ansi_to_utf8(error.code().message()));
            return false;
        }

        print_message(fmt::format("Server listening on port {}", port));

        start_accepting();
        return true;
    }
    
    void stop() {
        for (auto &con : m_clients) {
            if (auto ptr = con.lock()) {
                ptr->disconnect();
            }
        }
        m_acceptor.close();
    }

private:
    void start_accepting() {
        m_acceptor.async_accept([this](const boost::system::error_code &ec, boost::asio::ip::tcp::socket peer) {
            if (!ec) {
                if (m_clients.size() < banggame::server_max_clients) {
                    auto client = connection_type::make(*this, std::move(peer));
                    m_clients.insert(client);
                    
                    client->start();
                    print_message(fmt::format("{} connected", client->address_string()));

                    using timer_type = boost::asio::basic_waitable_timer<std::chrono::system_clock>;
                    auto timer = new timer_type(m_ctx);

                    timer->expires_after(net::timeout);
                    timer->async_wait(
                        [this, handle = std::weak_ptr(client),
                        timer = std::unique_ptr<timer_type>(timer)](const boost::system::error_code &ec) {
                            if (!ec && !client_validated(handle)) {
                                if (auto client = handle.lock()) {
                                    client->disconnect(net::connection_error::timeout_expired);
                                }
                            }
                        });
                } else {
                    peer.close();
                }
            }
            if (ec != boost::asio::error::operation_aborted) {
                start_accepting();
            }
        });
    }

    void on_receive_message(connection_handle client, input_message &&msg) {
        static_cast<Derived &>(*this).on_receive_message(client, std::move(msg));
    }

    void on_disconnect(connection_handle client) {
        m_clients.erase(client);
        static_cast<Derived &>(*this).on_disconnect(client);
    }

    bool client_validated(connection_handle client) const {
        if constexpr (requires (Derived obj) { obj.client_validated(client); }) {
            return static_cast<const Derived &>(*this).client_validated(client);
        } else {
            return true;
        }
    }

    void print_message(const std::string &msg) {
        if constexpr (requires (Derived obj) { obj.print_message(msg); }) {
            static_cast<Derived &>(*this).print_message(msg);
        }
    }

    void print_error(const std::string &msg) {
        if constexpr (requires (Derived obj) { obj.print_error(msg); }) {
            static_cast<Derived &>(*this).print_error(msg);
        }
    }

private:
    friend connection_type;

    boost::asio::io_context &m_ctx;
    boost::asio::ip::tcp::acceptor m_acceptor;

    std::set<connection_handle, std::owner_less<connection_handle>> m_clients;

};

}

#endif