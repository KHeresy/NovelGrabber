#include <fstream>
#include <sstream>

#include <boost/algorithm/string.hpp>

#include "HttpClient.h"

using namespace std;
using namespace boost::asio::ip;

HttpClient::HttpClient() : m_Resolver( m_IO_service )
{
}

string HttpClient::ReadHtml( const string& rServer, const string& rPath )
{
	// code reference to http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/example/iostreams/http_client.cpp
	tcp::iostream sStream;
	sStream.expires_from_now( boost::posix_time::seconds( 60 ) );

	#pragma region Send request
	// Establish a connection to the server.
	m_sigInfoLog( "Connect to " + rServer );
	sStream.connect( rServer, "http" );
	if( !sStream )
	{
		m_sigErrorLog( "Unable to connect: " + sStream.error().message() );
		return "";
	}

	// Send the request.
	m_sigInfoLog( "Request paget " + rPath );
	sStream << "GET " << rPath << " HTTP/1.0\r\n";
	sStream << "Host: " << rServer << "\r\n";
	sStream << "Accept: */*\r\n" << "Connection: close\r\n\r\n";
	#pragma endregion

	#pragma region recive data
	// Check that response is OK.
	string sHttpVersion;
	sStream >> sHttpVersion;
	unsigned int uCode;
	sStream >> uCode;
	string sMssage;
	getline( sStream, sMssage );
	m_sigInfoLog( "Recive data: " + sHttpVersion + " / " + sMssage );
	if( !sStream || sHttpVersion.substr(0, 5) != "HTTP/" )
	{
		m_sigErrorLog( "Invalid response" );
		return "";
	}
	if( uCode != 200 )
	{
		m_sigErrorLog( "Response returned with status code " + uCode );
		return "";
	}

	// Process the response headers, which are terminated by a blank line.
	string header;
	while( getline( sStream, header ) && header != "\r" )
		m_sigInfoLog( header );

	// Write the remaining data to output.
	stringstream oStream;
	oStream << sStream.rdbuf();
	return oStream.str();
	#pragma endregion
}

bool HttpClient::GetBinaryFile( const string& rServer, const string& rPath, const wstring& rFilename )
{
	// code reference to http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/example/iostreams/http_client.cpp
	tcp::iostream sStream;
	sStream.expires_from_now( boost::posix_time::seconds( 60 ) );

	#pragma region Send request
	// Establish a connection to the server.
	m_sigInfoLog( "Connect to " + rServer );
	sStream.connect( rServer, "http" );
	if( !sStream )
	{
		m_sigErrorLog( "Unable to connect: " + sStream.error().message() );
		return "";
	}

	// Send the request.
	m_sigInfoLog( "Request paget " + rPath );
	sStream << "GET " << rPath << " HTTP/1.0\r\n";
	sStream << "Host: " << rServer << "\r\n";
	sStream << "Accept: */*\r\n" << "Connection: close\r\n\r\n";
	#pragma endregion

	#pragma region recive data
	// Check that response is OK.
	string sHttpVersion;
	sStream >> sHttpVersion;
	unsigned int uCode;
	sStream >> uCode;
	string sMssage;
	getline( sStream, sMssage );
	m_sigInfoLog( "Recive data: " + sHttpVersion + " / " + sMssage );
	if( !sStream || sHttpVersion.substr(0, 5) != "HTTP/" )
	{
		m_sigErrorLog( "Invalid response" );
		return "";
	}
	if( uCode != 200 )
	{
		m_sigErrorLog( "Response returned with status code " + uCode );
		return "";
	}

	// Process the response headers, which are terminated by a blank line.
	string header;
	while( getline( sStream, header ) && header != "\r" )
		m_sigInfoLog( header );

	std::ofstream outfile( rFilename );
	auto buf = sStream.rdbuf();
	outfile << buf;
	outfile.close();

	#pragma endregion
}

boost::optional< pair<string,string> > HttpClient::ParseURL( const string& sURL )
{
	if( sURL.length() > 10 )
	{
		if( sURL.substr( 0, 7 ) == "http://" )
		{
			auto uPos = sURL.find_first_of( "/", 8 );
			return boost::optional< pair<string,string> >( make_pair( sURL.substr( 7, uPos - 7 ), sURL.substr( uPos ) ) );
		}
	}
	return boost::optional< pair<string,string> >();
}

void HttpClient::FindTag( const std::string& rHtml, const std::string& rTag, const std::map< std::string, boost::optional<std::string> >& rAttribute, size_t uStartPos )
{
	std::string sTagBegin	= "<" + rTag,
				sTagEnd		= "</" + rTag + ">";
	
	// find the position of tag
	size_t uPos1 = rHtml.find( sTagBegin, uStartPos );
	if( uPos1 == std::string::npos )
	{
		return;
	}
	
	// find the end of the start tag
	size_t uPos2 = rHtml.find( ">", uPos1 + sTagBegin.length() );
	if( uPos2 == std::string::npos )
	{
		return;
	}

	if( rAttribute.size() > 0 )
	{
		// get attributes
		std::string sAttributes = rHtml.substr( uPos1 + sTagBegin.length(), uPos2 - ( uPos1 + sTagBegin.length() ) );

		// TODO: check attribute
	}

	// TODO: check nested tag?

	// find close tag
	size_t uPos3 = rHtml.find( sTagEnd, uPos2 + 1 );
	if( uPos3 != std::string::npos )
	{
		std::string sContent = rHtml.substr( uPos2 + 1, uPos3 - uPos2 - 1 );
	}
}

std::pair<size_t,string> HttpClient::FindContentBetweenTag( const string& rHtml, const pair<string,string>& rTag, size_t uStartPos )
{
	size_t uPos1 = rHtml.find( rTag.first, uStartPos );
	if( uPos1 != string::npos )
	{
		size_t uPos2 = rHtml.find( rTag.second, uPos1 );
		uPos1 = uPos1 + rTag.first.length();

		std::string sContent = rHtml.substr( uPos1, uPos2 - uPos1 );
		boost::algorithm::trim( sContent );
		return make_pair( uPos1, sContent );
	}
	return make_pair( string::npos, "" );
}

boost::optional< pair<string,string> > HttpClient::AnalyzeLink( const std::string& rHtml, size_t uStartPos )
{
	size_t uPos1 = rHtml.find( "<a ", uStartPos );
	if( uPos1 != string::npos )
	{
		size_t uPos2 = rHtml.find( "href=\"", uPos1 );
		if( uPos2 != string::npos )
		{
			uPos2 += 6;
			uPos1 = rHtml.find( "\"", uPos2 );
			if( uPos1 != string::npos )
			{
				string sLinkUrl = rHtml.substr( uPos2, uPos1 - uPos2 );

				uPos1 = rHtml.find( ">", uPos1 );
				if( uPos1 != string::npos )
				{
					uPos1 += 1;
					uPos2 = rHtml.find( "</a>", uPos1 );
					string sLinkText = rHtml.substr( uPos1, uPos2 - uPos1 );
					boost::algorithm::trim( sLinkText );

					return boost::optional< pair<string,string> >( make_pair( sLinkText, sLinkUrl ) );
				}
			}
		}
	}
	return boost::optional< pair<string,string> >();
}
