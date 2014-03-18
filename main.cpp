#pragma region Header Files

// STL Header
#include <codecvt>
#include <fstream>
#include <map>
#include <iostream>
#include <regex>
#include <string>

#include <process.h>

// Boost Header
#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>
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

inline std::string CheckLink( const std::string& sUrl, const std::string& sPartent )
{
	if( sUrl.size() > 10 && sUrl.substr( 0, 7 ) == "http://" )
		return sUrl;
	
	if( sUrl.size() > 3 && sUrl[0] != '/' )
		return sPartent + "/" + sUrl;
	
	return sUrl;
}

inline std::wstring VertifyFilename( const std::wstring& sFilename )
{
	static wstring wsCHarMap1 = L"\\/:*\"|<>?!'";
	static wstring wsCHarMap2 = boost::locale::conv::utf_to_utf<wchar_t>( "\xEF\xBC\xBC\xEF\xBC\x8F\xEF\xBC\x9A\xEF\xBC\x8A\xEF\xBC\x82\xEF\xBD\x9C\xEF\xBC\x9C\xEF\xBC\x9E\xEF\xBC\x9F\xEF\xBC\x81\xE2\x80\x99" );

	std::wstring sNewName = sFilename;
	while( true )
	{
		size_t uPos = sNewName.find_first_of( wsCHarMap1 );
		if( uPos == wstring::npos )
			break;

		auto x1 = sNewName[uPos];
		auto x2 = wsCHarMap1.find( sNewName[uPos] );
		auto x3 = wsCHarMap1[ wsCHarMap1.find( sNewName[uPos] ) ];

		sNewName[uPos] = wsCHarMap2[ wsCHarMap1.find( sNewName[uPos] ) ];
	}
	return sNewName;
}

