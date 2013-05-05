#pragma once

// STL Header
#include <map>
#include <string>

// Boost Header
#include <boost/asio.hpp>
#include <boost/optional.hpp>

class HttpClient
{
public:
	HttpClient();

	bool Open( std::string sURL );

protected:
	static boost::optional< std::pair<std::string,std::string> > ParseURL( const std::string& sURL );

protected:
	boost::asio::io_service			m_IO_service;
	boost::asio::ip::tcp::resolver	m_Resolver;
};
