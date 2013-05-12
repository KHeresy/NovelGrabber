#pragma region Header Files

// STL Header
#include <chrono>
#include <codecvt>
#include <fstream>
#include <iostream>
#include <string>

#include <process.h>

// Boost Header
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/locale.hpp>
#include <boost/program_options.hpp>

// application header
#include "HttpClient.h"
#include "wenku8.cn.h"

#pragma endregion

using namespace std;

// global object
locale g_locUTF8( locale(""), new codecvt_utf8<wchar_t>() );

inline std::string GetTmpFileName()
{
	return ( boost::format( "%1%" ) % chrono::duration_cast<chrono::microseconds>( chrono::system_clock::now().time_since_epoch() ).count() ).str();
}

inline std::wstring toUTF8( const std::string& rS )
{
	return boost::locale::conv::to_utf<wchar_t>( rS, "GBK" );
}

inline std::wstring ConvertSC2TC( const std::wstring& sText )
{
	static string	sOpenCC	= "Binary\\opencc\\opencc.exe -i \"%1%\" -o \"%2%\" -c zhs2zhtw_p.ini";

	string sFile1 = GetTmpFileName();
	string sFile2 = sFile1 + ".tmp2";
	sFile1 += ".tmp1";
	wstring sResult;

	wofstream oFile( sFile1 );
	if( oFile.is_open() )
	{
		oFile.imbue( g_locUTF8 );
		oFile << sText;
		oFile.close();

		system( ( boost::format( sOpenCC ) % sFile1.c_str() % sFile2.c_str() ).str().c_str() );

		wifstream iFile( sFile2 );
		if( iFile.is_open() )
		{
			iFile.imbue( g_locUTF8 );
			getline( iFile, sResult );
			iFile.close();
		}

		boost::filesystem::remove( sFile1 );
		boost::filesystem::remove( sFile2 );
	}
	return sResult;
}

inline void ExternCommand( const wstring& sFile )
{
	static string	sOpenCC	= "Binary\\opencc\\opencc.exe -i \"%1%\" -o \"%2%\" -c zhs2zhtw_p.ini";

	string sTmpFile1 = GetTmpFileName();
	string sTmpFile2 = sTmpFile1 + ".tmp2";
	sTmpFile1 += ".tmp1";

	// rename original file first
	boost::filesystem::rename( sFile, sTmpFile1 );

	// execut OpenCC command
	system( ( boost::format( sOpenCC ) % sTmpFile1.c_str() % sTmpFile2.c_str() ).str().c_str() );

	// rename and remove file
	boost::filesystem::rename( sTmpFile2, sFile );
	boost::filesystem::remove( sTmpFile1 );
}

int main(int argc, char* argv[])
{
	locale::global( g_locUTF8 );
	cout.imbue( g_locUTF8 );
	wcout.imbue( g_locUTF8 );

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
			boost::optional<string> rHtml = mClient.ReadHtml( mURL->first, mURL->second );
			auto vBooks = mSite.AnalyzeIndexPage( *rHtml );
			cout << "Found " << vBooks.size() << " books" << endl;

			// write test
			for( BookIndex& rBook : vBooks )
			{
				wstring sBookName = ConvertSC2TC( boost::locale::conv::to_utf<wchar_t>( rBook.m_sTitle + ".html", "GB2312" ) );
				wcout << " Start process book <" << sBookName << ">, with " << rBook.m_vChapter.size() << " chapters" << endl;

				wofstream oFile( sBookName );
				oFile.imbue( g_locUTF8 );
				if( oFile.is_open() )
				{
					oFile << "<HTML>\n";
					oFile << "<HEAD><META http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
					oFile << "<BODY>\n";
					oFile << "<H3 ALIGN=CENTER>" << toUTF8( rBook.m_sTitle ) << "</H3>\n";
					oFile << "<H4 ALIGN=CENTER>" << toUTF8( rBook.m_sAuthor ) << "</H4>\n";

					// index
					oFile << "<A ID=\"INDEX\"><HR></A>\n";
					for( auto& rLink : rBook.m_vChapter )
					{
						oFile << "<div><a href=\"#" << &rLink << "\">" << toUTF8( rLink.first ) << "</a></siv>\n";
					}

					// content
					for( auto& rLink : rBook.m_vChapter )
					{
						oFile << "<HR><H4><A ID=\"" << &rLink << "\">" << toUTF8( rLink.first ) << "</A></H4>\n";
						cout << "  > " << rLink.second << endl;

						auto sHTML = mClient.ReadHtml( rLink.second );
						if( sHTML )
						{
							oFile << toUTF8( mSite.GetChapterContent( *sHTML ) );
						}
					}
					oFile << "</BODY></HTML>\n";
					oFile.close();

					cout << "  Convert from SC to TC" << endl;
					ExternCommand( sBookName );

					cout << "  Book output finished" << endl; 
				}
				else
				{
					 cerr << " Can't open the file to output" << endl;
				}
			}
		}
	}
	return 0;
}
