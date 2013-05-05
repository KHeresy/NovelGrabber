#pragma region Header Files
// STL Header
#include <map>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>

// Boost Header
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#pragma endregion

using namespace std;

int main(int argc, char* argv[])
{
	string	sURL;
	string	sDir;
	pair<string,string>	sWebPage;

	#pragma region Program Options
	{
		namespace BPO = boost::program_options;

		// define program options
		BPO::options_description bpoOptions( "Command Line Options" );
		bpoOptions.add_options()
			( "help,H",		BPO::bool_switch()->notifier( [&bpoOptions]( bool bH ){ if( bH ){ std::cout << bpoOptions << std::endl; exit(0); } } ),	"Help message" )
			( "url,U",		BPO::value(&sURL)->value_name("Web_Link"),		"The link of index page." )
			( "output,O",	BPO::value(&sDir)->value_name("output_dir"),	"Directory to save output files" );

		// prase
		try
		{
			BPO::variables_map mVMap;
			BPO::store( BPO::command_line_parser( argc, argv ).options( bpoOptions ).allow_unregistered().run(), mVMap );
			BPO::notify( mVMap );
		}
		catch( BPO::error_with_option_name e )
		{
			std::cerr << e.what() << std::endl;
			std::cout << bpoOptions << std::endl;
			return 1;
		}
		catch( BPO::error e )
		{
			std::cerr << e.what() << std::endl;
			std::cout << bpoOptions << std::endl;
			return 1;
		}
		catch(std::exception e)
		{
			std::cerr << e.what() << std::endl;
			std::cout << bpoOptions << std::endl;
			return 2;
		}
	}
	#pragma endregion

	#pragma region Anaylaze URL
	if( sURL.substr( 0, 7 ) == "http://" )
	{
		auto uPos = sURL.find_first_of( "/", 8 );
		sWebPage.first = sURL.substr( 7, uPos - 7 );
		sWebPage.second = sURL.substr( uPos );
		cout << "Try to open server: " << sWebPage.first << "\n  path: " << sWebPage.second << endl;
	}
	else
	{
		cerr << "Please provide a URL begin with \"http://\"" << endl;
		return -1;
	}
	#pragma endregion

	try
	{
		using boost::asio::ip::tcp;
		boost::asio::io_service io_service;

		// Get a list of endpoints corresponding to the server name.
		tcp::resolver resolver( io_service );
		tcp::resolver::query query( sWebPage.first, "http" );
		tcp::resolver::iterator endpoint_iterator = resolver.resolve( query );

		// Try each endpoint until we successfully establish a connection.
		tcp::socket socket(io_service);
		boost::asio::connect(socket, endpoint_iterator);

		// Form the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		boost::asio::streambuf request;
		std::ostream request_stream(&request);
		request_stream << "GET " << sWebPage.second << " HTTP/1.0\r\n";
		request_stream << "Host: " << sWebPage.first << "\r\n";
		request_stream << "Accept: */*\r\n";
		request_stream << "Connection: close\r\n\r\n";

		// Send the request.
		boost::asio::write(socket, request);

		// Read the response status line. The response streambuf will automatically
		// grow to accommodate the entire line. The growth may be limited by passing
		// a maximum size to the streambuf constructor.
		boost::asio::streambuf response;
		boost::asio::read_until(socket, response, "\r\n");

		// Check that response is OK.
		std::istream response_stream(&response);
		std::string http_version;
		response_stream >> http_version;
		unsigned int status_code;
		response_stream >> status_code;
		std::string status_message;
		std::getline(response_stream, status_message);
		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
			std::cout << "Invalid response\n";
			return 1;
		}
		if (status_code != 200)
		{
			std::cout << "Response returned with status code " << status_code << "\n";
			return 1;
		}

		// Read the response headers, which are terminated by a blank line.
		boost::asio::read_until(socket, response, "\r\n\r\n");

		std::cout.imbue( std::locale("Chinese_China.936") );

		// Process the response headers.
		std::string header;
		while (std::getline(response_stream, header) && header != "\r")
			std::cout << header << "\n";
		std::cout << "\n";

		// Write whatever content we already have to output.
		if (response.size() > 0)
			std::cout << &response;

		// Read until EOF, writing data to output as we go.
		boost::system::error_code error;
		while (boost::asio::read(socket, response, boost::asio::transfer_at_least(1), error))
			std::cout << &response;
		if (error != boost::asio::error::eof)
			throw boost::system::system_error(error);
	}
	catch (std::exception& e)
	{
		std::cout << "Exception: " << e.what() << "\n";
	}
	return 0;
}
