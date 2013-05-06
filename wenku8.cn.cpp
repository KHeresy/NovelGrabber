#include "wenku8.cn.h"

// Boost Header
#include <boost/assign/list_of.hpp>

bool Wenku8Cn::CheckServer( const std::string& rServer )
{
	static std::set<std::string> setSiteList = boost::assign::list_of( "www.wenku8.cn" );
	if( setSiteList.find( rServer ) != setSiteList.end() )
		return true;

	return false;
}

void Wenku8Cn::AnalyzeIndexPage( std::string& rHtmlContent )
{
}
