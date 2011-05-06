#ifndef __AsioDrivers__
#define __AsioDrivers__

#include <windows.h>
#include "asiolist.h"

class AsioDrivers : public AsioDriverList
{
public:
	AsioDrivers();
	~AsioDrivers();
	
	bool getCurrentDriverName(TCHAR *name);
	long getDriverNames(TCHAR **names, long maxDrivers);
	bool loadDriver(const TCHAR *name);
	void removeCurrentDriver();
	long getCurrentDriverIndex() {return curIndex;}
protected:
	unsigned long connID;
	long curIndex;
};

#endif
