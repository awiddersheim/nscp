#pragma once

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#ifdef USE_SSL
#include <boost/asio/ssl/context.hpp>
#endif

namespace socket_helpers {
	namespace server {

		using boost::asio::ip::tcp;
		static const bool debug_trace = true;

		//
		// The socket statemachine:
		// This is the idea not how it currently looks
		// SOCKET  | SSL-SOCKET | PROTOCOL   | RETURN
		//         | connect    |            | 
		// connect | handhake   | on_connect | true = allow, false = disallow
		// ...
		//         |            | wants_data | true = yes, read chunk, false = no, start sending
		// recv    | recv       | on_read    | true = read more, false = done reading
		//         |            | has_data   | true = yes, send chunk, false = no, stop sending
		// recv    | recv       | on_read    | true = read more, false = done reading
		// ...
		//         |            | is_done    | true = is done, disconnect, false (read/write loop)


		template<class protocol_type, std::size_t N>
		class connection : public boost::enable_shared_from_this<connection<protocol_type, N> >, private boost::noncopyable {
			typedef connection<protocol_type, N> connection_type;
		public:
			connection(boost::asio::io_service& io_service, boost::shared_ptr<protocol_type> protocol) 
				: strand_(io_service)
				, timer_(io_service)
				, protocol_(protocol)
			{
			}
			virtual ~connection() {
			}

			inline void trace(std::string msg) const {
				if (debug_trace) 
					protocol_->log_debug(__FILE__, __LINE__, msg);
			}

			virtual boost::asio::ip::tcp::socket& socket() = 0;

			//////////////////////////////////////////////////////////////////////////
			// High level connection start/stop
			virtual void start() {
				trace("start()");
				if (protocol_->on_connect()) {
					set_timeout(protocol_->get_info().timeout);
					do_process();
				} else {
					stop();
				}
			}

			virtual void stop() {
				trace("stop()");
				cancel_timer();
				boost::system::error_code ignored_ec;
				socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
			}

			//////////////////////////////////////////////////////////////////////////
			// Timeout related functions
			virtual void set_timeout(int seconds) {
				timer_.expires_from_now(boost::posix_time::seconds(seconds));
				timer_.async_wait(boost::bind(&connection::timeout, this->shared_from_this(), boost::asio::placeholders::error));  
			}

			virtual void cancel_timer() {
				timer_.cancel();
			}

