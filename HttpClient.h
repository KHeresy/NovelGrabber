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

	std::string ReadHtml( const std::string& rServer, const std::string& rPath );

	static boost::optional< std::pair<std::string,std::string> > ParseURL( const std::string& sURL );

	static std::pair<size_t,std::string> FindContentBetweenTag( const std::string& rHtml, const std::pair<std::string,std::string>& rTag, size_t uStartPos = 0 );
	
	static boost::optional< std::pair<std::string,std::string> > AnalyzeLink( const std::string& rHtml, size_t uStartPos = 0 );


protected:
	boost::asio::io_service			m_IO_service;
	boost::asio::ip::tcp::resolver	m_Resolver;
};
