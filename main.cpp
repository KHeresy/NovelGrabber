#pragma region Header Files

// STL Header
#include <fstream>
#include <iostream>
#include <string>

// Boost Header
#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include "HttpClient.h"
#include "wenku8.cn.h"

#pragma endregion

using namespace std;

inline std::string toUTF8( const std::string& rS )
{
	return boost::locale::conv::to_utf<char>( rS, "GB2312" );
}

int main(int argc, char* argv[])
{
	string	sURL;
	string	sDir = ".";

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

	HttpClient mClient;
	auto mURL = HttpClient::ParseURL( sURL );
	if( mURL )
	{
		Wenku8Cn mSite;
		if( mSite.CheckServer( mURL->first ) )
		{
			string rHtml = mClient.ReadHtml( mURL->first, mURL->second );
			auto vBooks = mSite.AnalyzeIndexPage( rHtml );

			// write test
			for( BookIndex& rBook : vBooks )
			{
				wstring s = boost::locale::conv::to_utf<wchar_t>( rBook.m_sTitle, "GB2312" ) + L".html";
				ofstream oFile( s );
				if( oFile.is_open() )
				{
					oFile << "<HTML>\n";
					oFile << "<HEAD><META http-equiv=\"Content-Type\" content=\"text/html; charset=GBK\">\n";
					oFile << "<BODY>\n";
					oFile << "<H3 ALIGN=CENTER>" << rBook.m_sTitle << "</H3>\n";
					oFile << "<H4 ALIGN=CENTER>" << rBook.m_sAuthor << "</H4>\n";
					oFile << "<HR>\n";

					// index
					for( auto& rLink : rBook.m_vChapter )
					{
						oFile << "<div><a href=\"#" << &rLink << "\">" << rLink.first << "</a></siv>\n";
					}

					// content
					for( auto& rLink : rBook.m_vChapter )
					{
						oFile << "<A ID=\"" << &rLink << "\"><HR></A>\n";

						auto mCURL = HttpClient::ParseURL( rLink.second );
						if( mCURL )
						{
							oFile << mSite.GetChapterContent( mClient.ReadHtml( mCURL->first, mCURL->second ) );
						}
					}
					oFile << "</BODY></HTML>\n";
					oFile.close();
				}
			}
		}
	}
	return 0;
}