inline std::wstring ConvertSC2TC( const std::wstring& sText )
{
	static string	sOpenCC	= "Binary\\opencc\\opencc.exe -i \"%1%\" -o \"%2%\" -c zhs2zht.ini";

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

		try{
			system((boost::format(sOpenCC) % sFile1.c_str() % sFile2.c_str()).str().c_str());
		}
		catch (exception e)
		{
			cerr << e.what() << endl;
		}

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
	static string	sOpenCC		= "Binary\\opencc\\opencc.exe -i \"%1%\" -o \"%2%\" -c zhs2zhtw_p.ini";
	static string	sCalibre	= "Binary\\Calibre2\\ebook-convert.exe \"%1%\" \"%2%\"";

	string sTmpFile1 = GetTmpFileName();
	string sTmpFile2 = sTmpFile1 + ".tmp2";
	sTmpFile1 += ".tmp1";

	// rename original file first
	boost::filesystem::rename( sFile, sTmpFile1 );

	// execut OpenCC command
	system( ( boost::format( sOpenCC ) % sTmpFile1 % sTmpFile2 ).str().c_str() );

	// rename and remove file
	boost::filesystem::rename( sTmpFile2, sFile );
	boost::filesystem::remove( sTmpFile1 );

	// convert to mobi
	cout << ( boost::format( sCalibre ) % sFile % boost::filesystem::path(sFile).replace_extension( "mobi" ).string() ).str().c_str() << endl;
	system( ( boost::format( sCalibre ) % sFile % boost::filesystem::path(sFile).replace_extension( "mobi" ).string() ).str().c_str() );
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
	bool	bFileIndex;
	int		iRetryTimes = 100;
	string	sURL;
	string	sSearch;
	string	sReplace;
	string	sEncode;
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
			( "search",			BPO::value(&sSearch)->value_name("string")->default_value(""),		"Search text in book name, use with --replace")
			( "replace",		BPO::value(&sReplace)->value_name("string")->default_value(""),		"Replace search trem with, use with --search")
			( "encode",			BPO::value(&sEncode)->value_name("encode")->default_value("BIG5"),	"Encode of input argument, used for search/replace")
			( "file_index",		BPO::bool_switch(&bFileIndex)->default_value(false),				"Add file index at the begin of file name")
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

	function<wstring(wstring)> funcNameRefine = [](wstring s){ return VertifyFilename(s); };
	if (sSearch != "")
	{
		funcNameRefine = [&sSearch,&sReplace,&sEncode](wstring s){
			return VertifyFilename( regex_replace(s, wregex(boost::locale::conv::to_utf<wchar_t>(sSearch, sEncode)), boost::locale::conv::to_utf<wchar_t>(sReplace, sEncode)) );
		};
	}

	HttpClient mClient;
	auto mURL = HttpClient::ParseURL( sURL );
	if( mURL )
	{
		Wenku8Cn mSite;
		if( mSite.CheckServer( mURL->first ) )
		{
			boost::filesystem::path pathURL = boost::filesystem::path( sURL ).parent_path();

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
			g_sOutPath = sDir / VertifyFilename( ConvertSC2TC( vBooks.first ) );
			if( !boost::filesystem::exists( g_sOutPath ) )
				boost::filesystem::create_directories( g_sOutPath );
			if( !bNoDLImage && !boost::filesystem::exists( g_sOutPath / sImage ) )
				boost::filesystem::create_directories( g_sOutPath / sImage );
			wcout << "Output to " << g_sOutPath << endl;

			// write test
			int idxBook = 0;
			for( BookIndex& rBook : vBooks.second )
			{
				wstring sBookName = ConvertSC2TC( rBook.m_sTitle + L".html" );
				wcout << " Start process book <" << sBookName << ">, with " << rBook.m_vChapter.size() << " chapters" << endl;

				sBookName = funcNameRefine(sBookName);

				auto fnBook = g_sOutPath;
				if (bFileIndex)
				{
					++idxBook;
					fnBook /= ( boost::lexical_cast<wstring>(idxBook) + L"_" + sBookName );
				}
				else
				{
					fnBook /= sBookName;
				} 

				// if file existed
				bool bToDownload = true;
				if( !bOverWrite && boost::filesystem::exists( fnBook ) )
				{
					bToDownload = false;

					// read the index of existed html file
					wifstream fExistedFile( fnBook.wstring() );
					wstring sTmp;
					int iCounter = 0;
					while( !fExistedFile.eof() )
					{
						getline( fExistedFile, sTmp );
						if( sTmp == L"<NAV>" )
						{
							while( !fExistedFile.eof() )
							{
								getline( fExistedFile, sTmp );
								if( sTmp == L"</NAV>" )
									break;
								else
									++iCounter;
							}
							break;
						}
					}
					fExistedFile.close();
					
					// re-download if chapter number is not equal
					if( iCounter != rBook.m_vChapter.size() )
					{
						bToDownload = true;
						cout << "  Chapter number is not equal, redownload. ( " << iCounter << " != " << rBook.m_vChapter.size() << " )" << endl;
					}
				}

				if( bToDownload )
				{
					wofstream oFile( fnBook.wstring() );
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
							oFile << "<p><a href=\"#CH" << ++idxChapter << "\">" << rLink.first << "</a><!-- " << rLink.second << " --></p>\n";
						}
						oFile << "</NAV>\n";
	
						// content
						idxChapter = 0;
						for( auto& rLink : rBook.m_vChapter )
						{
							oFile << "<HR><A ID=\"CH" << ++idxChapter << "\" /><H4>" << rLink.first << "</H4>\n";
							wcout << "  > " << rLink.second << endl;
	
							auto sHTML = mClient.ReadHtml( CheckLink( SConv( rLink.second ), pathURL.string() ) );
							if( sHTML )
							{
								if( !bNoDLImage )
								{
									auto vImg = mSite.FindAllImage( *sHTML );
									if( vImg.size() > 0 )
									{
										cout << "      Found " << vImg.size() << " images\n";
										size_t uShift = 0;
										for( auto& rImg : vImg )
										{
											string sLink = CheckLink( SConv( rImg.second ), pathURL.string() );
											auto sFile = HttpClient::GetFilename( sLink );
											if( sFile )
											{
												cout << "        " << *sFile ;
												boost::filesystem::path sImagePath = sImage / *sFile;
												boost::filesystem::path sFileName = g_sOutPath / sImagePath;

												if( bOverWrite || !boost::filesystem::exists( sFileName ) )
												{
													int iTime = 0;
													bool bOK = false;
													while( ++iTime < iRetryTimes )
													{
														cout << "." << flush;
														bOK = mClient.GetBinaryFile( sLink, sFileName.wstring() );
														if( bOK )
															break;
													}
													if( bOK )
														cout << " OK" << endl;
													else
														cout << " Failed" << endl;
												}
												else
													cout << " skipped" << endl;
												sHTML->replace( uShift + rImg.first, rImg.second.size(), sImagePath.wstring() );
												uShift += sImagePath.wstring().size() - rImg.second.size();
											}
										}
										cout << endl;
									}
								}
								oFile << mSite.GetChapterContent( *sHTML );
							}
						}
						oFile << "</BODY></HTML>\n";
						oFile.close();
	
						cout << "  Convert from SC to TC" << endl;
						ExternCommand( fnBook.string() );
	
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
