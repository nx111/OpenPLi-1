#include <lib/base/estring.h>
#include <ctype.h>
#include <limits.h>
#include <lib/system/elock.h>

///////////////////////////////////////// eString sprintf /////////////////////////////////////////////////
eString& eString::sprintf(char *fmt, ...)
{
// Implements the normal sprintf method, to use format strings with eString
// The max length of the result string is 1024 char.
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	std::vsnprintf(buf, 1024, fmt, ap);
	va_end(ap);
	assign(buf);
	return *this;
}

/////////////////////////////////////// eString upper() ////////////////////////////////////////////////
eString& eString::upper()
{
//	convert all lowercase characters to uppercase, and returns a reference to itself
	for (iterator it = begin(); it != end(); it++)
		switch(*it)
		{
			case 'a' ... 'z' :
				*it -= 32;
			break;

			case '\xe4' : 		//ä
				*it = '\xc4';	//Ä
			break;
			
			case '\xfc' :		//ü
				*it = '\xdc';	//Ü
			break;
			
			case '\xf6' :		//ö
				*it = '\xd6';	//Ö
			break;
		}

	return *this;
}

eString& eString::strReplace(const char* fstr, const eString& rstr,int encode)
{
//	replace all occurrence of fstr with rstr and, and returns a reference to itself
	unsigned int index=0;
	unsigned int fstrlen = strlen(fstr);
	int rstrlen=rstr.size();

	switch(encode){
	case UTF8_ENCODING:
		while(index<length()){
			if( (fstrlen+index)<=length() && !strcmp(mid(index,fstrlen).c_str(),fstr) ){
				replace(index,fstrlen,rstr);
				index+=rstrlen;
				continue;
			}
			if((at(index) & 0xE0)==0xC0)
				index+=2;
			else
			if((at(index) & 0xF0)==0xE0)
				index+=3;
			else
			if((at(index) & 0xF8)==0xF0)
				index+=4;
			else 
				index++;
		}
		break;
	default:
		while ( ( index = find(fstr, index) ) != npos )
		{
			replace(index, fstrlen, rstr);
			index+=rstrlen;
		}
		break;
	}
	return *this;
}


