#pragma region Header Files

// STL Header
#include <fstream>
#include <iostream>
#include <string>

#include <process.h>

// Boost Header
#include <boost/format.hpp>
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

inline void ExternCommand( const wstring& sFile, const wstring& sTmpFile )
{
	wstring	sIconv	= L"Binary\\libiconv\\iconv.exe -f GBK -t UTF-8 \"%1%\" > \"%2%\"",
			sOpenCC	= L"Binary\\opencc\\opencc.exe -i \"%1%\" -o \"%2%\" -c zhs2zhtw_p.ini";
	_wsystem( ( boost::wformat( sIconv ) % sFile % sTmpFile ).str().c_str() );
	_wsystem( ( boost::wformat( sOpenCC ) % sTmpFile % sFile ).str().c_str() );
}

inline void ExternCommand( const string& sFile, const string& sTmpFile )
{
	string	sIconv	= "Binary\\libiconv\\iconv.exe -f GBK -t UTF-8 \"%1%\" > \"%2%\"",
			sOpenCC	= "Binary\\opencc\\opencc.exe -i \"%1%\" -o \"%2%\" -c zhs2zhtw_p.ini";
	system( ( boost::format( sIconv ) % sFile % sTmpFile ).str().c_str() );
	system( ( boost::format( sOpenCC ) % sTmpFile % sFile ).str().c_str() );
}

int main(int argc, char* argv[])
{
	std::locale::global( std::locale("Chinese_China") );

	string sContent = "<HTML><BODY><A target=_blank xxx href=\"x\"><IMG src=\"x\"></A></BODY></HTML>";
	auto pRes = HTMLTag::Construct( "A", sContent );


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
				wstring sBookName = boost::locale::conv::to_utf<wchar_t>( rBook.m_sTitle + ".html", "GB2312" );
				cout << " Start process book <" << sBookName.c_str() << ">, with " << rBook.m_vChapter.size() << " chapters" << endl;

				ofstream oFile( sBookName );
				if( oFile.is_open() )
				{
					oFile << "<HTML>\n";
					oFile << "<HEAD><META http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
					oFile << "<BODY>\n";
					oFile << "<H3 ALIGN=CENTER>" << rBook.m_sTitle << "</H3>\n";
					oFile << "<H4 ALIGN=CENTER>" << rBook.m_sAuthor << "</H4>\n";
					oFile << "<HR>\n";

					// index
					oFile << "<A ID=\"INDEX\"><HR></A>\n";
					for( auto& rLink : rBook.m_vChapter )
					{
						oFile << "<div><a href=\"#" << &rLink << "\">" << rLink.first << "</a></siv>\n";
					}

					// content
					for( auto& rLink : rBook.m_vChapter )
					{
						oFile << "<HR><H4><A ID=\"" << &rLink << "\">" << rLink.first << "</A></H4>\n";

						auto mCURL = HttpClient::ParseURL( rLink.second );
						if( mCURL )
						{
							oFile << mSite.GetChapterContent( *(mClient.ReadHtml( mCURL->first, mCURL->second ) ) );
						}
					}
					oFile << "</BODY></HTML>\n";
					oFile.close();

					cout << " Convert HTML to UTF-8 TC" << endl;
					ExternCommand( sBookName, L"tmp" );

					cout << " Book output finished" << endl; 
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
