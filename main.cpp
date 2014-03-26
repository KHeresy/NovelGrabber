#pragma region Header Files

// STL Header
#include <codecvt>
#include <fstream>
#include <map>
#include <iostream>
#include <string>

// Boost Header
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/empty_deleter.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

// application header
#include "HttpClient.h"
#include "wenku8.cn.h"

#pragma endregion

using namespace std;
namespace FS = boost::filesystem;

// global object
locale		g_locUTF8( locale(""), new codecvt_utf8<wchar_t>() );
FS::path	g_sOutPath = FS::current_path();

wstring	g_sOpenCC	= L"\\opencc\\opencc.exe -i \"%1%\" -o \"%2%\" -c zhs2zht.ini";
wstring	g_sCalibre	= L"\\Calibre2\\ebook-convert.exe \"%1%\" \"%2%\"";


#pragma region wstring / string convetor
inline string SConv(const wstring& wsStr)
{
	return boost::locale::conv::utf_to_utf<char>(wsStr);
}

inline wstring SConv(const string& sStr)
{
	return boost::locale::conv::utf_to_utf<wchar_t>(sStr);
}
#pragma endregion

inline wstring GetTmpFileName()
{
	return FS::unique_path().wstring();
}

inline string CheckLink( const string& sUrl, const string& sPartent )
{
	if( sUrl.size() > 10 && sUrl.substr( 0, 7 ) == "http://" )
		return sUrl;
	
	if( sUrl.size() > 3 && sUrl[0] != '/' )
		return sPartent + "/" + sUrl;
	
	return sUrl;
}

