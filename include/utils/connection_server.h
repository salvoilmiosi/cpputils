#ifndef __CONNECTION_SERVER_H__
#define __CONNECTION_SERVER_H__

#include <functional>
#include <map>

#include "connection.h"
#include "ansicvt.h"

namespace net {

template<typename ParentType, typename MessageTypes>
struct connection_server_client : connection_base<connection_server_client<ParentType, MessageTypes>, MessageTypes> {
    using base = connection_base<connection_server_client<ParentType, MessageTypes>, MessageTypes>;
    using base::address_string;
    using base::error_message;

    ParentType &parent;
    int client_id;

    connection_server_client(ParentType &parent, int client_id, boost::asio::ip::tcp::socket &&socket)
        : base(parent.m_ctx, std::move(socket)), parent(parent), client_id(client_id) {}
    
    void on_receive_message(typename MessageTypes::input_message &&msg) {
        parent.on_receive_message(client_id, std::move(msg));
    }

    void on_error() {
        parent.print_message(fmt::format("{} disconnected ({})", address_string(), ansi_to_utf8(error_message())));
        parent.on_disconnect(client_id);
    }

    void on_disconnect() {
        parent.print_message(fmt::format("{} disconnected", address_string()));
        parent.on_disconnect(client_id);
    }
};

template<typename Derived, typename MessageTypes>
class connection_server {
private:
    using input_message = typename MessageTypes::input_message;
    using output_message = typename MessageTypes::output_message;
    using header_type = typename MessageTypes::header_type;

    using connection_type = connection_server_client<connection_server<Derived, MessageTypes>, MessageTypes>;
    friend connection_type;

    boost::asio::io_context &m_ctx;
    boost::asio::ip::tcp::acceptor m_acceptor;

    std::map<int, typename connection_type::pointer> m_clients;

    int m_client_id_counter = 0;

public:
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
        for (auto &[id, con] : m_clients) {
            con->disconnect();
        }
        m_acceptor.close();
    }

    void push_message(int client_id, output_message &&msg) {
        if (auto it = m_clients.find(client_id); it != m_clients.end()) {
            it->second->push_message(std::move(msg));
        }
    }

private:
    void start_accepting() {
        m_acceptor.async_accept([this](const boost::system::error_code &ec, boost::asio::ip::tcp::socket peer) {
            if (!ec) {
                if (m_clients.size() < banggame::server_max_clients) {
                    int client_id = ++m_client_id_counter;
                    auto client = connection_type::make(*this, client_id, std::move(peer));
                    client->start();
                    
                    print_message(fmt::format("{} connected", client->address_string()));

                    auto it = m_clients.emplace(client_id, std::move(client)).first;

                    using timer_type = boost::asio::basic_waitable_timer<std::chrono::system_clock>;
                    auto timer = new timer_type(m_ctx);

                    timer->expires_after(net::timeout);
                    timer->async_wait(
                        [this,
                        timer = std::unique_ptr<timer_type>(timer),
                        client_id = it->first, ptr = std::weak_ptr(it->second)](const boost::system::error_code &ec) {
                            if (!ec) {
                                if (auto client = ptr.lock()) {
                                    if (!client_validated(client_id)) {
                                        client->disconnect(net::connection_error::timeout_expired);
                                    }
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

    void on_receive_message(int client_id, input_message &&msg) {
        static_cast<Derived &>(*this).on_receive_message(client_id, std::move(msg));
    }

    void on_disconnect(int client_id) {
        m_clients.erase(client_id);
        static_cast<Derived &>(*this).on_disconnect(client_id);
    }

    bool client_validated(int client_id) const {
        if constexpr (requires (Derived obj) { obj.client_validated(client_id); }) {
            return static_cast<const Derived &>(*this).client_validated(client_id);
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

};

}

#endif