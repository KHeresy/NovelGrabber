#pragma once

// STL Header
#include <map>
#include <string>

// Boost Header
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/signals2.hpp>

/**
 * Class to save all information about a HTML set
 */
class HTMLTag
{
public:
	std::string													m_sTagName;
	boost::optional<std::string>								m_sContent;
	std::map< std::string,boost::optional<std::string> >	m_mAttributes;

public:
	HTMLTag( const std::string& sTagName )
	{
		m_sTagName = sTagName;
	}

	HTMLTag( const std::string& sTagName, const std::string& sHtmlSource, size_t uBeginPos = 0 )
	{
		m_sTagName = sTagName;
		GetData( sHtmlSource, uBeginPos );
	}

	boost::optional< std::pair<size_t,size_t> > GetData( const std::string& sHtmlSource, size_t uBeginPos = 0 );

	boost::optional< std::pair<size_t,size_t> > FindQuoteContent( const std::string& rSourze, const char c, size_t uBeginPos = 0 );

public:
	static boost::optional< std::pair< HTMLTag, std::pair<size_t,size_t> > > Construct( const std::string& sTagName, std::string& sHtmlSource, size_t uBeginPos = 0 )
	{
		HTMLTag mTag( sTagName );
		boost::optional< std::pair<size_t,size_t> > pInfo = mTag.GetData( sHtmlSource, uBeginPos );
		if( pInfo )
			return make_pair( mTag, *pInfo );

		return boost::optional< std::pair< HTMLTag, std::pair<size_t,size_t> > >();
	}
};

class HTMLParser
{
public:
	typedef std::map< std::string,boost::optional<std::string> >	TAttributes;

public:
	static void FindTag( const std::string& rHtml, const std::string& rTag, const std::map< std::string, boost::optional<std::string> >& rAttribute, size_t uStartPos = 0 );
};

class HttpClient
{
public:
	boost::signals2::signal<void(const std::string&)>	m_sigErrorLog;
	boost::signals2::signal<void(const std::string&)>	m_sigInfoLog;

public:
	HttpClient();

	std::string ReadHtml( const std::string& rServer, const std::string& rPath );

	bool GetBinaryFile( const std::string& rServer, const std::string& rPath, const std::wstring& rFilename );

	static boost::optional< std::pair<std::string,std::string> > ParseURL( const std::string& sURL );

	static std::pair<size_t,std::string> FindContentBetweenTag( const std::string& rHtml, const std::pair<std::string,std::string>& rTag, size_t uStartPos = 0 );
	
	static boost::optional< std::pair<std::string,std::string> > AnalyzeLink( const std::string& rHtml, size_t uStartPos = 0 );
	

protected:
	boost::asio::io_service			m_IO_service;
	boost::asio::ip::tcp::resolver	m_Resolver;
};
