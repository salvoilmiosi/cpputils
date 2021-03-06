#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <asio.hpp>
#include <fmt/core.h>

#include <vector>
#include <memory>
#include <chrono>

#include "binary_serial.h"
#include "enum_error_code.h"

namespace net {

    DEFINE_ENUM_ERROR_CODE(connection_error,
        (no_error,              "No Error")
        (timeout_expired,       "Timeout Expired")
        (validation_failure,    "Validation Failure")
        (invalid_message,       "Invalid Message")
    )

    constexpr auto timeout = std::chrono::seconds(5);

    template<typename T>
    concept header = requires (const T value) {
        requires binary::serializable<T>;
        requires binary::deserializable<T>;
        requires std::is_trivially_copyable_v<T>;
        { value.validate() } -> std::convertible_to<bool>;
        { value.length } -> std::convertible_to<size_t>;
    };

    template<binary::deserializable InputMessage, binary::serializable OutputMessage, header HeaderType>
    struct message_types {
        using input_message = InputMessage;
        using output_message = OutputMessage;
        using header_type = HeaderType;
    };

    template<typename Derived, typename MessageTypes>
    class connection : public std::enable_shared_from_this<Derived> {
    public:
        using std::enable_shared_from_this<Derived>::shared_from_this;
        using pointer = std::shared_ptr<Derived>;
        using handle = std::weak_ptr<Derived>;

        static pointer make(auto && ... args) {
            return pointer(new Derived(FWD(args) ... ));
        }

    protected:
        enum class connection_state : uint8_t {
            disconnected,
            error,
            resolving,
            connecting,
            connected
        };

        connection(asio::io_context &ctx, asio::ip::tcp::socket &&socket)
            : m_socket(std::move(socket))
            , m_strand(ctx)
            , m_timer(ctx)
            , m_state(connection_state::connecting)
        {
            auto endpoint = m_socket.remote_endpoint();
            m_address = fmt::format("{}:{}", endpoint.address().to_string(), endpoint.port());
        }

        connection(asio::io_context &ctx)
            : m_socket(ctx)
            , m_strand(ctx)
            , m_timer(ctx)
            , m_state(connection_state::disconnected) {}

    public:
        void connect(const std::string &host, uint16_t port, auto &&on_complete) {
            auto self(shared_from_this());
            auto resolver = new asio::ip::tcp::resolver(m_socket.get_executor());
            m_state = connection_state::resolving;
            resolver->async_resolve(asio::ip::tcp::v4(), host, std::to_string(port),
                [this, self, host = fmt::format("{}:{}", host, port),
                    resolver = std::unique_ptr<asio::ip::tcp::resolver>(resolver),
                    on_complete = std::move(on_complete)]
                (const std::error_code &ec, asio::ip::tcp::resolver::results_type results) mutable {
                    if (m_state != connection_state::resolving) {
                        on_complete(asio::error::operation_aborted);
                    } else if (ec) {
                        disconnect(ec);
                        on_complete(ec);
                    } else {
                        m_state = connection_state::connecting;
                        m_socket.async_connect(*results,
                            [this, self, host = std::move(host), on_complete = std::move(on_complete)]
                            (const std::error_code &ec) {
                                if (!ec) {
                                    m_address = host;
                                } else {
                                    disconnect(ec);
                                }
                                on_complete(ec);
                            });
                    }
                });
        }

        void disconnect(const std::error_code &ec = {}) {
            switch (m_state) {
            case connection_state::error:
            case connection_state::disconnected:
                break;
            case connection_state::connecting:
            case connection_state::connected:
                if (m_socket.is_open()) {
                    asio::post(m_socket.get_executor(),
                        [this, self = shared_from_this()]{
                            std::error_code ec;
                            m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                            m_socket.close(ec);
                        });
                }
            [[fallthrough]];
            default:
                m_out_queue.clear();
                if (!ec || ec == std::error_code(asio::error::eof)) {
                    m_state = connection_state::disconnected;
                    if constexpr (requires (Derived obj) { obj.on_disconnect(); }) {
                        static_cast<Derived &>(*this).on_disconnect();
                    }
                } else {
                    m_state = connection_state::error;
                    if constexpr (requires (Derived obj) { obj.on_error(ec); }) {
                        static_cast<Derived &>(*this).on_error(ec);
                    }
                }
            }
        }

        const std::string &address_string() const {
            return m_address;
        }
        
    public:
        void push_message(auto && ... args) {
            auto self(shared_from_this());
            asio::post(m_socket.get_executor(),
                asio::bind_executor(m_strand,
                    [this, self, ... args = FWD(args)] () mutable {
                        bool empty = m_out_queue.empty();
                        m_out_queue.push_back(wrap_message(std::move(args) ... ));
                        if (empty) {
                            write_next_message();
                        }
                    }));
        }

        void start() {
            if (m_state == connection_state::connecting) {
                m_state = connection_state::connected;
                read_next_message();
            }
        }

    private:
        using input_message = typename MessageTypes::input_message;
        using output_message = typename MessageTypes::output_message;
        using header_type = typename MessageTypes::header_type;

        void read_next_message() {
            auto self(shared_from_this());
            m_buffer.resize(sizeof(header_type));
            asio::async_read(m_socket, asio::buffer(m_buffer),
                [this, self](const std::error_code &ec, size_t nbytes) {
                    if (!ec) {
                        header_type h = binary::deserialize<header_type>(m_buffer);
                        if (h.validate()) {
                            m_timer.expires_after(timeout);
                            m_timer.async_wait([this](const std::error_code &ec) {
                                if (!ec) {
                                    m_socket.cancel();
                                    disconnect(connection_error::timeout_expired);
                                }
                            });

                            m_buffer.resize(h.length);
                            asio::async_read(m_socket, asio::buffer(m_buffer),
                                [this, self](const std::error_code &ec, size_t nbytes) {
                                    m_timer.cancel();
                                    if (!ec) {
                                        try {
                                            static_cast<Derived &>(*this).on_receive_message(binary::deserialize<input_message>(m_buffer));
                                            read_next_message();
                                        } catch (const binary::read_error &error) {
                                            disconnect(error.code());
                                        }
                                    } else {
                                        disconnect(ec);
                                    }
                                });
                        } else {
                            disconnect(connection_error::validation_failure);
                        }
                    } else {
                        disconnect(ec);
                    }
                });
        }

        std::vector<std::byte> wrap_message(auto && ... args) {
            const output_message msg(FWD(args) ... );
            
            header_type h;
            h.length = binary::get_size(msg);

            std::vector<std::byte> data;
            data.reserve(sizeof(h) + h.length);

            binary::serializer<header_type>{}(h, data);
            binary::serializer<output_message>{}(msg, data);

            return data;
        }

        void write_next_message() {
            auto self(shared_from_this());
            asio::async_write(m_socket, asio::buffer(m_out_queue.front()),
                [this, self](const std::error_code &ec, size_t nbytes) {
                    if (!ec) {
                        m_out_queue.pop_front();
                        if (!m_out_queue.empty()) {
                            write_next_message();
                        }
                    } else {
                        disconnect(ec);
                    }
                });
        }

    private:
        asio::ip::tcp::socket m_socket;
        asio::io_context::strand m_strand;
        asio::basic_waitable_timer<std::chrono::system_clock> m_timer;
        
        std::atomic<connection_state> m_state;

        std::deque<std::vector<std::byte>> m_out_queue;

        std::vector<std::byte> m_buffer;
        std::string m_address;
    };

}

#endif