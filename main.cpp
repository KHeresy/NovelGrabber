#pragma region Header Files

// STL Header
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
boost::filesystem::path g_sOutPath = boost::filesystem::current_path();

inline std::string GetTmpFileName()
{
	return boost::filesystem::unique_path().string();
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

inline void ExternCommand( const string& sFile )
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

inline string SConv( const wstring& wsStr )
{
	return boost::locale::conv::utf_to_utf<char>( wsStr );
}

inline wstring SConv( const string& sStr )
{
	return boost::locale::conv::utf_to_utf<wchar_t>( sStr );
}

int main(int argc, char* argv[])
{
	// Set locale for Chinese
	locale::global( g_locUTF8 );
	cout.imbue( g_locUTF8 );
	wcout.imbue( g_locUTF8 );

	// some variables
	bool	bNoDLImage;
	bool	bOverWrite;
	string	sURL;
	boost::filesystem::path	sDir;
	boost::filesystem::path	sImage = "images";

	#pragma region Program Options
	{
		namespace BPO = boost::program_options;

		// define program options
		BPO::options_description bpoOptions( "Command Line Options" );
		bpoOptions.add_options()
			( "help,H",			BPO::bool_switch()->notifier( [&bpoOptions]( bool bH ){ if( bH ){ std::cout << bpoOptions << std::endl; exit(0); } } ),	"Help message" )
			( "url,U",			BPO::value(&sURL)->value_name("Web_Link"),							"The link of index page." )
			( "output,O",		BPO::value(&sDir)->value_name("output_dir")->default_value("."),	"Directory to save output files" )
			( "no_dl_image",	BPO::bool_switch(&bNoDLImage)->default_value(false),				"Not download image" )
			( "no_overwrite",	BPO::bool_switch(&bOverWrite)->default_value(false),				"Overwrite existed files" );

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
			boost::optional<wstring> rHtml = mClient.ReadHtml( mURL->first, mURL->second );
			auto vBooks = mSite.AnalyzeIndexPage( *rHtml );
			if( vBooks.first == L"" )
			{
				cerr << "Can't find book information" << endl;
				return -1;
			}
			wcout << L"Strat to download novel: " << vBooks.first << "\n";
			cout << " >Found " << vBooks.second.size() << " books" << endl;

			// check directory
			g_sOutPath = sDir;
			if( !boost::filesystem::exists( g_sOutPath ) )
				boost::filesystem::create_directories( g_sOutPath );
			if( !bNoDLImage && !boost::filesystem::exists( g_sOutPath / sImage ) )
				boost::filesystem::create_directories( g_sOutPath / sImage );
			wcout << "Output to " << g_sOutPath << endl;

			// write test
			for( BookIndex& rBook : vBooks.second )
			{
				wstring sBookName = ConvertSC2TC( rBook.m_sTitle + L".html" );
				wcout << " Start process book <" << sBookName << ">, with " << rBook.m_vChapter.size() << " chapters" << endl;

				if( bOverWrite || !boost::filesystem::exists( ( g_sOutPath / sBookName ).string() ) )
				{
					wofstream oFile( ( g_sOutPath / sBookName ).string() );
					oFile.imbue( g_locUTF8 );
					if( oFile.is_open() )
					{
						oFile << "<HTML>\n";
						oFile << "<HEAD>\n<META http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
						oFile << "<TITLE>" << rBook.m_sTitle << "</TITLE>\n";
						oFile << "<META name=\"Author\" content=\"" << rBook.m_sAuthor << "\">\n</HEAD>\n";
						oFile << "<BODY>\n";
						oFile << "<H3 ALIGN=\"CENTER\">" << rBook.m_sTitle << "</H3>\n";
						oFile << "<H4 ALIGN=\"CENTER\">" << rBook.m_sAuthor << "</H4>\n";
	
						// index
						oFile << "<A ID=\"INDEX\" /><HR>\n<NAV>\n";
						size_t idxChapter = 0;
						for( auto& rLink : rBook.m_vChapter )
						{
							oFile << "<p><a href=\"#CH" << ++idxChapter << "\">" << rLink.first << "</a></p>\n";
						}
						oFile << "</NAV>\n";
	
						// content
						idxChapter = 0;
						for( auto& rLink : rBook.m_vChapter )
						{
							oFile << "<HR><A ID=\"CH" << ++idxChapter << "\" /><H4>" << rLink.first << "</H4>\n";
							wcout << "  > " << rLink.second << endl;
	
							auto sHTML = mClient.ReadHtml( SConv( rLink.second ) );
							if( sHTML )
							{
								if( !bNoDLImage )
								{
									auto vImg = mSite.FindAllImage( *sHTML );
									if( vImg.size() > 0 )
									{
										cout << "      Found " << vImg.size() << " images" << endl;
										size_t uShift = 0;
										for( auto& rImg : vImg )
										{
											auto sFile = HttpClient::GetFilename( SConv( rImg.second ) );
											if( sFile )
											{
												boost::filesystem::path sFileName = sImage / *sFile;
												wstring sFilename = SConv( ( g_sOutPath / sFileName ).string() );
												if( bOverWrite || !boost::filesystem::exists( sFilename ) )
												{
													mClient.GetBinaryFile( SConv( rImg.second ), sFilename );
												}
												sHTML->replace( uShift + rImg.first, rImg.second.size(), SConv( sFileName.string() ) );
												uShift += sFileName.string().size() - rImg.second.size();
											}
										}
									}
								}
								oFile << mSite.GetChapterContent( *sHTML );
							}
						}
						oFile << "</BODY></HTML>\n";
						oFile.close();
	
						cout << "  Convert from SC to TC" << endl;
						ExternCommand( ( g_sOutPath / sBookName ).string() );
	
						cout << "  Book output finished" << endl; 
					}
					else
					{
						 cerr << " Can't open the file to output" << endl;
					}
				}
			}
		}
	}
	return 0;
}