inline wstring VertifyFilename( const wstring& sFilename )
{
	static wstring wsCHarMap1 = L"\\/:*\"|<>?!'";
	static wstring wsCHarMap2 = SConv("\xEF\xBC\xBC\xEF\xBC\x8F\xEF\xBC\x9A\xEF\xBC\x8A\xEF\xBC\x82\xEF\xBD\x9C\xEF\xBC\x9C\xEF\xBC\x9E\xEF\xBC\x9F\xEF\xBC\x81\xE2\x80\x99");

	wstring sNewName = sFilename;
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

inline bool SystemCommand( const wstring& rCmd )
{
	if (system(SConv(rCmd).c_str()))
	{
		BOOST_LOG_TRIVIAL(error) << "System call failed: " << rCmd;
		return false;
	}
	return true;
}

inline wstring ConvertSC2TC( const wstring& sText )
{
	wstring sFile1 = GetTmpFileName();
	wstring sFile2 = sFile1 + L".tmp2";
	sFile1 += L".tmp1";
	wstring sResult;

	wofstream oFile( sFile1 );
	if( oFile.is_open() )
	{
		oFile.imbue( g_locUTF8 );
		oFile << sText;
		oFile.close();

		if (SystemCommand((boost::wformat(g_sOpenCC) % sFile1 % sFile2).str()))
		{
			wifstream iFile(sFile2);
			if (iFile.is_open())
			{
				iFile.imbue(g_locUTF8);
				getline(iFile, sResult);
				iFile.close();
			}

			FS::remove(sFile1);
			FS::remove(sFile2);
		}
		else
		{
			BOOST_LOG_TRIVIAL(error) << "OpenCC Convert error! " << sText;
		}
	}
	return sResult;
}

inline void PostProcess( FS::path sFile)
{
	// name temp file
	wstring sTmpFile1 = GetTmpFileName();
	wstring sTmpFile2 = sTmpFile1 + L".html";
	sTmpFile1 += L".mobi";

	// rename original file first
	FS::rename( sFile, sTmpFile1 );

	// execut OpenCC command
	if (SystemCommand((boost::wformat(g_sOpenCC) % sTmpFile1 % sTmpFile2).str()))
	{
		BOOST_LOG_TRIVIAL(trace) << "OpenCC convert done";

		// remove temp file
		FS::remove(sTmpFile1);

		// get current codepage
		auto cp = GetConsoleOutputCP();

		// convert to mobi
		if (SystemCommand((boost::wformat(g_sCalibre) % sTmpFile2 % sTmpFile1).str()))
		{
			FS::rename(sTmpFile2, sFile );
			FS::rename(sTmpFile1, sFile.replace_extension("mobi") );
			BOOST_LOG_TRIVIAL(trace) << "Calibre convert done";
		}
		else
		{
			BOOST_LOG_TRIVIAL(error) << "Calibre convert error!";
		}

		// restore codepage
		SetConsoleOutputCP( cp );
	}
	else
	{
		BOOST_LOG_TRIVIAL(error) << "OpenCC Convert error!";
	}
}

int main(int argc, char* argv[])
{
	#pragma region Set locale for Chinese
	locale::global( g_locUTF8 );
	cout.imbue( g_locUTF8 );
	wcout.imbue( g_locUTF8 );
	clog.imbue(g_locUTF8);
	wclog.imbue(g_locUTF8);
	#pragma endregion

	#pragma region Variables
	bool	bNoDLImage;
	bool	bOverWrite;
	bool	bFileIndex;
	int		iRetryTimes;
	string	sURL;
	string	sSearch;
	string	sReplace;
	string	sEncode = "BIG5";
	FS::path	sExtBin = "Binary";
	FS::path	sDir;
	FS::path	sImage = "images";
	FS::path	sLogFile;
	#pragma endregion

	#pragma region Program Options
	{
		namespace BPO = boost::program_options;

		// define program options
		BPO::options_description bpoOptions( "Command Line Options" );
		bpoOptions.add_options()
			( "help,H",			BPO::bool_switch()->notifier( [&bpoOptions]( bool bH ){ if( bH ){ cout << bpoOptions << endl; exit(0); } } ),	"Help message" )
			( "url,U",			BPO::value(&sURL)->value_name("Web_Link"),							"The link of index page." )
			( "output,O",		BPO::value(&sDir)->value_name("output_dir")->default_value("."),	"Directory to save output files" )
			( "retry",			BPO::value(&iRetryTimes)->value_name("times")->default_value(100),	"HTTP retry times")
			( "log",			BPO::value(&sLogFile)->value_name("log_file"),						"Log")
			( "search",			BPO::value(&sSearch)->value_name("string")->default_value(""),		"Search text in book name, use with --replace")
			( "replace",		BPO::value(&sReplace)->value_name("string")->default_value(""),		"Replace search trem with, use with --search")
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
			cerr << e.what() << endl;
			cout << bpoOptions << endl;
			return 1;
		}
		catch( BPO::error e )
		{
			cerr << e.what() << endl;
			cout << bpoOptions << endl;
			return 1;
		}
		catch(exception e)
		{
			cerr << e.what() << endl;
			cout << bpoOptions << endl;
			return 2;
		}
	}
	#pragma endregion

	#pragma region Log System
	boost::shared_ptr< boost::log::core > core = boost::log::core::get();
	boost::shared_ptr< boost::log::sinks::text_ostream_backend > backend(new boost::log::sinks::text_ostream_backend());
	backend->add_stream(boost::shared_ptr< std::ostream >(&std::clog, boost::empty_deleter()));
	backend->auto_flush(true);

	if (!sLogFile.empty())
	{
		boost::shared_ptr< std::ostream > fLog(new std::ofstream(sLogFile.string(), std::ofstream::app));
		fLog->imbue(g_locUTF8);
		backend->add_stream(fLog);
	}

	typedef boost::log::sinks::synchronous_sink< boost::log::sinks::text_ostream_backend > sink_t;
	boost::shared_ptr< sink_t > sink(new sink_t(backend));
	sink->set_formatter(
		boost::log::expressions::stream << "[" << boost::log::trivial::severity << "]"
		<< "[" << boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %T") << "] "
		<< boost::log::expressions::smessage
	);
	core->add_sink(sink);

	// setup common attributes
	boost::log::add_common_attributes();
	BOOST_LOG_TRIVIAL(trace) << "Strat log system";
	#pragma endregion

	#pragma region configuration by options
	wstring sExtPath = ( FS::current_path() / sExtBin ).wstring();
	g_sOpenCC = sExtPath + g_sOpenCC;
	g_sCalibre = sExtPath + g_sCalibre;

	function<wstring(wstring)> funcNameRefine = [](wstring s){ return VertifyFilename(s); };
	if (sSearch != "")
	{
		funcNameRefine = [&sSearch, &sReplace, &sEncode](wstring s){
			wstring ss = VertifyFilename(s);
			boost::replace_all(ss, boost::locale::conv::to_utf<wchar_t>(sSearch, sEncode), boost::locale::conv::to_utf<wchar_t>(sReplace, sEncode));
			return ss;
		};
	}
	#pragma endregion

	try{
		HttpClient mClient;
		BOOST_LOG_TRIVIAL(info) << "Try to open: " << sURL;
		auto mURL = HttpClient::ParseURL(sURL);
		if (mURL)
		{
			Wenku8Cn mSite;
			if (mSite.CheckServer(mURL->first))
			{
				FS::path pathURL = FS::path(sURL).parent_path();

				int iTime = 0;
				boost::optional<wstring> rHtml;
				while (++iTime < iRetryTimes)
				{
					rHtml = mClient.ReadHtml(mURL->first, mURL->second);
					if (rHtml)
						break;
				}

				if (!rHtml)
				{
					BOOST_LOG_TRIVIAL(error) << "Can't open URL: " << sURL;
					return -1;
				}

				auto vBooks = mSite.AnalyzeIndexPage(*rHtml);
				if (vBooks.first == L"")
				{
					BOOST_LOG_TRIVIAL(error) << "Can't find book information";
					return -1;
				}
				wstring sBN = ConvertSC2TC(vBooks.first);
				BOOST_LOG_TRIVIAL(trace) << L"Strat to download novel: " << sBN;
				BOOST_LOG_TRIVIAL(info) << " >Found " << vBooks.second.size() << " books";

				// check directory
				g_sOutPath = sDir / VertifyFilename(sBN);
				if (!FS::exists(g_sOutPath))
					FS::create_directories(g_sOutPath);
				if (!bNoDLImage && !FS::exists(g_sOutPath / sImage))
					FS::create_directories(g_sOutPath / sImage);
				BOOST_LOG_TRIVIAL(trace) << "Output to " << g_sOutPath;

				// write
				int idxBook = 0;
				FS::current_path(g_sOutPath);
				for (BookIndex& rBook : vBooks.second)
				{
					wstring sBookName = ConvertSC2TC(rBook.m_sTitle + L".html");
					sBookName = funcNameRefine(sBookName);

					FS::path fnBook;
					if (bFileIndex)
					{
						++idxBook;
						fnBook /= (boost::lexical_cast<wstring>(idxBook)+L"_" + sBookName);
					}
					else
					{
						fnBook /= sBookName;
					}
					BOOST_LOG_TRIVIAL(trace) << " Start process book <" << fnBook << ">, with " << rBook.m_vChapter.size() << " chapters";

					// if file existed
					bool bToDownload = true;
					if (!bOverWrite && FS::exists(fnBook))
					{
						bToDownload = false;

						// read the index of existed html file
						wifstream fExistedFile(fnBook.wstring());
						wstring sTmp;
						int iCounter = 0;
						while (!fExistedFile.eof())
						{
							getline(fExistedFile, sTmp);
							if (sTmp == L"<NAV>")
							{
								while (!fExistedFile.eof())
								{
									getline(fExistedFile, sTmp);
									if (sTmp == L"</NAV>")
										break;
									else
										++iCounter;
								}
								break;
							}
						}
						fExistedFile.close();

						// re-download if chapter number is not equal
						if (iCounter != rBook.m_vChapter.size())
						{
							bToDownload = true;
							BOOST_LOG_TRIVIAL(info) << "  Chapter number is not equal, redownload. ( " << iCounter << " != " << rBook.m_vChapter.size() << " )";
						}
					}

					if (bToDownload)
					{
						wofstream oFile(fnBook.wstring());
						oFile.imbue(g_locUTF8);
						if (oFile.is_open())
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
							for (auto& rLink : rBook.m_vChapter)
							{
								oFile << "<p><a href=\"#CH" << ++idxChapter << "\">" << rLink.first << "</a><!-- " << rLink.second << " --></p>\n";
							}
							oFile << "</NAV>\n";

							// content
							idxChapter = 0;
							for (auto& rLink : rBook.m_vChapter)
							{
								oFile << "<HR><A ID=\"CH" << ++idxChapter << "\" /><H4>" << rLink.first << "</H4>\n";
								BOOST_LOG_TRIVIAL(trace) << "  > " << rLink.second;

								int iBTime = 0;
								boost::optional<wstring> sHTML;
								while (++iBTime < iRetryTimes)
								{
									sHTML = mClient.ReadHtml(CheckLink(SConv(rLink.second), pathURL.string()));
									if (sHTML)
										break;
								}
								if (sHTML)
								{
									if (!bNoDLImage)
									{
										auto vImg = mSite.FindAllImage(*sHTML);
										if (vImg.size() > 0)
										{
											cout << "      Found " << vImg.size() << " images\n";
											size_t uShift = 0;
											for (auto& rImg : vImg)
											{
												string sLink = CheckLink(SConv(rImg.second), pathURL.string());
												auto sFile = HttpClient::GetFilename(sLink);
												if (sFile)
												{
													BOOST_LOG_TRIVIAL(info) <<  "        " << *sFile;
													FS::path sImagePath = sImage / *sFile;

													if (bOverWrite || !FS::exists(sImagePath))
													{
														int iTime = 0;
														bool bOK = false;
														while (++iTime < iRetryTimes)
														{
															BOOST_LOG_TRIVIAL(trace) << ".";
															bOK = mClient.GetBinaryFile(sLink, sImagePath.wstring());
															if (bOK)
																break;
														}
														if (bOK)
															BOOST_LOG_TRIVIAL(trace) << "OK";
														else
															BOOST_LOG_TRIVIAL(error) << "Image <" << *sFile << "> download error.";
													}
													else
														BOOST_LOG_TRIVIAL(trace) << "SKIP";
													sHTML->replace(uShift + rImg.first, rImg.second.size(), sImagePath.wstring());
													uShift += sImagePath.wstring().size() - rImg.second.size();
												}
											}
											cout << endl;
										}
									}
									oFile << mSite.GetChapterContent(*sHTML);
								}
							}
							oFile << "</BODY></HTML>\n";
							oFile.close();

							BOOST_LOG_TRIVIAL(trace) << "  Start Convert";
							PostProcess(fnBook);
							BOOST_LOG_TRIVIAL(trace) << "  Book output finished";
						}
						else
						{
							BOOST_LOG_TRIVIAL(error) << " Can't open the file to output";
						}
					}
					else
					{
						BOOST_LOG_TRIVIAL(trace) << "  Skip this book.";
					}
				}
			}
		}
	}
	catch (exception e)
	{
		BOOST_LOG_TRIVIAL(fatal) << "Fatal error: " << e.what();
	}
	return 0;
}
