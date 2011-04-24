#ifndef __AsioDrivers__
#define __AsioDrivers__

#include <windows.h>
#include "asiolist.h"

class AsioDrivers : public AsioDriverList
{
public:
	AsioDrivers();
	~AsioDrivers();
	
	bool getCurrentDriverName(char *name);
	long getDriverNames(char **names, long maxDrivers);
	bool loadDriver(const char *name);
	void removeCurrentDriver();
	long getCurrentDriverIndex() {return curIndex;}
protected:
	unsigned long connID;
	long curIndex;
};

#endif
