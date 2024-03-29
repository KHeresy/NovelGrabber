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
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/core/null_deleter.hpp>

// application header
#include "HttpClient.h"
#include "wenku8.cn.h"

// opencc
#include <opencc.h>
#pragma endregion

using namespace std;
using namespace HttpClientLite;
namespace FS = boost::filesystem;

// global object
locale		g_locUTF8( locale(""), new codecvt_utf8<wchar_t>() );
FS::path	g_sOutPath = FS::current_path();
opencc::SimpleConverter*	g_pOpenCC = nullptr;

const wstring	g_sTargetFormat = L"epub";
wstring	g_sCalibre	= L"\\Calibre2\\ebook-convert.exe \"%1%\" \"%2%\"";
wstring	g_sKepubify = L"\\kepubify.exe \"%1%\"";

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
	if (_wsystem(rCmd.c_str()))
	{
		BOOST_LOG_TRIVIAL(error) << "System call failed: " << rCmd;
		return false;
	}
	return true;
}

inline wstring GenCalibreCommand( const wstring& sInputFiile, const wstring& sOuputFiile, const wstring& sCover, const wstring& wSeriesName)
{
	wstring sCmd = (boost::wformat(g_sCalibre) % sInputFiile % sOuputFiile).str();
	if (sCover != L"")
		sCmd += (L" --no-svg-cove --cover \"" + sCover + L"\"");
	else
		sCmd += L" --no-default-epub-cover";

	if (wSeriesName != L"")
		sCmd += L" --series \"" + wSeriesName + L"\"";

	return sCmd;
}

inline void PostProcess(FS::path sFile, wstring sCoverImg, const wstring& sBookName)
{
	// name temp file
	wstring sSourceFile = sFile.wstring();
	wstring sTargetFile = sFile.replace_extension(g_sTargetFormat).wstring();
	wstring sTmpTarget = (sFile.stem().wstring() + L"_converted.kepub." + g_sTargetFormat);
	wstring sFinalTarget = (sFile.stem().wstring() + L".kepub." + g_sTargetFormat);

	try {
		// get current codepage
		auto cp = GetConsoleOutputCP();

		// convert to e-book
		if (SystemCommand(GenCalibreCommand(sSourceFile, sTargetFile, (sFile.parent_path() / sCoverImg).wstring(), sBookName)))
		{
			BOOST_LOG_TRIVIAL(trace) << "Calibre convert done";

			if (SystemCommand((boost::wformat(g_sKepubify) % sTargetFile).str()))
			{
				FS::remove(sTargetFile);
				FS::rename(sTmpTarget, sFinalTarget);
				BOOST_LOG_TRIVIAL(trace) << "Kepubify convert done";
			}
			else
			{
				BOOST_LOG_TRIVIAL(error) << "Kepubify convert error!";
			}
		}
		else
		{
			BOOST_LOG_TRIVIAL(error) << "Calibre convert error!";
		}

		// restore codepage
		SetConsoleOutputCP(cp);
	}
	catch (std::exception e)
	{
		BOOST_LOG_TRIVIAL(error) << e.what();
	}
}

inline wstring ConvertS2T(const wstring& rInput )
{
	try
	{
		return SConv(g_pOpenCC->Convert(SConv(rInput)));
	}
	catch(exception e)
	{
		BOOST_LOG_TRIVIAL(error) << "OpenCC Convert error. " << e.what();
	}
	return L"";
}

