#include "clock_recovery.h"

#include <cctype>
#include <chrono>
#include <fcntl.h>
#include <map>
#include <strings.h>
#include <unistd.h>
#include <vector>
#include<list>

#include <libcamera/base/log.h>

#include "sync_status.h"

using namespace std::chrono_literals;
using namespace RPiController;
using namespace libcamera;

LOG_DEFINE_CATEGORY(RPiClockRec)


float ClockRecovery::slope(){
	//always just add the last two ones, and remove before in adding fucntion
	xsum += client_sequence.back() * 1.0;
	ysum += error_values.back() * 1.0;
	xysum += error_values.back() * client_sequence.back() * 1.0;
	x2sum += client_sequence.back() * client_sequence.back() * 1.0;
	
	double sl = (error_values.size() * xysum - xsum * ysum)/(error_values.size() * x2sum - xsum * xsum) ;
	if(sl){return sl;} else {return 0;}
}

void ClockRecovery::updating_values(std::chrono::microseconds correction){

	std::list<int>::iterator iterror = error_values.begin();
	std::list<int> update;

	for(; iterror != error_values.end(); iterror++){
		update.push_back((*iterror)-correction.count());
 	}
	ysum -= (double)correction.count() * update.size();
	xysum -= correction.count() * xsum;
	error_values = update;
}

std::chrono::microseconds ClockRecovery::trending_error(std::chrono::microseconds LastWallClock, std::chrono::microseconds ClientWallClock, std::chrono::microseconds lastPayloadFrameDuration, unsigned int sequence) {
	//local wall clock - server wall clock, so if we hurry, we get positive, which shortens the frame
	//std::chrono::microseconds lastPayloadFrameDuration = (NextWallClock - LastWallClock) /syncPeriod ;
	std::chrono::microseconds delta = (ClientWallClock) - (LastWallClock);
	unsigned int mul = (delta + lastPayloadFrameDuration / 2) / lastPayloadFrameDuration;
	std::chrono::microseconds delta_mod = delta - mul * lastPayloadFrameDuration;
	
	int y = delta_mod.count();
	int x = sequence;

	if(error_values.size() == listSize){
		xsum -= client_sequence.front() * 1.0;
		ysum -= error_values.front() * 1.0;
		xysum -= error_values.front() * client_sequence.front() * 1.0;
		x2sum -= client_sequence.front() * client_sequence.front() * 1.0;

		error_values.pop_front();
		client_sequence.pop_front();
	}
	error_values.push_back(y);
	client_sequence.push_back(x);
	LOG(RPiClockRec, Info) <<"NEXT "<<y<<","<<x;
	int trending = error_values.front() + slope() * (error_values.size() - 1) * syncPeriod;
	LOG(RPiClockRec, Info) << "eror value trending  "<<trending;

	return std::chrono::microseconds(trending);
}

