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
		#pragma region Send request
		// Get a list of endpoints corresponding to the server name.
		tcp::resolver::query query( mURL->first, "http" );
		tcp::resolver::iterator endpoint_iterator = m_Resolver.resolve( query );

		// Try each endpoint until we successfully establish a connection.
		tcp::socket socket( m_IO_service );
		boost::asio::connect( socket, endpoint_iterator );
		
		// Form the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		boost::asio::streambuf request;
		ostream request_stream( &request );
		request_stream << "GET " << mURL->second << " HTTP/1.0\r\n";
		request_stream << "Host: " << mURL->first << "\r\n";
		request_stream << "Accept: */*\r\n";
		request_stream << "Connection: close\r\n\r\n";

		// Send the request.
		boost::asio::write(socket, request);
		#pragma endregion

		#pragma region Read response
		// Read the response status line. The response streambuf will automatically
		// grow to accommodate the entire line. The growth may be limited by passing
		// a maximum size to the streambuf constructor.
		boost::asio::streambuf response;
		boost::asio::read_until( socket, response, "\r\n" );

		// Check that response is OK.
		istream response_stream( &response );
		string http_version;
		response_stream >> http_version;
		unsigned int status_code;
		response_stream >> status_code;
		string status_message;
		getline( response_stream, status_message );
		if( response_stream && http_version.substr( 0, 5 ) == "HTTP/" )
		{
			if( status_code == 200 )
			{
				// Read the response headers, which are terminated by a blank line.
				boost::asio::read_until(socket, response, "\r\n\r\n");

				// Process the response headers.
				string header;
				while( std::getline(response_stream, header) && header != "\r" )
					std::cout << header << "\n";
				std::cout << "\n";

				// Write whatever content we already have to output.
				if( response.size() > 0 )
					std::cout << &response;

				// Read until EOF, writing data to output as we go.
				boost::system::error_code error;
				while( boost::asio::read( socket, response, boost::asio::transfer_at_least(1), error ) )
					std::cout << &response;

				if( error != boost::asio::error::eof )
					throw boost::system::system_error( error );
			}
		}
		#pragma endregion
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
