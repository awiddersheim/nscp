#pragma once

#include <nrpe/packet.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

#include "handler.hpp"

namespace nrpe {
	namespace server {
		class parser : public boost::noncopyable{
			unsigned int payload_length_;
			unsigned int packet_length_;
			std::vector<char> buffer_;
		public:
			parser(unsigned int payload_length) 
				: payload_length_(payload_length)
				, packet_length_(nrpe::length::get_packet_length(payload_length)) 
			{}

			template <typename InputIterator>
			boost::tuple<bool, InputIterator> digest(InputIterator begin, InputIterator end) {
				int count = packet_length_ - buffer_.size();
				for (; count > 0&& begin != end; ++begin, --count)
					buffer_.push_back(*begin);
				return boost::make_tuple(buffer_.size() >= packet_length_, begin);
			}

			nrpe::packet parse() {
				nrpe::packet packet(buffer_, payload_length_);
				buffer_.clear();
				return packet;
			}
			void reset() {
				buffer_.clear();
			}
		};

	}// namespace server
} // namespace nrpe
