//! @file mosh/fcgi/fcgistream/fcgistream.tcc Defines member functions for Fcgistream
/***************************************************************************
* Copyright (C) 2011 m0shbear                                              *
*               2007 Eddie                                                 *
*                                                                          *
* This file is part of mosh-fcgi.                                          *
*                                                                          *
* mosh-fcgi is free software: you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as  published   *
* by the Free Software Foundation, either version 3 of the License, or (at *
* your option) any later version.                                          *
*                                                                          *
* mosh-fcgi is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or    *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public     *
* License for more details.                                                *
*                                                                          *
* You should have received a copy of the GNU Lesser General Public License *
* along with mosh-fcgi.  If not, see <http://www.gnu.org/licenses/>.       *
****************************************************************************/

#ifndef MOSH_FCGI_FCGISTREAM_FCGISTREAM_TCC
#define MOSH_FCGI_FCGISTREAM_FCGISTREAM_TCC

#include <algorithm>
#include <limits>
#include <locale>
#include <memory>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <ios>
#include <mosh/fcgi/exceptions.hpp>
#include <mosh/fcgi/bits/block.hpp>
#ifndef MOSH_FCGI_USE_CGI
#include <mosh/fcgi/protocol/types.hpp>
#include <mosh/fcgi/protocol/full_id.hpp>
#include <mosh/fcgi/protocol/header.hpp>
#include <mosh/fcgi/protocol/vars.hpp>
#else
#include <mosh/fcgi/bits/array_deleter.hpp>
extern "C" {
#include <unistd.h>
}
#endif
#include <mosh/fcgi/fcgistream.hpp>
#include <mosh/fcgi/bits/iconv.hpp>
#include <mosh/fcgi/bits/iconv_cvt.hpp>
#include <mosh/fcgi/bits/namespace.hpp>

MOSH_FCGI_BEGIN

template <class char_type, class traits>
int Fcgistream<char_type, traits>::Fcgibuf::empty_buffer() {
	using namespace std;
	using namespace protocol;
	char_type const* p_stream_pos = this->pbase();
	while (1) {
		size_t count = this->pptr() - p_stream_pos;
		size_t wanted_size = count * sizeof(char_type) + dump_size;
		if (!wanted_size)
			break;
#ifndef MOSH_FCGI_USE_CGI
		int remainder = wanted_size % chunk_size;
		wanted_size += sizeof(Header) + (remainder ? (chunk_size - remainder) : remainder);
		if (wanted_size > numeric_limits<uint16_t>::max()) wanted_size = numeric_limits<uint16_t>::max();
		Block data_block(transceiver->request_write(wanted_size));
		data_block.size = (data_block.size / chunk_size) * chunk_size;
#else
		int remainder = 0;
		std::unique_ptr<char> data_buf(new char[wanted_size], Array_deleter<char>());
		Block data_block(data_buf.get(), wanted_size);
#endif
		locale loc = this->getloc();
#ifndef MOSH_FCGI_USE_CGI
		char* to_next = data_block.data + sizeof(Header);
#else
		char* to_next = data_block.data;
#endif
		if (count) {
			if (sizeof(char_type) != sizeof(char)) {
				Iconv::IC_state* i = ic.get();
				if (use_facet<codecvt<char_type, char, Iconv::IC_state*> >(loc)
				        .out(i, p_stream_pos, this->pptr(), p_stream_pos,
				             to_next, data_block.data + data_block.size, to_next)
				        == codecvt_base::error) {
					pbump(-(this->pptr() - this->pbase()));
					dump_size = 0;
					dump_ptr = 0;
#ifndef MOSH_FCGI_USE_CGI
					throw exceptions::Stream(id);
#else
					throw std::runtime_error(std::string("stream"));
#endif
				}
			} else {
#ifndef MOSH_FCGI_USE_CGI
				size_t cnt = min(data_block.size - sizeof(Header), count);
				memcpy(data_block.data + sizeof(Header), p_stream_pos, cnt);
#else
				size_t cnt = min(data_block.size, count);
#endif
				p_stream_pos += cnt;
				to_next += cnt;
			}
		}
		size_t dumped_size = min(dump_size, static_cast<size_t>(data_block.data + data_block.size - to_next));
		memcpy(to_next, dump_ptr, dumped_size);
		dump_ptr += dumped_size;
		dump_size -= dumped_size;
#ifndef MOSH_FCGI_USE_CGI
		uint16_t content_length = to_next - data_block.data + dumped_size - sizeof(Header);
		uint8_t content_remainder = content_length % chunk_size;
		Header& header = *(Header*)data_block.data;
		header.version() = version;
		header.type() = type;
		header.request_id() = id.fcgi_id;
		header.content_length() = content_length;
		header.padding_length() = content_remainder ? (chunk_size - content_remainder) : content_remainder;
		transceiver->secure_write(sizeof(Header) + content_length + header.padding_length(), id, false);
#else
		write(fd, data_block.data, data_block.size);
#endif
		
	}
	pbump(-(this->pptr() - this->pbase()));
	return 0;
}

template <class _char_type, class traits>
std::streamsize Fcgistream<_char_type, traits>::Fcgibuf::xsputn(const _char_type *s, std::streamsize n)
{
	std::streamsize remainder = n;
	while (remainder) {
		std::streamsize actual = std::min(remainder, this->epptr() - this->pptr());
		std::memcpy(this->pptr(), s, actual * sizeof(_char_type));
		this->pbump(actual);
		remainder -= actual;
		if (remainder) {
			s += actual;
			empty_buffer();
		}
	}

	return n;
}

template<class char_type, class traits > void Fcgistream<char_type, traits>::dump(std::basic_istream<char>& stream)
{
	const size_t buffer_size = 32768;
	char buffer[buffer_size];

	while (stream.good()) {
		stream.read(buffer, buffer_size);
		dump(buffer, stream.gcount());
	}
}

MOSH_FCGI_END

#endif