template<class FuncProcessFile>
void scanDirectory(const FS::path& sPath, FuncProcessFile funcProcessor)
{
	for (const auto& itItem : FS::directory_iterator(sPath))
	{
		if (FS::is_directory(itItem))
		{
			scanDirectory(itItem, funcProcessor);
		}
		else if (FS::is_regular_file(itItem))
		{
			funcProcessor(itItem);
		}
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
	// control by program options
	bool	bNoDLImage;
	bool	bOverWrite;
	bool	bFileIndex;
	bool	bTFileNameTitle;
	bool	bReProcessMode;
	int		iRetryTimes;
	int		iSleep;
	int		iIndexDigitals;
	string	sURL;
	string	sSearch;
	string	sReplace;
	FS::path	sDir;
	FS::path	sLogFile;
	FS::path	sOpenCC_Conf;

	// internal usage
	string		sEncode = "BIG5";
	FS::path	sExtBin = "Binary";
	FS::path	sImage = "images";
	#pragma endregion

	#pragma region Program Options
	{
		namespace BPO = boost::program_options;

		// define program options
		BPO::options_description bpoOptions( "Command Line Options" );
		bpoOptions.add_options()
			( "help,H",				BPO::bool_switch()->notifier( [&bpoOptions]( bool bH ){ if( bH ){ cout << bpoOptions << endl; exit(0); } } ),	"Help message" )
			( "url,U",				BPO::value(&sURL)->value_name("Web_Link"),											"The link of index page." )
			( "output,O",			BPO::value(&sDir)->value_name("output_dir")->default_value("."),					"Directory to save output files" )
			( "retry",				BPO::value(&iRetryTimes)->value_name("times")->default_value(100),					"HTTP retry times")
			( "wait",				BPO::value(&iSleep)->value_name("ms")->default_value(100),							"Sleep between http request")
			( "s2c_conf",			BPO::value(&sOpenCC_Conf)->value_name("conf_file")->default_value(".\\s2t.json"),	"SC to TC convertor configuration file")
			( "log",				BPO::value(&sLogFile)->value_name("log_file"),										"Log")
			( "search",				BPO::value(&sSearch)->value_name("string")->default_value(""),						"Search text in book name, use with --replace")
			( "replace",			BPO::value(&sReplace)->value_name("string")->default_value(""),						"Replace search trem with, use with --search")
			( "filename-as-title",	BPO::bool_switch(&bTFileNameTitle)->default_value(false),							"Use refined file-name as title in metadata")
			( "file_index",			BPO::bool_switch(&bFileIndex)->default_value(false),								"Add file index at the begin of file name")
			( "index_num",			BPO::value(&iIndexDigitals)->value_name("num")->default_value(2),					"Digitals of index (--file_index)")
			( "no_dl_image",		BPO::bool_switch(&bNoDLImage)->default_value(false),								"Not download image" )
			( "overwrite",			BPO::bool_switch(&bOverWrite)->default_value(false),								"Overwrite existed files")
			( "reprocess",			BPO::bool_switch(&bReProcessMode)->default_value(false),							"Reprocess existed HTML files");

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
	backend->add_stream(boost::shared_ptr< std::ostream >(&std::clog, boost::null_deleter()));
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
	// initial OpenCC
	try
	{
		g_pOpenCC = new opencc::SimpleConverter(sOpenCC_Conf.string());
	}
	catch (exception e)
	{
		BOOST_LOG_TRIVIAL(error) << "OpenCC Initialize error. " << e.what();
		return -1;
	}

	// external command refine
	wstring sExtPath = ( FS::current_path() / sExtBin ).wstring();
	g_sCalibre = sExtPath + g_sCalibre;
	g_sKepubify= sExtPath + g_sKepubify;

	// filename refine
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

	if (bReProcessMode)
	{
		BOOST_LOG_TRIVIAL(info) << "Enter re-process mode: " << sDir << "\n";
		scanDirectory(sDir, [](const FS::path& sPath) {
			if (sPath.extension() == ".html")
			{
				BOOST_LOG_TRIVIAL(info) << sPath;
				std::wifstream inputFile(sPath.string());
				if (inputFile)
				{
					std::wstring sHTML((std::istreambuf_iterator<wchar_t>(inputFile)),std::istreambuf_iterator<wchar_t>());
					inputFile.close();

					boost::replace_all(sHTML, "<br />\r\n<br />\r\n", "<BR />\r\n");
					boost::replace_all(sHTML, "&nbsp;&nbsp;&nbsp;&nbsp;", "&emsp;&emsp;");
					boost::replace_all(sHTML, "</A><H3", "<H3");
					boost::replace_all(sHTML, "</H3>", "</H3></A>");

					wstring sCover = L"";
					auto pFirstImage = HTMLParser::FindContentBetweenTag(sHTML, { L"<img src=\"", L"\" "}, 0);
					if (pFirstImage.first != std::wstring::npos)
						sCover = pFirstImage.second;

					std::wofstream outputFile(sPath.string());
					outputFile << sHTML;
					outputFile.close();

					FS::current_path(sPath.parent_path());
					PostProcess(sPath, sCover, sPath.parent_path().filename().wstring());

					BOOST_LOG_TRIVIAL(info) << sPath;
				}
			}
		});
		return 0;
	}

	try
	{
		auto duSleep = std::chrono::milliseconds(iSleep);

		Client mClient;
		BOOST_LOG_TRIVIAL(info) << "Try to open: " << sURL;
		auto mURL = URL(sURL);
		if (mURL)
		{
			Wenku8Cn mSite;
			if (mSite.CheckServer(mURL.m_sHost))
			{
				FS::path pathURL = FS::path(sURL).parent_path();

				int iTime = 0;
				std::optional<wstring> rHtml;
				while (++iTime < iRetryTimes)
				{
					std::this_thread::sleep_for(duSleep);
					rHtml = mClient.ReadHtml(mURL);
					if (rHtml)
					{
						*rHtml = mSite.FilterHTML(*rHtml);
						break;
					}
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
				wstring sBN = ConvertS2T(vBooks.first);
				BOOST_LOG_TRIVIAL(trace) << "Strat to download novel: " << SConv( sBN );
				BOOST_LOG_TRIVIAL(info) << " >Found " << vBooks.second.size() << " books";

				// check directory
				g_sOutPath = sDir / VertifyFilename(sBN);
				if (!FS::exists(g_sOutPath))
					FS::create_directories(g_sOutPath);
				if (!bNoDLImage && !FS::exists(g_sOutPath / sImage))
					FS::create_directories(g_sOutPath / sImage);
				BOOST_LOG_TRIVIAL(trace) << "Output to " << g_sOutPath;

				// write
				FS::current_path(g_sOutPath);

				// Output URL for reference
				{
					ofstream oFile("url.txt");
					oFile << sURL;
					oFile.close();
				}

				// Process book
				int idxBook = 0;
				for (BookIndex& rBook : vBooks.second)
				{
					wstring sBookName = ConvertS2T(rBook.m_sTitle);
					sBookName = funcNameRefine(sBookName);
					if (bFileIndex)
					{
						++idxBook;
						wstring wFmt = L"%0" + boost::lexical_cast<wstring>(iIndexDigitals) + L"d_%s";
						sBookName = (boost::wformat(wFmt) % idxBook % sBookName).str();
					}

					FS::path fnBook = sBookName + L".html";
					BOOST_LOG_TRIVIAL(trace) << " Start process book <" << fnBook << ">, with " << rBook.m_vChapter.size() << " chapters";

					// if file existed
					bool bToDownload = true;
					if (FS::exists(fnBook))
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
							FS::rename(fnBook, fnBook.wstring() + L".bak");
							BOOST_LOG_TRIVIAL(info) << "  Chapter number is not equal, redownload. ( " << iCounter << " != " << rBook.m_vChapter.size() << " )";
						}
					}

					if (bToDownload || bOverWrite)
					{
						wofstream oFile(fnBook.wstring());
						oFile.imbue(g_locUTF8);
						if (oFile.is_open())
						{
							wstring	sTitle = ConvertS2T(rBook.m_sTitle ),
									sAuthor = ConvertS2T(rBook.m_sAuthor );

							#pragma region HTML Header
							oFile << "<HTML>\n";
							oFile << "<HEAD>\n<META http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
							if (bTFileNameTitle)
								oFile << "<TITLE>" << sBookName << "</TITLE>\n";
							else
								oFile << "<TITLE>" << sTitle << "</TITLE>\n";
							oFile << "<META name=\"Author\" content=\"" << sAuthor << "\">\n</HEAD>\n";
							#pragma endregion

							#pragma region begining
							oFile << "<BODY>\n";
							oFile << "<H3 ALIGN=\"CENTER\">" << sTitle << "</H3>\n";
							oFile << "<H4 ALIGN=\"CENTER\">" << sAuthor << "</H4>\n";
							#pragma endregion

							#pragma region index
							oFile << "<SECTION><A NAME=\"INDEX\" ></A><HR>\n<NAV>\n";
							size_t idxChapter = 0;
							for (auto& rLink : rBook.m_vChapter)
							{
								rLink.first = ConvertS2T(rLink.first);
								oFile << "<p><a href=\"#CH" << ++idxChapter << "\">" << rLink.first << "</a><!-- " << rLink.second << " --></p>\n";
							}
							oFile << "</NAV>\n</SECTION>\n";
							#pragma endregion

							#pragma region content
							idxChapter = 0;
							wstring sCover = L"";
							for (auto& rLink : rBook.m_vChapter)
							{
								oFile << "<HR><SECTION><A NAME=\"CH" << ++idxChapter << "\" ><H3 CLASS=\"chapter\">" << rLink.first << "</H3></A>\n";
								BOOST_LOG_TRIVIAL(trace) << "  > " << SConv( rLink.second );

								int iBTime = 0;
								std::optional<wstring> sHTML;
								while (++iBTime < iRetryTimes)
								{
									std::this_thread::sleep_for(duSleep);
									sHTML = mClient.ReadHtml(CheckLink(SConv(rLink.second), pathURL.string()));
									if (sHTML)
									{
										*sHTML = mSite.FilterHTML(*sHTML);
										break;
									}
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
												URL sLink = URL( CheckLink(SConv(rImg.second), pathURL.string()) );
												if (sLink)
												{
													std::string sFile = sLink.getFilename();

													BOOST_LOG_TRIVIAL(info) <<  "        " << sLink.m_sPath;
													FS::path sImagePath = sImage / sFile;

													bool bNeedDownload = true;
													if (FS::exists(sImagePath))
													{
														if (!bOverWrite)
														{
															auto uSize = FS::file_size(sImagePath);
															bNeedDownload = false;
														}
													}

													if (bNeedDownload)
													{
														int iTimes = 0;
														bool bOK = false;
														while (++iTimes < iRetryTimes)
														{
															BOOST_LOG_TRIVIAL(trace) << ".";
															std::this_thread::sleep_for(duSleep);
															bOK = mClient.GetBinaryFile(sLink, sImagePath.wstring());
															if (bOK)
																break;
														}
														if (bOK)
															BOOST_LOG_TRIVIAL(trace) << "OK";
														else
															BOOST_LOG_TRIVIAL(error) << "Image <" << sFile << "> download error.";
													}
													else
														BOOST_LOG_TRIVIAL(trace) << "SKIP";

													sHTML->replace(uShift + rImg.first, rImg.second.size(), sImagePath.wstring());
													uShift += sImagePath.wstring().size() - rImg.second.size();

													if (sCover == L"")
														sCover = sImagePath.wstring();
												}
											}
										}
									}
									oFile << ConvertS2T( mSite.GetChapterContent(*sHTML) );
									oFile << "</SECTION>\n";
								}
								else
								{
									BOOST_LOG_TRIVIAL(error) << "Can't read the page:" << SConv( rLink.second );
								}
							}
							oFile << "</BODY></HTML>\n";
							#pragma endregion

							oFile.close();
							BOOST_LOG_TRIVIAL(trace) << "  Start Convert";
							PostProcess(fnBook, sCover, sBN);
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
