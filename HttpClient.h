#pragma once

// STL Header
#include <map>
#include <string>

// Boost Header
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/signals2.hpp>

class HttpClient
{
public:
	boost::signals2::signal<void(const std::string&)>	m_sigErrorLog;
	boost::signals2::signal<void(const std::string&)>	m_sigInfoLog;

public:
	HttpClient();

	std::string ReadHtml( std::string sURL );

protected:
	static boost::optional< std::pair<std::string,std::string> > ParseURL( const std::string& sURL );

protected:
	boost::asio::io_service			m_IO_service;
	boost::asio::ip::tcp::resolver	m_Resolver;
};
