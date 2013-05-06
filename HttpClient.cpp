#include <sstream>

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
