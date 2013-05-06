#include <iostream>

#include "HttpClient.h"

using namespace std;
using namespace boost::asio::ip;

HttpClient::HttpClient() : m_Resolver( m_IO_service )
{
}

bool HttpClient::Open( std::string sURL )
{
	auto mURL = ParseURL( sURL );
	if( mURL )
	{
		// code reference to http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/example/iostreams/http_client.cpp
		tcp::iostream sStream;
		sStream.expires_from_now( boost::posix_time::seconds( 60 ) );

		// Establish a connection to the server.
		sStream.connect( mURL->first, "http" );
		if( !sStream )
		{
			std::cout << "Unable to connect: " << sStream.error().message() << "\n";
			return false;
		}

		// Send the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		sStream << "GET " << mURL->second << " HTTP/1.0\r\n";
		sStream << "Host: " << mURL->first << "\r\n";
		sStream << "Accept: */*\r\n" << "Connection: close\r\n\r\n";

		// By default, the stream is tied with itself. This means that the stream
		// automatically flush the buffered output before attempting a read. It is
		// not necessary not explicitly flush the stream at this point.

		// Check that response is OK.
		string http_version;
		sStream >> http_version;
		unsigned int status_code;
		sStream >> status_code;
		string status_message;
		getline( sStream, status_message );
		if( !sStream || http_version.substr(0, 5) != "HTTP/" )
		{
			cout << "Invalid response\n";
			return 1;
		}
		if( status_code != 200 )
		{
			cout << "Response returned with status code " << status_code << "\n";
			return 1;
		}

		// Process the response headers, which are terminated by a blank line.
		string header;
		while( getline( sStream, header ) && header != "\r" )
			cout << header << "\n";
		cout << "\n";

		// Write the remaining data to output.
		cout << sStream.rdbuf();
		return true;
	}
	return false;
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
