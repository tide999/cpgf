#ifndef __GEXCEPTION_H
#define __GEXCEPTION_H

#include <stdexcept>
#include <string>


namespace cpgf {

class GException : public std::runtime_error {
private:
	typedef std::runtime_error super;

public:
	GException(int errorCode, const char * message);
	
	int getCode() const;
	const char * getMessage() const;

private:
	int errorCode;	
};

void raiseException(int errorCode, const char * message);
void raiseFormatException(int errorCode, const char * message, ...);



} // namespace cpgf



#endif
