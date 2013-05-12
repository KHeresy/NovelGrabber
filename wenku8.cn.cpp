#include "wenku8.cn.h"
#include "HttpClient.h"

// Boost Header
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;

Wenku8Cn::Wenku8Cn()
{
	m_SiteList = boost::assign::list_of( "www.wenku8.cn" );

	m_TitleTag			= make_pair( "<div id=\"title\">",					"</div>" );
	m_AuthorTag			= make_pair( "<div id=\"info\">",					"</div>" );
	m_BookTitleTag		= make_pair( "<td colspan=\"4\" class=\"vcss\">",	"</td>" );
	m_BookChapterTag	= make_pair( "<td class=\"ccss\">",					"</td>" );
	m_ContentTag		= make_pair( "<div id=\"content\">",				"\n</div>" );
	m_ImageTag			= make_pair( "<img src=\"",							"\" border=\"0\" class=\"imagecontent\">" );
}

bool Wenku8Cn::CheckServer( const std::string& rServer )
{
	if( m_SiteList.find( rServer ) != m_SiteList.end() )
		return true;

	return false;
}

std::vector<BookIndex> Wenku8Cn::AnalyzeIndexPage( std::string& rHtmlContent )
{
	string sTitle, sAuthor;

	// find title and author
	auto pTitle = HTMLParser::FindContentBetweenTag( rHtmlContent, m_TitleTag );
	if( pTitle.first != string::npos )
		sTitle	= pTitle.second;

	auto pAuthor = HTMLParser::FindContentBetweenTag( rHtmlContent, m_AuthorTag );
		sAuthor = pAuthor.second;

	// find all books' title
	vector< std::pair<size_t,string> > vBooks;
	size_t uPos = 0;
	while( true )
	{
		auto pBook = HTMLParser::FindContentBetweenTag( rHtmlContent, m_BookTitleTag, uPos );
		if( pBook.first != string::npos )
		{
			vBooks.push_back( pBook );
			uPos = pBook.first + 1;
		}
		else
		{
			break;
		}
	}

	// get chapter link of each book
	vector<BookIndex> vBookLinks;
	for( size_t i = 0; i < vBooks.size(); ++ i )
	{
		// make sure the range of this book
		size_t uPos1 = vBooks[i].first, uPos2;
		if( i == vBooks.size() - 1 )
			uPos2 = rHtmlContent.length();
		else
			uPos2 = vBooks[i+1].first;

		// basic book data
		BookIndex mBook;
		mBook.m_sTitle	= sTitle + " " + vBooks[i].second;
		mBook.m_sAuthor	= sAuthor;
		
		// get all chapter
		while( true )
		{
			auto pLink = HTMLParser::FindContentBetweenTag( rHtmlContent, m_BookChapterTag, uPos1 );
			if( pLink.first == string::npos || pLink.first > uPos2 )
			{
				break;
			}
			else
			{
				if( pLink.second == "&nbsp;" )
					break;

				auto slink = HTMLParser::AnalyzeLink( pLink.second );
				if( slink )
				{
					mBook.m_vChapter.push_back( *slink );
				}
				uPos1 = pLink.first;
			}
		}
		vBookLinks.push_back( mBook );
	}
	return vBookLinks;
}

string Wenku8Cn::GetChapterContent( const string& rHtml )
{
	auto pContent = HTMLParser::FindContentBetweenTag( rHtml, m_ContentTag );
	if( pContent.first != string::npos )
	{
		boost::algorithm::replace_all( pContent.second, "<ul ", "<!--" );
		boost::algorithm::replace_all( pContent.second, "</ul>", " -->" );

		return pContent.second;
	}

	return "";
}

vector< pair<size_t,string> > Wenku8Cn::FindAllImage( const string& rHTML, size_t uPos )
{
	vector< pair<size_t,string> > vRes;
	size_t uStartPos = uPos;
	while( true )
	{
		auto pImg = HTMLParser::FindContentBetweenTag( rHTML, m_ImageTag, uStartPos );
		if( pImg.first == string::npos )
			break;
		else
		{
			vRes.push_back( pImg );
			uStartPos = pImg.first + pImg.second.size() + m_ImageTag.second.size();
		}
	}
	return vRes;
}
