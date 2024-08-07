#include<list>
#include <chrono>
using namespace std::chrono_literals;

namespace RPiController {

class ClockRecovery {
	public:
		//at the moment I think we only follow the difference rather than the exct values of STS and CTS	
		std::list<int> error_values;
		std::list<int> client_sequence;
		//default sync period, magic number a bit...
		unsigned int syncPeriod = 30;
		unsigned int listSize = 100; 
		void updating_values(std::chrono::microseconds delta);
		std::chrono::microseconds trending_error(std::chrono::microseconds LastWallClock, std::chrono::microseconds ClientWallClock, std::chrono::microseconds lastPayloadFrameDuration, unsigned int sequence);
		//initializes the objects and sets sync period to the one of the sync class
		ClockRecovery(unsigned int syncperiod, unsigned int listsize)
		{
			syncPeriod = syncperiod;
			listSize = listsize;
		}
		//default cnstructor
		ClockRecovery(){}
	private:
		float slope();
		double xsum = 0;
		double ysum = 0;
		double xysum = 0;
		double x2sum = 0;
};
}