			virtual void timeout(const boost::system::error_code& e) {
				if (e != boost::asio::error::operation_aborted) {
					trace("timeout()");
					boost::system::error_code ignored_ec;
					socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Socket state machine (assumed all sockets are simple connect-read-write-disconnect

			void do_process() {
				if (protocol_->wants_data()) {
					start_read_request();
				} else if (protocol_->has_data()) {
					//std::vector<boost::asio::const_buffer> buffers;
					//buffers.push_back();
					start_write_request(buf(protocol_->get_outbound()));
				} else {
					stop();
				}
			}

			virtual void start_read_request() = 0;
			virtual void handle_read_request(const boost::system::error_code& e, std::size_t bytes_transferred) {
				trace("handle_read_request(" + strEx::s::xtos(bytes_transferred) + ")");
				if (!e) {
					if (protocol_->on_read(buffer_.begin(), buffer_.begin() + bytes_transferred)) {
						do_process();
					} else {
						stop();
					}
				} else {
					protocol_->log_error(__FILE__, __LINE__, "Failed to read data: " + e.message());
				}
			}

			virtual void start_write_request(const boost::asio::const_buffer& response) = 0;
			virtual void handle_write_response(const boost::system::error_code& e, std::size_t bytes_transferred) {
				trace("handle_write_response(" + strEx::s::xtos(bytes_transferred) + ")");
				if (!e) {
					protocol_->on_write();
					do_process();
				} else {
					protocol_->log_error(__FILE__, __LINE__, "Failed to send data: " + e.message());
				}
			}

		protected:
			//////////////////////////////////////////////////////////////////////////
			// Internal functions and data

			boost::asio::const_buffer buf(const typename protocol_type::outbound_buffer_type s) {
				buffers_.push_back(s);
				return boost::asio::buffer(buffers_.back());
			}


			boost::asio::io_service::strand strand_;
			boost::array<char, N> buffer_;
			boost::asio::deadline_timer timer_;
			std::list<typename protocol_type::outbound_buffer_type> buffers_;
			boost::shared_ptr<protocol_type> protocol_;
		};

		template<class protocol_type, std::size_t N>
		class tcp_connection : public connection<protocol_type, N> {
		private:
			typedef connection<protocol_type, N> parent_type;
			typedef tcp_connection<protocol_type, N> my_type;

			boost::asio::ip::tcp::socket socket_;

		public:
			tcp_connection(boost::asio::io_service& io_service, boost::shared_ptr<protocol_type> protocol) 
				: connection<protocol_type, N>(io_service, protocol)
				, socket_(io_service)
			{
			}
			virtual ~tcp_connection() {
			}

			virtual boost::asio::ip::tcp::socket& socket() {
				return socket_;
			}

			virtual void start_read_request() {
				this->trace("tcp::start_read_request()");
				socket_.async_read_some(
					boost::asio::buffer(parent_type::buffer_), parent_type::strand_.wrap(
					boost::bind(&parent_type::handle_read_request, this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					));
			}
			virtual void start_write_request(const boost::asio::const_buffer& response) {
				this->trace("start_write_request(" + strEx::s::xtos(boost::asio::buffer_size(response)) + ")");
				boost::asio::async_write(socket_, boost::asio::const_buffers_1(response), parent_type::strand_.wrap(
					boost::bind(&parent_type::handle_write_response, this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					));
			}
		};

#ifdef USE_SSL
		template<class protocol_type, std::size_t N>
		class ssl_connection : public connection<protocol_type, N> {
			typedef connection<protocol_type, N> parent_type;
			typedef ssl_connection<protocol_type, N> my_type;
		public:
			ssl_connection(boost::asio::io_service& io_service, boost::asio::ssl::context &context, boost::shared_ptr<protocol_type> protocol) 
				: connection<protocol_type, N>(io_service, protocol)
				, ssl_socket_(io_service, context)
			{
			}
			virtual ~ssl_connection() {
			}


			virtual boost::asio::ip::tcp::socket& socket() {
				return ssl_socket_.next_layer();
			}


			virtual void start() {
				this->trace("ssl::start_read_request()");
				boost::shared_ptr<my_type> self = boost::shared_dynamic_cast<my_type>(this->shared_from_this());
				ssl_socket_.async_handshake(boost::asio::ssl::stream_base::server,parent_type::strand_.wrap(
					boost::bind(&ssl_connection::handle_handshake, self, boost::asio::placeholders::error)
					));
			}
			
			virtual void handle_handshake(const boost::system::error_code& e) {
				if (!e)
					parent_type::start();
				else {
					parent_type::protocol_->log_error(__FILE__, __LINE__, "Failed to establish secure connection: " + e.message());
				}
			}

			virtual void start_read_request() {
				this->trace("ssl::start_read_request()");
				ssl_socket_.async_read_some(
					boost::asio::buffer(parent_type::buffer_),
					parent_type::strand_.wrap(
					boost::bind(&parent_type::handle_read_request, this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					)
					);
			}

			virtual void start_write_request(const boost::asio::const_buffer& response) {
				this->trace("ssl::start_write_request(" + strEx::s::xtos(boost::asio::buffer_size(response)) + ")");
				boost::asio::async_write(ssl_socket_, boost::asio::const_buffers_1(response),
					parent_type::strand_.wrap(
					boost::bind(&parent_type::handle_write_response, this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
					)
					);
			}

		protected:
			typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;
			ssl_socket ssl_socket_;
		};
#endif
	} // namespace server
} // namespace socket_helpers